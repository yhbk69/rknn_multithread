#include <stdio.h>
#include <mutex>
#include "rknn_api.h"

#include "postprocess.h"
#include "preprocess.h"

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include "coreNum.hpp"
#include "rkYolov5s.hpp"

/**
 * 打印张量属性信息(调试用)
 * 包含张量的维度、形状、数据类型、量化参数等
 */
static void dump_tensor_attr(rknn_tensor_attr *attr)
{
    std::string shape_str = attr->n_dims < 1 ? "" : std::to_string(attr->dims[0]);
    for (int i = 1; i < attr->n_dims; ++i)
    {
        shape_str += ", " + std::to_string(attr->dims[i]);
    }
    // 调试输出已注释，需要时可打开
}

/**
 * 从文件中读取指定偏移和大小的数据
 * @param fp 文件指针
 * @param ofst 偏移量
 * @param sz 读取字节数
 * @return 读取的数据指针，失败返回NULL
 */
static unsigned char *load_data(FILE *fp, size_t ofst, size_t sz)
{
    unsigned char *data;
    int ret;

    data = NULL;

    if (NULL == fp)
    {
        return NULL;
    }

    // 移动文件指针到指定偏移
    ret = fseek(fp, ofst, SEEK_SET);
    if (ret != 0)
    {
        printf("blob seek failure.\n");
        return NULL;
    }

    // 分配内存并读取数据
    data = (unsigned char *)malloc(sz);
    if (data == NULL)
    {
        printf("buffer malloc failure.\n");
        return NULL;
    }
    ret = fread(data, 1, sz, fp);
    return data;
}

/**
 * 加载整个模型文件到内存
 * @param filename 模型文件路径
 * @param model_size 输出参数，模型文件大小
 * @return 模型数据指针，失败返回NULL
 */
static unsigned char *load_model(const char *filename, int *model_size)
{
    FILE *fp;
    unsigned char *data;

    fp = fopen(filename, "rb");
    if (NULL == fp)
    {
        printf("Open file %s failed.\n", filename);
        return NULL;
    }

    // 获取文件大小
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);

    // 读取整个模型文件
    data = load_data(fp, 0, size);

    fclose(fp);

    *model_size = size;
    return data;
}

/**
 * 保存浮点数组到文本文件(调试用)
 */
static int saveFloat(const char *file_name, float *output, int element_size)
{
    FILE *fp;
    fp = fopen(file_name, "w");
    for (int i = 0; i < element_size; i++)
    {
        fprintf(fp, "%.6f\n", output[i]);
    }
    fclose(fp);
    return 0;
}

/**
 * 构造函数: 初始化模型路径和默认阈值
 */
rkYolov5s::rkYolov5s(const std::string &model_path)
{
    this->model_path = model_path;
    nms_threshold = NMS_THRESH;      // NMS(非极大值抑制)阈值，用于去除重叠框
    box_conf_threshold = BOX_THRESH; // 置信度阈值，低于此值的检测框会被过滤
}

/**
 * 初始化RKNN模型
 * @param ctx_in 主模型上下文指针，用于共享模型参数
 * @param share_weight 是否共享模型参数(第一个实例为false，后续实例为true)
 * @return 0成功，-1失败
 */
