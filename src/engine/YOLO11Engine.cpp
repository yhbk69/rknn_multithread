/*
 * YOLO11Engine.cpp - YOLO11 RKNN 推理引擎实现
 *
 * 与 YOLOv5Engine 的区别：
 *   - YOLO11 是 anchor-free 模型，输出单张量 [1, 8400, 84]
 *   - 无预定义 anchor，使用 DFL（Distribution Focal Loss）解码
 *   - 后处理使用 YOLO11PostProcess 类
 *
 * 三级流水线：
 *   Stage P (CPU) — 预处理：BGR→RGB + letterbox
 *   Stage I (NPU) — 推理：rknn_inputs_set → rknn_run → rknn_outputs_get
 *   Stage O (CPU) — 后处理：单张量解码 + NMS + 绘制检测框
 */

#include "engine/YOLO11Engine.hpp"
#include "preprocess.h"
#include "coreNum.hpp"

#include <future>
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

// P1: 使用彩色日志系统
#include "Logger.hpp"

// P2: 使用 RGA 硬件加速预处理
#include "RgaAccelerator.hpp"

/* ============ 辅助函数 ============ */

/**
 * 加载整个模型文件到 vector（RAII 自动释放内存）
 * @param filename 模型文件路径
 * @return 模型数据 vector，失败返回空 vector
 */
static std::vector<unsigned char> load_model(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        LOG_ERROR("[YOLO11]", "Open file %s failed", filename);
        return {};
    }
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<unsigned char> data(size);
    size_t read_bytes = fread(data.data(), 1, size, fp);
    fclose(fp);
    if (read_bytes != (size_t)size) return {};
    return data;
}

/* ============ YOLO11Engine 实现 ============ */

/**
 * 构造函数：初始化模型路径和默认阈值
 * @param model_path RKNN 模型文件路径
 */
YOLO11Engine::YOLO11Engine(const std::string &model_path)
    : model_path(model_path)
{
    // YOLO11 默认阈值
    nms_threshold = 0.45f;
    box_conf_threshold = 0.25f;
}

/**
 * IEngine::init — 通用初始化（无参，用于非 rknn 场景）
 */
int YOLO11Engine::init()
{
    return rknn_init(nullptr, false, -1);
}

/**
 * RKNN 模型初始化（支持上下文共享和核心绑定）
 *
 * 与 YOLOv5Engine::rknn_init 流程相同：
 *   1. 初始化后处理上下文
 *   2. 加载模型文件
 *   3. 创建/共享 RKNN 上下文
 *   4. 绑定 NPU 核心
 *   5. 查询输入输出张量
 *
 * @param ctx_in       主模型上下文（用于共享权重）
 * @param share_weight true=复用权重，false=完整加载
 * @param core_num     NPU 核心编号（-1=自动）
 * @return 0 成功，-1 失败
 */
