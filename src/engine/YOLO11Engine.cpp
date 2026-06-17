#include "engine/YOLO11Engine.hpp"
#include "preprocess.h"
#include "coreNum.hpp"

#include <future>
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

/* ============ 辅助函数（与 rkYolov5s.cpp 共享） ============ */

static unsigned char *load_model(const char *filename, int *model_size)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) { printf("Open file %s failed.\n", filename); return NULL; }
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    unsigned char *data = (unsigned char *)malloc(size);
    if (!data) { fclose(fp); return NULL; }
    fseek(fp, 0, SEEK_SET);
    size_t read_bytes = fread(data, 1, size, fp);
    fclose(fp);
    if (read_bytes != (size_t)size) { free(data); return NULL; }
    *model_size = size;
    return data;
}

/* ============ YOLO11Engine ============ */

YOLO11Engine::YOLO11Engine(const std::string &model_path)
    : model_path(model_path)
{
    // 阈值由 IEngine::setThresholds 或 DetectThread 外部设置
    nms_threshold = 0.45f;
    box_conf_threshold = 0.25f;
}

int YOLO11Engine::init()
{
    return rknn_init(nullptr, false, -1);
}

int YOLO11Engine::rknn_init(rknn_context *ctx_in, bool share_weight, int core_num)
{
    printf("[YOLO11] Loading model...\n");

    /* 初始化后处理 */
    if (post_ctx_.init(model_path.c_str()) < 0) {
        printf("[YOLO11] Failed to init postprocess\n");
        return -1;
    }

    /* 加载模型 */
    int model_data_size = 0;
    model_data = load_model(model_path.c_str(), &model_data_size);
    if (!model_data) {
        printf("[YOLO11] Failed to load model\n");
        return -1;
    }

    /* 支持权重共享（供 rknnPool 调用） */
    if (share_weight && ctx_in) {
        ret = rknn_dup_context(ctx_in, &ctx);
    } else {
        ret = ::rknn_init(&ctx, model_data, model_data_size, 0, NULL);
    }
    if (ret < 0) {
        printf("[YOLO11] rknn_init failed ret=%d\n", ret);
        free(model_data); model_data = nullptr;
        return -1;
    }

    /* NPU 核心绑定 */
    rknn_core_mask core_mask;
    switch (core_num) {
    case 0: core_mask = RKNN_NPU_CORE_0; break;
    case 1: core_mask = RKNN_NPU_CORE_1; break;
    case 2: core_mask = RKNN_NPU_CORE_2; break;
    default: core_mask = RKNN_NPU_CORE_AUTO; break;
    }
    ret = rknn_set_core_mask(ctx, core_mask);
    if (ret < 0) {
        printf("[YOLO11] rknn_set_core_mask failed ret=%d\n", ret);
        ::rknn_destroy(ctx);
        free(model_data); model_data = nullptr;
        return -1;
    }

    /* 查询输入输出 */
    rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    printf("[YOLO11] input num: %d, output num: %d\n", io_num.n_input, io_num.n_output);

    input_attrs.resize(io_num.n_input);
    for (int i = 0; i < io_num.n_input; i++) {
        input_attrs[i].index = i;
        rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &input_attrs[i], sizeof(rknn_tensor_attr));
    }

    output_attrs.resize(io_num.n_output);
    for (int i = 0; i < io_num.n_output; i++) {
        output_attrs[i].index = i;
        rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &output_attrs[i], sizeof(rknn_tensor_attr));
    }

    /* 量化参数（单输出） */
    out_zp_ = output_attrs[0].zp;
    out_scale_ = output_attrs[0].scale;

    /* 输入尺寸解析 */
    if (input_attrs[0].fmt == RKNN_TENSOR_NCHW) {
        channel = input_attrs[0].dims[1];
        height  = input_attrs[0].dims[2];
        width   = input_attrs[0].dims[3];
    } else {
        height  = input_attrs[0].dims[1];
        width   = input_attrs[0].dims[2];
        channel = input_attrs[0].dims[3];
    }
    printf("[YOLO11] model input: %dx%dx%d\n", height, width, channel);

    /* 配置输入 */
    memset(inputs, 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = RKNN_TENSOR_UINT8;
    inputs[0].size = width * height * channel;
    inputs[0].fmt = RKNN_TENSOR_NHWC;
    inputs[0].pass_through = 0;

    resized_img_ = cv::Mat(height, width, CV_8UC3);
    return 0;
}