int rkYolov5s::init(rknn_context *ctx_in, bool share_weight)
{
    printf("Loading model...\n");

    // 从文件加载模型到内存
    int model_data_size = 0;
    model_data = load_model(model_path.c_str(), &model_data_size);

    // 如果是第一个模型实例，则初始化新上下文；否则复用已有模型的参数
    // rknn_dup_context可以共享模型权重，节省内存
    if (share_weight == true)
        ret = rknn_dup_context(ctx_in, &ctx);
    else
        ret = rknn_init(&ctx, model_data, model_data_size, 0, NULL);

    if (ret < 0)
    {
        printf("rknn_init error ret=%d\n", ret);
        return -1;
    }

    // 设置模型绑定的NPU核心
    // RK3588有3个NPU核心，通过轮询分配实现负载均衡
    rknn_core_mask core_mask;
    switch (get_core_num())
    {
    case 0:
        core_mask = RKNN_NPU_CORE_0;
        break;
    case 1:
        core_mask = RKNN_NPU_CORE_1;
        break;
    case 2:
        core_mask = RKNN_NPU_CORE_2;
        break;
    }
    ret = rknn_set_core_mask(ctx, core_mask);
    if (ret < 0)
    {
        printf("rknn_init core error ret=%d\n", ret);
        return -1;
    }

    // 查询RKNN SDK版本信息
    rknn_sdk_version version;
    ret = rknn_query(ctx, RKNN_QUERY_SDK_VERSION, &version, sizeof(rknn_sdk_version));
    if (ret < 0)
    {
        printf("rknn_init error ret=%d\n", ret);
        return -1;
    }
    printf("sdk version: %s driver version: %s\n", version.api_version, version.drv_version);

    // 获取模型的输入输出张量数量
    ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret < 0)
    {
        printf("rknn_init error ret=%d\n", ret);
        return -1;
    }
    printf("model input num: %d, output num: %d\n", io_num.n_input, io_num.n_output);

    // 查询并设置输入张量属性
    // 获取每个输入张量的维度、数据类型、格式等信息
    input_attrs = (rknn_tensor_attr *)calloc(io_num.n_input, sizeof(rknn_tensor_attr));
    for (int i = 0; i < io_num.n_input; i++)
    {
        input_attrs[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &(input_attrs[i]), sizeof(rknn_tensor_attr));
        if (ret < 0)
        {
            printf("rknn_init error ret=%d\n", ret);
            return -1;
        }
        dump_tensor_attr(&(input_attrs[i]));
    }

    // 查询并设置输出张量属性
    // YOLOv5s有3个输出，分别对应3个不同尺度的特征图
    output_attrs = (rknn_tensor_attr *)calloc(io_num.n_output, sizeof(rknn_tensor_attr));
    for (int i = 0; i < io_num.n_output; i++)
    {
        output_attrs[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &(output_attrs[i]), sizeof(rknn_tensor_attr));
        dump_tensor_attr(&(output_attrs[i]));
    }

    // 解析模型输入张量的格式和尺寸
    // 支持NCHW和NHWC两种格式
    if (input_attrs[0].fmt == RKNN_TENSOR_NCHW)
    {
        printf("model is NCHW input fmt\n");
        channel = input_attrs[0].dims[1];
        height = input_attrs[0].dims[2];
        width = input_attrs[0].dims[3];
    }
    else
    {
        printf("model is NHWC input fmt\n");
        height = input_attrs[0].dims[1];
        width = input_attrs[0].dims[2];
        channel = input_attrs[0].dims[3];
    }
    printf("model input height=%d, width=%d, channel=%d\n", height, width, channel);

    // 配置输入张量参数
    memset(inputs, 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = RKNN_TENSOR_UINT8;  // 输入数据类型为8位无符号整数
    inputs[0].size = width * height * channel;
    inputs[0].fmt = RKNN_TENSOR_NHWC;    // 输入格式为NHWC(高宽通道)
    inputs[0].pass_through = 0;          // 不进行直通模式

    return 0;
}

/**
 * 获取模型上下文指针，用于子模型共享参数
 */
rknn_context *rkYolov5s::get_pctx()
{
    return &ctx;
}

/**
 * 执行目标检测推理
 * @param orig_img 原始输入图像(BGR格式)
 * @return 绘制了检测框的图像
 */
