#include <stdio.h>
#include <mutex>
#include "rknn_api.h"

#include "postprocess.h"
#include "preprocess.h"
#include "config_loader.hpp"

#include <future>
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include "coreNum.hpp"
#include "rkYolov5s.hpp"

// P1: 使用彩色日志系统
#include "Logger.hpp"

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
 * 从文件读取数据块到 vector（RAII 自动释放内存）
 * @param fp 已打开的文件指针
 * @param ofst 文件偏移量
 * @param sz 要读取的字节数
 * @return 数据 vector，失败返回空 vector
 */
static std::vector<unsigned char> load_data(FILE *fp, size_t ofst, size_t sz)
{
    if (NULL == fp)
        return {};

    int ret = fseek(fp, ofst, SEEK_SET);
    if (ret != 0)
    {
        LOG_ERROR("[rkYolov5s]", "blob seek failure");
        return {};
    }

    std::vector<unsigned char> data(sz);
    size_t read_bytes = fread(data.data(), 1, sz, fp);
    if (read_bytes != sz)
    {
        LOG_ERROR("[rkYolov5s]", "blob read failure: expected %zu, got %zu", sz, read_bytes);
        return {};
    }
    return data;
}

/**
 * 加载整个模型文件到 vector（RAII 自动释放内存）
 * @param filename 模型文件路径
 * @return 模型数据 vector，失败返回空 vector
 */
static std::vector<unsigned char> load_model(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (NULL == fp)
    {
        LOG_ERROR("[rkYolov5s]", "Open file %s failed", filename);
        return {};
    }

    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);

    auto data = load_data(fp, 0, size);
    fclose(fp);
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
YOLOv5Engine::YOLOv5Engine(const std::string &model_path)
{
    this->model_path = model_path;
    // 从配置加载阈值
    AppConfig cfg = load_config();
    nms_threshold = cfg.nms_threshold;
    box_conf_threshold = cfg.box_threshold;
}

// ---- IEngine 接口实现 ----

/**
 * IEngine::init — 通用初始化（无参，用于非 rknn 场景或 mock 测试）
 */
int YOLOv5Engine::init() {
    return rknn_init(nullptr, false, -1);
}

/**
 * IEngine::detect — 执行检测并返回结果
 */
int YOLOv5Engine::detect(const cv::Mat &frame, detect_result_group_t *out) {
    cv::Mat mutable_frame = frame.clone();
    infer(mutable_frame, out);
    return 0;
}

/**
 * IEngine::setThresholds — 动态设置阈值
 */
void YOLOv5Engine::setThresholds(float conf, float nms) {
    std::lock_guard<std::mutex> lock(mtx);
    box_conf_threshold = conf;
    nms_threshold = nms;
}

// ---- rknn 特有初始化 ----

/**
 * rknn_init — RKNN 模型初始化（支持上下文共享和核心绑定）
 * @param ctx_in 主模型上下文指针，用于共享模型参数
 * @param share_weight 是否共享模型参数(第一个实例为false，后续实例为true)
 * @param core_num 指定绑定的 NPU 核心编号（-1 使用全局轮询）
 * @return 0成功，-1失败
 */
