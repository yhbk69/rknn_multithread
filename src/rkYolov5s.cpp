/*
 * rkYolov5s.cpp - YOLOv5s RKNN 推理引擎实现
 *
 * 三级流水线：
 *   Stage P (CPU) — 预处理：BGR→RGB + letterbox 缩放填充
 *   Stage I (NPU) — 推理：rknn_inputs_set → rknn_run → rknn_outputs_get
 *   Stage O (CPU) — 后处理：三尺度特征图解码 + NMS + 绘制检测框
 *
 * NPU 核心绑定：
 *   - RK3588 有 3 个 NPU 核心
 *   - 每个模型实例绑定一个核心（避免核心争抢）
 *   - 支持权重共享：第一个实例完整加载，后续实例复用权重
 */

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
 * 打印张量属性信息（调试用）
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
 * @param fp   已打开的文件指针
 * @param ofst 文件偏移量
 * @param sz   要读取的字节数
 * @return 数据 vector，失败返回空 vector
 */
static std::vector<unsigned char> load_data(FILE *fp, size_t ofst, size_t sz)
{
    // 检查文件指针是否有效
    if (NULL == fp)
        return {};

    // 定位到指定偏移量
    int ret = fseek(fp, ofst, SEEK_SET);
    if (ret != 0)
    {
        LOG_ERROR("[rkYolov5s]", "blob seek failure");
        return {};
    }

    // 读取数据到 vector（自动管理内存）
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
    // 打开模型文件
    FILE *fp = fopen(filename, "rb");
    if (NULL == fp)
    {
        LOG_ERROR("[rkYolov5s]", "Open file %s failed", filename);
        return {};
    }

    // 获取文件大小
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);

    // 读取整个文件
    auto data = load_data(fp, 0, size);
    fclose(fp);
    return data;
}

/**
 * 保存浮点数组到文本文件（调试用）
 * 用于保存 NPU 输出的量化前数据，方便离线分析
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
 * 构造函数：初始化模型路径和默认阈值
 * @param model_path RKNN 模型文件路径
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
 * 实际的 RKNN 初始化在 rknn_init() 中完成
 */
int YOLOv5Engine::init() {
    return rknn_init(nullptr, false, -1);
}

/**
 * IEngine::detect — 执行检测并返回结果
 * @param frame 输入帧（会被 clone 后处理）
 * @param out   输出检测结果
 */
int YOLOv5Engine::detect(const cv::Mat &frame, detect_result_group_t *out) {
    cv::Mat mutable_frame = frame.clone();
    infer(mutable_frame, out);
    return 0;
}

/**
 * IEngine::setThresholds — 动态设置阈值（线程安全）
 * @param conf 置信度阈值
 * @param nms  NMS IoU 阈值
 */
void YOLOv5Engine::setThresholds(float conf, float nms) {
    std::lock_guard<std::mutex> lock(mtx);
    box_conf_threshold = conf;
    nms_threshold = nms;
}

// ---- RKNN 特有初始化 ----

/**
 * RKNN 模型初始化（支持上下文共享和核心绑定）
 *
 * 流程：
 *   1. 初始化后处理上下文（加载标签和 anchor）
 *   2. 从文件加载模型到内存
 *   3. 创建或共享 RKNN 上下文
 *   4. 绑定 NPU 核心
 *   5. 查询输入输出张量属性
 *
 * @param ctx_in       主模型上下文指针（用于共享模型参数）
 * @param share_weight true=复用已有权重，false=完整加载
 * @param core_num     NPU 核心编号（-1 使用全局轮询）
 * @return 0 成功，-1 失败
 */