int YOLO11Engine::rknn_init(rknn_context *ctx_in, bool share_weight, int core_num)
{
    LOG_INFO("[YOLO11]", "Loading model...");

    // 初始化后处理上下文（加载标签）
    if (post_ctx_.init(model_path.c_str()) < 0) {
        LOG_ERROR("[YOLO11]", "Failed to init postprocess");
        return -1;
    }

    // 加载模型文件到内存
    model_data = load_model(model_path.c_str());
    if (model_data.empty()) {
        LOG_ERROR("[YOLO11]", "Failed to load model");
        return -1;
    }

    // 创建或共享 RKNN 上下文
    if (share_weight && ctx_in) {
        ret = rknn_dup_context(ctx_in, &ctx);  // 复用已有上下文的权重
    } else {
        ret = ::rknn_init(&ctx, model_data.data(), model_data.size(), 0, NULL);  // 完整加载
    }
    if (ret < 0) {
        LOG_ERROR("[YOLO11]", "rknn_init failed ret=%d", ret);
        return -1;
    }

    // 绑定 NPU 核心（RK3588 有 3 个核心）
    rknn_core_mask core_mask;
    switch (core_num) {
    case 0: core_mask = RKNN_NPU_CORE_0; break;
    case 1: core_mask = RKNN_NPU_CORE_1; break;
    case 2: core_mask = RKNN_NPU_CORE_2; break;
    default: core_mask = RKNN_NPU_CORE_AUTO; break;  // 自动选择
    }
    ret = rknn_set_core_mask(ctx, core_mask);
    if (ret < 0) {
        LOG_ERROR("[YOLO11]", "rknn_set_core_mask failed ret=%d", ret);
        ::rknn_destroy(ctx);
        return -1;
    }

    // 查询输入输出张量数量
    rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    LOG_INFO("[YOLO11]", "input num: %d, output num: %d", io_num.n_input, io_num.n_output);

    // 查询输入张量属性
    input_attrs.resize(io_num.n_input);
    for (int i = 0; i < io_num.n_input; i++) {
        input_attrs[i].index = i;
        rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &input_attrs[i], sizeof(rknn_tensor_attr));
    }

    // 查询输出张量属性
    output_attrs.resize(io_num.n_output);
    for (int i = 0; i < io_num.n_output; i++) {
        output_attrs[i].index = i;
        rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &output_attrs[i], sizeof(rknn_tensor_attr));
    }

    // YOLO11 单输出：保存量化参数
    out_zp_ = output_attrs[0].zp;
    out_scale_ = output_attrs[0].scale;

    // 解析输入尺寸（支持 NCHW 和 NHWC 格式）
    if (input_attrs[0].fmt == RKNN_TENSOR_NCHW) {
        channel = input_attrs[0].dims[1];
        height  = input_attrs[0].dims[2];
        width   = input_attrs[0].dims[3];
    } else {
        height  = input_attrs[0].dims[1];
        width   = input_attrs[0].dims[2];
        channel = input_attrs[0].dims[3];
    }
    LOG_INFO("[YOLO11]", "model input: %dx%dx%d", height, width, channel);

    // 配置输入张量
    memset(inputs, 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = RKNN_TENSOR_UINT8;   // 8 位无符号整数
    inputs[0].size = width * height * channel;
    inputs[0].fmt = RKNN_TENSOR_NHWC;     // 高度、宽度、通道
    inputs[0].pass_through = 0;

    // 预分配输入缓冲区
    resized_img_ = cv::Mat(height, width, CV_8UC3);
    return 0;
}

/**
 * 动态设置阈值（线程安全）
 */
void YOLO11Engine::setThresholds(float conf, float nms)
{
    std::lock_guard<std::mutex> lock(mtx);
    box_conf_threshold = conf;
    nms_threshold = nms;
}

/**
 * IEngine::detect — 执行检测并返回结果
 */
int YOLO11Engine::detect(const cv::Mat &frame, detect_result_group_t *out)
{
    cv::Mat mutable_frame = frame.clone();
    infer(mutable_frame, out);
    return 0;
}

/**
 * 执行目标检测推理（三级流水线：P → I → O）
 *
 * 与 YOLOv5Engine::infer 的区别：
 *   - YOLO11 使用单输出张量，后处理使用 YOLO11PostProcess
 *
 * @param orig_img  原始输入图像（BGR），推理后原地绘制检测框
 * @param out_group [可选] 输出检测结果
 * @return 绘制了检测框的图像
 */