cv::Mat rkYolov5s::infer(cv::Mat &orig_img)
{
    // 加锁保证线程安全，防止多个线程同时操作同一个模型实例
    std::lock_guard<std::mutex> lock(mtx);

    // BGR转RGB(RKNN模型期望RGB输入)
    cv::Mat img;
    cv::cvtColor(orig_img, cv::COLOR_BGR2RGB);
    img_width = img.cols;
    img_height = img.rows;

    // 用于存储letterbox的填充信息
    BOX_RECT pads;
    memset(&pads, 0, sizeof(BOX_RECT));
    cv::Size target_size(width, height);
    cv::Mat resized_img(target_size.height, target_size.width, CV_8UC3);

    // 计算图像缩放比例
    float scale_w = (float)target_size.width / img.cols;
    float scale_h = (float)target_size.height / img.rows;

    // 如果输入图像尺寸与模型期望尺寸不同，则需要缩放
    if (img_width != width || img_height != height)
    {
        // 使用RGA硬件加速图像缩放(比OpenCV快很多)
        rga_buffer_t src;
        rga_buffer_t dst;
        memset(&src, 0, sizeof(src));
        memset(&dst, 0, sizeof(dst));
        ret = resize_rga(src, dst, img, resized_img, target_size);
        if (ret != 0)
        {
            fprintf(stderr, "resize with rga error\n");
        }

        /*********
        // 备选方案: 使用OpenCV进行letterbox缩放(保持宽高比，填充黑边)
        // 当前使用RGA方案，速度更快
        float min_scale = std::min(scale_w, scale_h);
        scale_w = min_scale;
        scale_h = min_scale;
        letterbox(img, resized_img, pads, min_scale, target_size);
        *********/
        inputs[0].buf = resized_img.data;
    }
    else
    {
        // 图像尺寸匹配，直接使用原始图像数据
        inputs[0].buf = img.data;
    }

    // 设置模型输入数据
    rknn_inputs_set(ctx, io_num.n_input, inputs);

    // 准备输出缓冲区
    rknn_output outputs[io_num.n_output];
    memset(outputs, 0, sizeof(outputs));
    for (int i = 0; i < io_num.n_output; i++)
    {
        outputs[i].want_float = 0; // 输出为INT8量化数据，非浮点数
    }

    // 执行模型推理
    ret = rknn_run(ctx, NULL);
    // 获取推理输出结果
    ret = rknn_outputs_get(ctx, io_num.n_output, outputs, NULL);

    // 后处理: 解码检测框、NMS过滤
    detect_result_group_t detect_result_group;
    std::vector<float> out_scales;
    std::vector<int32_t> out_zps;
    // 收集输出张量的量化参数(缩放因子和零点)
    for (int i = 0; i < io_num.n_output; ++i)
    {
        out_scales.push_back(output_attrs[i].scale);
        out_zps.push_back(output_attrs[i].zp);
    }

    // 执行后处理: 解码边界框、置信度过滤、NMS去重
    post_process((int8_t *)outputs[0].buf, (int8_t *)outputs[1].buf, (int8_t *)outputs[2].buf, height, width,
                 box_conf_threshold, nms_threshold, pads, scale_w, scale_h, out_zps, out_scales, &detect_result_group);

    // 在原始图像上绘制检测框和标签
    char text[256];
    for (int i = 0; i < detect_result_group.count; i++)
    {
        detect_result_t *det_result = &(detect_result_group.results[i]);
        sprintf(text, "%s %.1f%%", det_result->name, det_result->prop * 100);

        int x1 = det_result->box.left;
        int y1 = det_result->box.top;
        int x2 = det_result->box.right;
        int y2 = det_result->box.bottom;

        // 绘制蓝色矩形框
        rectangle(orig_img, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(256, 0, 0, 256), 3);
        // 绘制类别名称和置信度
        putText(orig_img, text, cv::Point(x1, y1 + 12), cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255));
    }

    // 释放输出缓冲区
    ret = rknn_outputs_release(ctx, io_num.n_output, outputs);

    return orig_img;
}

/**
 * 析构函数: 释放模型资源
 */
rkYolov5s::~rkYolov5s()
{
    // 释放后处理分配的标签内存
    deinitPostProcess();

    // 销毁RKNN模型上下文
    ret = rknn_destroy(ctx);

    // 释放模型数据内存
    if (model_data)
        free(model_data);

    // 释放输入输出属性内存
    if (input_attrs)
        free(input_attrs);
    if (output_attrs)
        free(output_attrs);
}