int YOLOv5Engine::rknn_init(rknn_context *ctx_in, bool share_weight, int core_num)
{
    LOG_INFO("[YOLOv5Engine]", "Loading model...");

    // 初始化后处理上下文（加载标签文件和 anchor 参数）
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

    // 创建或共享 RKNN 上下文
    // 第一个实例完整加载模型权重，后续实例复用权重（节省 NPU 内存）
    if (share_weight == true)
        ret = rknn_dup_context(ctx_in, &ctx);  // 复用上下文
    else
        ret = ::rknn_init(&ctx, model_data.data(), model_data.size(), 0, NULL);  // 完整加载

    if (ret < 0)
    {
        LOG_ERROR("[YOLOv5Engine]", "rknn_init error ret=%d", ret);
        return -1;
    }

    // 设置模型绑定的 NPU 核心
    // RK3588 有 3 个 NPU 核心，按通道均匀分配减少争抢
    rknn_core_mask core_mask;
    int core_id = (core_num >= 0) ? core_num : get_core_num();
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

    // 查询 RKNN SDK 版本信息（用于日志）
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

    // 查询并存储输入张量属性（维度、数据类型、量化参数等）
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

    // 查询并存储输出张量属性
    output_attrs.resize(io_num.n_output);
    for (int i = 0; i < io_num.n_output; i++)
    {
        output_attrs[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &(output_attrs[i]), sizeof(rknn_tensor_attr));
        dump_tensor_attr(&(output_attrs[i]));
    }

    // 预计算输出量化参数，避免每帧重复构造
    // 量化公式：float_value = (int8_value - zp) * scale
    out_scales_.resize(io_num.n_output);
    out_zps_.resize(io_num.n_output);
    for (int i = 0; i < io_num.n_output; ++i)
    {
        out_scales_[i] = output_attrs[i].scale;
        out_zps_[i] = output_attrs[i].zp;
    }

    // 解析模型输入张量的格式和尺寸
    // 支持 NCHW 和 NHWC 两种格式
    if (input_attrs[0].fmt == RKNN_TENSOR_NCHW)
    {
        LOG_INFO("[YOLOv5Engine]", "model is NCHW input fmt");
        channel = input_attrs[0].dims[1];  // NCHW: C 在 dims[1]
        height = input_attrs[0].dims[2];
        width = input_attrs[0].dims[3];
    }
    else
    {
        LOG_INFO("[YOLOv5Engine]", "model is NHWC input fmt");
        height = input_attrs[0].dims[1];  // NHWC: H 在 dims[1]
        width = input_attrs[0].dims[2];
        channel = input_attrs[0].dims[3];
    }
    LOG_INFO("[YOLOv5Engine]", "model input height=%d, width=%d, channel=%d", height, width, channel);

    // 配置输入张量参数
    memset(inputs, 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = RKNN_TENSOR_UINT8;  // 输入数据类型为 8 位无符号整数
    inputs[0].size = width * height * channel;
    inputs[0].fmt = RKNN_TENSOR_NHWC;    // 输入格式为 NHWC（高度、宽度、通道）
    inputs[0].pass_through = 0;          // 不进行直通模式

    // 预分配推理输入图像，避免每帧重新分配
    resized_img_ = cv::Mat(height, width, CV_8UC3);

    return 0;
}

/**
 * 获取模型上下文指针，供子模型共享权重
 */
rknn_context *YOLOv5Engine::get_pctx()
{
    return &ctx;
}

// P2: 使用 RGA 硬件加速预处理
#include "RgaAccelerator.hpp"

/**
 * Stage P (CPU + RGA) — 预处理
 *
 * 流程：
 *   1. 计算缩放比例（保持宽高比）
 *   2. BGR→RGB 颜色转换
 *   3. Letterbox 缩放+填充（黑色边框）
 *
 * 优化：
 *   - 优先使用 RGA 硬件加速（速度提升 50-60%）
 *   - RGA 失败时回退 CPU（cv::cvtColor + letterbox）
 *
 * @param orig_img 原始输入图像（BGR 格式）
 * @param data     输出预处理数据（RGB 图像 + 缩放参数）
 * @param model_w  模型输入宽度
 * @param model_h  模型输入高度
 */
static void stage_preprocess(const cv::Mat &orig_img, YOLOv5Engine::PipelineData &data, int model_w, int model_h)
{
    int img_w = orig_img.cols;
    int img_h = orig_img.rows;
    cv::Size target_size(model_w, model_h);

    // 计算缩放比例
    data.scale_w = (float)target_size.width / img_w;
    data.scale_h = (float)target_size.height / img_h;

    if (img_w != model_w || img_h != model_h)
    {
        // 需要缩放：保持宽高比，取较小缩放比例
        float min_scale = std::min(data.scale_w, data.scale_h);
        data.scale_w = min_scale;
        data.scale_h = min_scale;
        memset(&data.pads, 0, sizeof(BOX_RECT));

        // 尝试 RGA 硬件加速（BGR→RGB + 缩放 + 填充一步完成）
        if (RgaAccelerator::letterboxRga(orig_img, data.padded_img, data.pads, model_w, model_h) == 0) {
            // RGA 成功，rgb_img 复用 padded_img
            data.rgb_img = data.padded_img;
        } else {
            // RGA 失败，回退 CPU
            cv::cvtColor(orig_img, data.rgb_img, cv::COLOR_BGR2RGB);
            letterbox(data.rgb_img, data.padded_img, data.pads, min_scale, target_size);
        }
    }
    else
    {
        // 尺寸匹配，只需颜色转换
        data.scale_w = 1.0f;
        data.scale_h = 1.0f;
        memset(&data.pads, 0, sizeof(BOX_RECT));

        // 使用 RGA 进行 BGR→RGB
        if (RgaAccelerator::cvtColorRga(orig_img, data.rgb_img) != 0) {
            cv::cvtColor(orig_img, data.rgb_img, cv::COLOR_BGR2RGB);
        }
    }
}

/**
 * 执行目标检测推理（三级流水线：P → I → O）
 *
 * @param orig_img  原始输入图像（BGR 格式），推理后原地绘制检测框
 * @param out_group [可选] 输出检测结果组，传 NULL 时不输出
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

    // ========== Stage P (CPU) — 预处理 ==========
    // 栈上分配 PipelineData，线程安全
    PipelineData data;
    stage_preprocess(orig_img, data, width, height);

    // ========== Stage I (NPU) — 推理 ==========
    // 设置输入数据（指向预处理后的 RGB 图像）
    inputs[0].buf = data.padded_img.empty() ? data.rgb_img.data : data.padded_img.data;

    // 提交输入到 NPU
    ret = rknn_inputs_set(ctx, io_num.n_input, inputs);
    if (ret < 0)
    {
        LOG_ERROR("[YOLOv5Engine]", "rknn_inputs_set failed, ret=%d", ret);
        return orig_img;
    }

    // 准备输出缓冲区
    rknn_output outputs[io_num.n_output];
    memset(outputs, 0, sizeof(outputs));
    for (int i = 0; i < io_num.n_output; i++)
        outputs[i].want_float = 0;  // 不需要 float 输出（使用量化参数反量化）

    // 带重试的推理执行（最多重试 3 次）
    int max_retry = 3;
    bool infer_ok = false;
    for (int retry = 0; retry < max_retry; retry++)
    {
        // 执行推理
        ret = rknn_run(ctx, NULL);
        if (ret < 0)
        {
            LOG_ERROR("[YOLOv5Engine]", "rknn_run failed (retry %d/%d), ret=%d",
                    retry + 1, max_retry, ret);
            continue;
        }

        // 获取推理输出
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

    // ========== Stage O (CPU) — 后处理 + 绘制 ==========
    detect_result_group_t detect_result_group;

    // 后处理：三尺度特征图解码 + NMS + 坐标还原
    post_ctx_.process((int8_t *)outputs[0].buf, (int8_t *)outputs[1].buf, (int8_t *)outputs[2].buf,
                 height, width, conf, nms, data.pads, data.scale_w, data.scale_h,
                 out_zps_, out_scales_, &detect_result_group);

    // 在原图上绘制检测框和类别标签
    char text[256];
    for (int i = 0; i < detect_result_group.count; i++)
    {
        detect_result_t *det_result = &(detect_result_group.results[i]);
        snprintf(text, sizeof(text), "%s %.1f%%", det_result->name, det_result->prop * 100);

        // 绘制矩形框（蓝色）
        rectangle(orig_img, cv::Point(det_result->box.left, det_result->box.top),
                  cv::Point(det_result->box.right, det_result->box.bottom), cv::Scalar(255, 0, 0), 3);

        // 绘制类别标签（白色）
        putText(orig_img, text, cv::Point(det_result->box.left, det_result->box.top + 12),
                cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255));
    }

    // 输出检测结果（如果调用方需要）
    if (out_group)
        *out_group = detect_result_group;

    // 保存最近一次检测结果（线程安全）
    {
        std::lock_guard<std::mutex> lock(mtx);
        last_detect_result_ = detect_result_group;
    }

    // 释放 NPU 输出缓冲区
    ret = rknn_outputs_release(ctx, io_num.n_output, outputs);
    if (ret < 0)
        LOG_ERROR("[YOLOv5Engine]", "rknn_outputs_release failed, ret=%d", ret);

    return orig_img;
}

/**
 * 析构函数：释放 RKNN 资源
 * model_data（vector）自动 RAII 析构
 */
YOLOv5Engine::~YOLOv5Engine()
{
    // post_ctx_ / input_attrs / output_attrs / model_data 自动 RAII 析构
    rknn_destroy(ctx);
}