int YOLOv5Engine::rknn_init(rknn_context *ctx_in, bool share_weight, int core_num)
{
    LOG_INFO("[YOLOv5Engine]", "Loading model...");

    // 初始化后处理上下文（加载标签和 anchor）
    if (post_ctx_.init(model_path.c_str()) < 0)
    {
        LOG_ERROR("[YOLOv5Engine]", "Failed to init postprocess context");
        return -1;
    }

    // 从文件加载模型到内存（RAII vector 自动管理）
    model_data = load_model(model_path.c_str());
    if (model_data.empty())
    {
        LOG_ERROR("[YOLOv5Engine]", "Failed to load model: %s", model_path.c_str());
        return -1;
    }

    // 如果是第一个模型实例，则初始化新上下文；否则复用已有模型的参数
    // rknn_dup_context可以共享模型权重，节省内存
    if (share_weight == true)
        ret = rknn_dup_context(ctx_in, &ctx);
    else
        ret = ::rknn_init(&ctx, model_data.data(), model_data.size(), 0, NULL);

    if (ret < 0)
    {
        LOG_ERROR("[YOLOv5Engine]", "rknn_init error ret=%d", ret);
        return -1;
    }

    // 设置模型绑定的NPU核心
    // RK3588有3个NPU核心，支持按通道固定分配（P2-1）或全局轮询
    rknn_core_mask core_mask;
    int core_id = (core_num >= 0) ? core_num : get_core_num();
    // P0 修复: 添加 default 分支防止 core_id >= 3 时未定义行为
    switch (core_id)
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
    default:
        LOG_ERROR("[YOLOv5Engine]", "Invalid core_id: %d, using Core 0", core_id);
        core_mask = RKNN_NPU_CORE_0;
        break;
    }
    ret = rknn_set_core_mask(ctx, core_mask);
    if (ret < 0)
    {
        LOG_ERROR("[YOLOv5Engine]", "rknn_init core error ret=%d", ret);
        return -1;
    }

    // 查询RKNN SDK版本信息
    rknn_sdk_version version;
    ret = rknn_query(ctx, RKNN_QUERY_SDK_VERSION, &version, sizeof(rknn_sdk_version));
    if (ret < 0)
    {
        LOG_ERROR("[YOLOv5Engine]", "rknn_init error ret=%d", ret);
        return -1;
    }
    LOG_INFO("[YOLOv5Engine]", "sdk version: %s driver version: %s", version.api_version, version.drv_version);

    // 获取模型的输入输出张量数量
    ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret < 0)
    {
        LOG_ERROR("[YOLOv5Engine]", "rknn_init error ret=%d", ret);
        return -1;
    }
    LOG_INFO("[YOLOv5Engine]", "model input num: %d, output num: %d", io_num.n_input, io_num.n_output);

    // 查询并设置输入张量属性
    input_attrs.resize(io_num.n_input);
    for (int i = 0; i < io_num.n_input; i++)
    {
        input_attrs[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &(input_attrs[i]), sizeof(rknn_tensor_attr));
        if (ret < 0)
        {
            LOG_ERROR("[YOLOv5Engine]", "rknn_init error ret=%d", ret);
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

    // 预计算输出量化参数，避免每帧重复构造
    out_scales_.resize(io_num.n_output);
    out_zps_.resize(io_num.n_output);
    for (int i = 0; i < io_num.n_output; ++i)
    {
        out_scales_[i] = output_attrs[i].scale;
        out_zps_[i] = output_attrs[i].zp;
    }

    // 解析模型输入张量的格式和尺寸
    // 支持NCHW和NHWC两种格式
    if (input_attrs[0].fmt == RKNN_TENSOR_NCHW)
    {
        LOG_INFO("[YOLOv5Engine]", "model is NCHW input fmt");
        channel = input_attrs[0].dims[1];
        height = input_attrs[0].dims[2];
        width = input_attrs[0].dims[3];
    }
    else
    {
        LOG_INFO("[YOLOv5Engine]", "model is NHWC input fmt");
        height = input_attrs[0].dims[1];
        width = input_attrs[0].dims[2];
        channel = input_attrs[0].dims[3];
    }
    LOG_INFO("[YOLOv5Engine]", "model input height=%d, width=%d, channel=%d", height, width, channel);

    // 配置输入张量参数
    memset(inputs, 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = RKNN_TENSOR_UINT8;  // 输入数据类型为8位无符号整数
    inputs[0].size = width * height * channel;
    inputs[0].fmt = RKNN_TENSOR_NHWC;    // 输入格式为NHWC(高宽通道)
    inputs[0].pass_through = 0;          // 不进行直通模式

    // 预分配推理输入图像，避免每帧重新分配
    resized_img_ = cv::Mat(height, width, CV_8UC3);

    return 0;
}

/**
 * 获取模型上下文指针，用于子模型共享参数
 */
rknn_context *YOLOv5Engine::get_pctx()
{
    return &ctx;
}

// P2: 使用 RGA 硬件加速预处理
#include "RgaAccelerator.hpp"

/**
 * P2-3: 三级流水线 - Stage P (CPU + RGA)
 * BGR→RGB 转换 + letterbox 缩放填充（RGA 硬件加速）
 * 优化：减少中间 Mat 拷贝，使用 move 语义
 */
static void stage_preprocess(const cv::Mat &orig_img, YOLOv5Engine::PipelineData &data, int model_w, int model_h)
{
    int img_w = orig_img.cols;
    int img_h = orig_img.rows;
    cv::Size target_size(model_w, model_h);
    data.scale_w = (float)target_size.width / img_w;
    data.scale_h = (float)target_size.height / img_h;

    if (img_w != model_w || img_h != model_h)
    {
        float min_scale = std::min(data.scale_w, data.scale_h);
        data.scale_w = min_scale;
        data.scale_h = min_scale;
        memset(&data.pads, 0, sizeof(BOX_RECT));

        // P2: 尝试 RGA 硬件加速
        if (RgaAccelerator::letterboxRga(orig_img, data.padded_img, data.pads, model_w, model_h) == 0) {
            // RGA 成功，rgb_img 不需要（letterboxRga 已包含 BGR→RGB）
            data.rgb_img = data.padded_img;  // 复用，避免额外拷贝
        } else {
            // RGA 失败，回退 CPU
            cv::cvtColor(orig_img, data.rgb_img, cv::COLOR_BGR2RGB);
            letterbox(data.rgb_img, data.padded_img, data.pads, min_scale, target_size);
        }
    }
    else
    {
        data.scale_w = 1.0f;
        data.scale_h = 1.0f;
        memset(&data.pads, 0, sizeof(BOX_RECT));

        // P2: 使用 RGA 进行 BGR→RGB
        if (RgaAccelerator::cvtColorRga(orig_img, data.rgb_img) != 0) {
            cv::cvtColor(orig_img, data.rgb_img, cv::COLOR_BGR2RGB);
        }
    }
}

/**
 * 执行目标检测推理（三级流水线：P 预处理 → I NPU 推理 → O 后处理绘制）
 * @param orig_img   原始输入图像(BGR格式)
 * @param out_group  [可选] 输出原始检测结果组，传 NULL 时不输出
 * @return 绘制了检测框的图像
 */
cv::Mat YOLOv5Engine::infer(cv::Mat &orig_img, detect_result_group_t *out_group)
{
    // 仅锁读取共享阈值，推理过程不需持锁
    float conf, nms;
    {
        std::lock_guard<std::mutex> lock(mtx);
        conf = box_conf_threshold;
        nms = nms_threshold;
    }

    // Stage P (CPU) — 预处理（栈上分配，线程安全）
    PipelineData data;
    stage_preprocess(orig_img, data, width, height);

    // Stage I (NPU) — 推理
    inputs[0].buf = data.padded_img.empty() ? data.rgb_img.data : data.padded_img.data;

    ret = rknn_inputs_set(ctx, io_num.n_input, inputs);
    if (ret < 0)
    {
        LOG_ERROR("[YOLOv5Engine]", "rknn_inputs_set failed, ret=%d", ret);
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
            LOG_ERROR("[YOLOv5Engine]", "rknn_run failed (retry %d/%d), ret=%d",
                    retry + 1, max_retry, ret);
            continue;
        }
        ret = rknn_outputs_get(ctx, io_num.n_output, outputs, NULL);
        if (ret < 0)
        {
            LOG_ERROR("[YOLOv5Engine]", "rknn_outputs_get failed (retry %d/%d), ret=%d",
                    retry + 1, max_retry, ret);
            continue;
        }
        infer_ok = true;
        break;
    }

    if (!infer_ok)
    {
        LOG_ERROR("[YOLOv5Engine]", "inference failed after %d retries, skipping frame", max_retry);
        return orig_img;
    }

    // P2-3: Stage O (CPU) — 后处理 + 绘制
    detect_result_group_t detect_result_group;
    post_ctx_.process((int8_t *)outputs[0].buf, (int8_t *)outputs[1].buf, (int8_t *)outputs[2].buf,
                 height, width, conf, nms, data.pads, data.scale_w, data.scale_h,
                 out_zps_, out_scales_, &detect_result_group);

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
        LOG_ERROR("[YOLOv5Engine]", "rknn_outputs_release failed, ret=%d", ret);

    return orig_img;
}

/**
 * 析构函数: 释放模型资源
 */
YOLOv5Engine::~YOLOv5Engine()
{
    // post_ctx_ / input_attrs / output_attrs / model_data 自动 RAII 析构
    rknn_destroy(ctx);
}
