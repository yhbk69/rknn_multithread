#include <stdio.h>
#include <mutex>
#include "rknn_api.h"

#include "postprocess.h"
#include "preprocess.h"
#include "config_loader.hpp"

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
    size_t read_bytes = fread(data, 1, sz, fp);
    if (read_bytes != sz)
    {
        printf("blob read failure: expected %zu, got %zu\n", sz, read_bytes);
        free(data);
        return NULL;
    }
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
    if (fp == NULL)
    {
        printf("saveFloat: cannot open %s\n", file_name);
        return -1;
    }
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
    // 从配置加载阈值
    AppConfig cfg = load_config();
    nms_threshold = cfg.nms_threshold;
    box_conf_threshold = cfg.box_threshold;
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

    // 初始化后处理上下文（加载标签和 anchor）
    if (post_ctx_.init(model_path.c_str()) < 0)
    {
        printf("Failed to init postprocess context\n");
        return -1;
    }

    // 从文件加载模型到内存
    int model_data_size = 0;
    model_data = load_model(model_path.c_str(), &model_data_size);
    if (model_data == nullptr)
    {
        printf("Failed to load model: %s\n", model_path.c_str());
        return -1;
    }

    // 如果是第一个模型实例，则初始化新上下文；否则复用已有模型的参数
    // rknn_dup_context可以共享模型权重，节省内存
    if (share_weight == true)
        ret = rknn_dup_context(ctx_in, &ctx);
    else
        ret = rknn_init(&ctx, model_data, model_data_size, 0, NULL);

    if (ret < 0)
    {
        printf("rknn_init error ret=%d\n", ret);
        free(model_data);
        model_data = nullptr;
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
        free(model_data);
        model_data = nullptr;
        return -1;
    }

    // 查询RKNN SDK版本信息
    rknn_sdk_version version;
    ret = rknn_query(ctx, RKNN_QUERY_SDK_VERSION, &version, sizeof(rknn_sdk_version));
    if (ret < 0)
    {
        printf("rknn_init error ret=%d\n", ret);
        free(model_data);
        model_data = nullptr;
        return -1;
    }
    printf("sdk version: %s driver version: %s\n", version.api_version, version.drv_version);

    // 获取模型的输入输出张量数量
    ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret < 0)
    {
        printf("rknn_init error ret=%d\n", ret);
        free(model_data);
        model_data = nullptr;
        return -1;
    }
    printf("model input num: %d, output num: %d\n", io_num.n_input, io_num.n_output);

    // 查询并设置输入张量属性
    input_attrs.resize(io_num.n_input);
    for (int i = 0; i < io_num.n_input; i++)
    {
        input_attrs[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &(input_attrs[i]), sizeof(rknn_tensor_attr));
        if (ret < 0)
        {
            printf("rknn_init error ret=%d\n", ret);
            free(model_data);
            model_data = nullptr;
            return -1;
        }
        dump_tensor_attr(&(input_attrs[i]));
    }

    // 查询并设置输出张量属性
    output_attrs.resize(io_num.n_output);
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
 * @param orig_img   原始输入图像(BGR格式)
 * @param out_group  [可选] 输出原始检测结果组，传 NULL 时不输出
 * @return 绘制了检测框的图像
 */