void YOLO11Engine::setThresholds(float conf, float nms)
{
    std::lock_guard<std::mutex> lock(mtx);
    box_conf_threshold = conf;
    nms_threshold = nms;
}

int YOLO11Engine::detect(const cv::Mat &frame, detect_result_group_t *out)
{
    cv::Mat mutable_frame = frame.clone();
    infer(mutable_frame, out);
    return 0;
}

cv::Mat YOLO11Engine::infer(cv::Mat &orig_img, detect_result_group_t *out_group)
{
    float conf, nms;
    {
        std::lock_guard<std::mutex> lock(mtx);
        conf = box_conf_threshold;
        nms = nms_threshold;
    }

    /* YOLO11 预处理：BGR→RGB + letterbox */
    cv::Mat img;
    cv::cvtColor(orig_img, img, cv::COLOR_BGR2RGB);
    int img_w = img.cols, img_h = img.rows;

    BOX_RECT pads;
    memset(&pads, 0, sizeof(BOX_RECT));
    cv::Size target_size(width, height);
    float scale_w = (float)target_size.width / img_w;
    float scale_h = (float)target_size.height / img_h;

    if (img_w != width || img_h != height) {
        float min_scale = std::min(scale_w, scale_h);
        scale_w = scale_h = min_scale;
        letterbox(img, resized_img_, pads, min_scale, target_size);
        inputs[0].buf = resized_img_.data;
    } else {
        scale_w = scale_h = 1.0f;
        memset(&pads, 0, sizeof(pads));
        inputs[0].buf = img.data;
    }

    rknn_inputs_set(ctx, io_num.n_input, inputs);

    /* 推理 + 获取输出（带重试） */
    std::vector<rknn_output> outputs(io_num.n_output);
    memset(outputs.data(), 0, outputs.size() * sizeof(rknn_output));
    for (int i = 0; i < io_num.n_output; i++)
        outputs[i].want_float = 0;

    int max_retry = 3;
    bool infer_ok = false;
    for (int retry = 0; retry < max_retry; retry++) {
        ret = rknn_run(ctx, NULL);
        if (ret < 0) {
            fprintf(stderr, "[YOLO11] rknn_run failed (retry %d/%d)\n", retry + 1, max_retry);
            continue;
        }
        ret = rknn_outputs_get(ctx, io_num.n_output, outputs.data(), NULL);
        if (ret < 0) {
            fprintf(stderr, "[YOLO11] rknn_outputs_get failed (retry %d/%d)\n", retry + 1, max_retry);
            continue;
        }
        infer_ok = true;
        break;
    }
    if (!infer_ok) {
        fprintf(stderr, "[YOLO11] inference failed after %d retries\n", max_retry);
        return orig_img;
    }

    /* 后处理 */
    detect_result_group_t detect_result;
    post_ctx_.process((int8_t *)outputs[0].buf, height, width,
                      conf, nms, pads, scale_w, scale_h,
                      out_zp_, out_scale_, &detect_result);

    /* 绘制 */
    char text[256];
    for (int i = 0; i < detect_result.count; i++) {
        auto *det = &detect_result.results[i];
        snprintf(text, sizeof(text), "%s %.1f%%", det->name, det->prop * 100);
        rectangle(orig_img, cv::Point(det->box.left, det->box.top),
                  cv::Point(det->box.right, det->box.bottom), cv::Scalar(255, 0, 0), 3);
        putText(orig_img, text, cv::Point(det->box.left, det->box.top + 12),
                cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255));
    }

    if (out_group) *out_group = detect_result;

    {
        std::lock_guard<std::mutex> lock(mtx);
        last_detect_result_ = detect_result;
    }

    rknn_outputs_release(ctx, io_num.n_output, outputs.data());
    return orig_img;
}

YOLO11Engine::~YOLO11Engine()
{
    ::rknn_destroy(ctx);
    if (model_data) free(model_data);
}