cv::Mat YOLO11Engine::infer(cv::Mat &orig_img, detect_result_group_t *out_group)
{
    // 读取阈值（线程安全）
    float conf, nms;
    {
        std::lock_guard<std::mutex> lock(mtx);
        conf = box_conf_threshold;
        nms = nms_threshold;
    }

    // ========== Stage P — 预处理 ==========
    cv::Mat img;
    BOX_RECT pads;
    memset(&pads, 0, sizeof(BOX_RECT));
    cv::Size target_size(width, height);
    int img_w = orig_img.cols, img_h = orig_img.rows;
    float scale_w = (float)target_size.width / img_w;
    float scale_h = (float)target_size.height / img_h;

    if (img_w != width || img_h != height) {
        // 需要缩放：保持宽高比
        float min_scale = std::min(scale_w, scale_h);
        scale_w = scale_h = min_scale;

        // 尝试 RGA 硬件加速
        if (RgaAccelerator::letterboxRga(orig_img, resized_img_, pads, width, height) == 0) {
            img = resized_img_;
        } else {
            // 回退 CPU
            cv::cvtColor(orig_img, img, cv::COLOR_BGR2RGB);
            letterbox(img, resized_img_, pads, min_scale, target_size);
            img = resized_img_;
        }
        inputs[0].buf = img.data;
    } else {
        // 尺寸匹配，只需颜色转换
        scale_w = scale_h = 1.0f;
        memset(&pads, 0, sizeof(pads));

        if (RgaAccelerator::cvtColorRga(orig_img, img) != 0) {
            cv::cvtColor(orig_img, img, cv::COLOR_BGR2RGB);
        }
        inputs[0].buf = img.data;
    }

    // ========== Stage I — NPU 推理 ==========
    rknn_inputs_set(ctx, io_num.n_input, inputs);

    // 准备输出缓冲区
    std::vector<rknn_output> outputs(io_num.n_output);
    memset(outputs.data(), 0, outputs.size() * sizeof(rknn_output));
    for (int i = 0; i < io_num.n_output; i++)
        outputs[i].want_float = 0;  // 使用量化参数反量化

    // 带重试的推理执行
    int max_retry = 3;
    bool infer_ok = false;
    for (int retry = 0; retry < max_retry; retry++) {
        ret = rknn_run(ctx, NULL);
        if (ret < 0) {
            LOG_ERROR("[YOLO11]", "rknn_run failed (retry %d/%d)", retry + 1, max_retry);
            continue;
        }
        ret = rknn_outputs_get(ctx, io_num.n_output, outputs.data(), NULL);
        if (ret < 0) {
            LOG_ERROR("[YOLO11]", "rknn_outputs_get failed (retry %d/%d)", retry + 1, max_retry);
            continue;
        }
        infer_ok = true;
        break;
    }
    if (!infer_ok) {
        LOG_ERROR("[YOLO11]", "inference failed after %d retries", max_retry);
        return orig_img;
    }

    // ========== Stage O — 后处理 ==========
    detect_result_group_t detect_result;

    // YOLO11 后处理：单张量解码 + NMS
    post_ctx_.process((int8_t *)outputs[0].buf, height, width,
                      conf, nms, pads, scale_w, scale_h,
                      out_zp_, out_scale_, &detect_result);

    // 在原图上绘制检测框
    char text[256];
    for (int i = 0; i < detect_result.count; i++) {
        auto *det = &detect_result.results[i];
        snprintf(text, sizeof(text), "%s %.1f%%", det->name, det->prop * 100);

        // 绘制矩形框（蓝色）
        rectangle(orig_img, cv::Point(det->box.left, det->box.top),
                  cv::Point(det->box.right, det->box.bottom), cv::Scalar(255, 0, 0), 3);

        // 绘制类别标签（白色）
        putText(orig_img, text, cv::Point(det->box.left, det->box.top + 12),
                cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255));
    }

    // 输出检测结果
    if (out_group) *out_group = detect_result;

    // 保存最近一次检测结果（线程安全）
    {
        std::lock_guard<std::mutex> lock(mtx);
        last_detect_result_ = detect_result;
    }

    // 释放 NPU 输出缓冲区
    rknn_outputs_release(ctx, io_num.n_output, outputs.data());
    return orig_img;
}

/**
 * 析构函数：释放 RKNN 资源
 * model_data（vector）自动 RAII 析构
 */
YOLO11Engine::~YOLO11Engine()
{
    ::rknn_destroy(ctx);
    // model_data (vector) 自动 RAII 析构
}