cv::Mat rkYolov5s::infer(cv::Mat &orig_img, detect_result_group_t *out_group)
{
    // 仅锁读取共享阈值，推理过程不需持锁
    float conf, nms;
    {
        std::lock_guard<std::mutex> lock(mtx);
        conf = box_conf_threshold;
        nms = nms_threshold;
    }

    cv::Mat img;
    cv::cvtColor(orig_img, img, cv::COLOR_BGR2RGB);
    int img_width = img.cols;
    int img_height = img.rows;

    BOX_RECT pads;
    memset(&pads, 0, sizeof(BOX_RECT));
    cv::Size target_size(width, height);
    cv::Mat resized_img(target_size.height, target_size.width, CV_8UC3);

    float scale_w = (float)target_size.width / img.cols;
    float scale_h = (float)target_size.height / img.rows;

    if (img_width != width || img_height != height)
    {
        float min_scale = std::min(scale_w, scale_h);
        scale_w = min_scale;
        scale_h = min_scale;
        letterbox(img, resized_img, pads, min_scale, target_size);
        inputs[0].buf = resized_img.data;
    }
    else
    {
        scale_w = 1.0f;
        scale_h = 1.0f;
        memset(&pads, 0, sizeof(pads));
        inputs[0].buf = img.data;
    }

    ret = rknn_inputs_set(ctx, io_num.n_input, inputs);
    if (ret < 0)
    {
        fprintf(stderr, "[rkYolov5s] rknn_inputs_set failed, ret=%d\n", ret);
        return orig_img;
    }

    rknn_output outputs[io_num.n_output];
    memset(outputs, 0, sizeof(outputs));
    for (int i = 0; i < io_num.n_output; i++)
        outputs[i].want_float = 0;

    int max_retry = 3;
    bool infer_ok = false;
    for (int retry = 0; retry < max_retry; retry++)
    {
        ret = rknn_run(ctx, NULL);
        if (ret < 0)
        {
            fprintf(stderr, "[rkYolov5s] rknn_run failed (retry %d/%d), ret=%d\n",
                    retry + 1, max_retry, ret);
            continue;
        }
        ret = rknn_outputs_get(ctx, io_num.n_output, outputs, NULL);
        if (ret < 0)
        {
            fprintf(stderr, "[rkYolov5s] rknn_outputs_get failed (retry %d/%d), ret=%d\n",
                    retry + 1, max_retry, ret);
            continue;
        }
        infer_ok = true;
        break;
    }

    if (!infer_ok)
    {
        fprintf(stderr, "[rkYolov5s] inference failed after %d retries, skipping frame\n", max_retry);
        return orig_img;
    }

    detect_result_group_t detect_result_group;
    std::vector<float> out_scales;
    std::vector<int32_t> out_zps;
    for (int i = 0; i < io_num.n_output; ++i)
    {
        out_scales.push_back(output_attrs[i].scale);
        out_zps.push_back(output_attrs[i].zp);
    }
    post_ctx_.process((int8_t *)outputs[0].buf, (int8_t *)outputs[1].buf, (int8_t *)outputs[2].buf,
                 height, width, conf, nms, pads, scale_w, scale_h,
                 out_zps, out_scales, &detect_result_group);

    char text[256];
    for (int i = 0; i < detect_result_group.count; i++)
    {
        detect_result_t *det_result = &(detect_result_group.results[i]);
        snprintf(text, sizeof(text), "%s %.1f%%", det_result->name, det_result->prop * 100);
        rectangle(orig_img, cv::Point(det_result->box.left, det_result->box.top),
                  cv::Point(det_result->box.right, det_result->box.bottom), cv::Scalar(255, 0, 0), 3);
        putText(orig_img, text, cv::Point(det_result->box.left, det_result->box.top + 12),
                cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255));
    }

    if (out_group)
        *out_group = detect_result_group;

    // 仅写回检测结果时持锁
    {
        std::lock_guard<std::mutex> lock(mtx);
        last_detect_result_ = detect_result_group;
    }

    ret = rknn_outputs_release(ctx, io_num.n_output, outputs);
    if (ret < 0)
        fprintf(stderr, "[rkYolov5s] rknn_outputs_release failed, ret=%d\n", ret);

    return orig_img;
}

/**
 * 析构函数: 释放模型资源
 */
rkYolov5s::~rkYolov5s()
{
    // post_ctx_ / input_attrs / output_attrs 自动 RAII 析构

    ret = rknn_destroy(ctx);

    if (model_data)
        free(model_data);
}
