#ifndef YOLO11_ENGINE_H
#define YOLO11_ENGINE_H

#include "rknn_api.h"
#include "core/IEngine.hpp"
#include "postprocess_yolo11.hpp"

#include "opencv2/core/core.hpp"

/*
 * YOLO11Engine — YOLO11 目标检测推理引擎
 *
 * 继承 IEngine 接口，处理单输出张量的 anchor-free 模型。
 * 适用于 YOLO11n/s/m/l/x 等 RKNN 模型。
 */
class YOLO11Engine : public IEngine
{
private:
    int ret;
    mutable std::mutex mtx;
    std::string model_path;
    unsigned char *model_data = nullptr;

    rknn_context ctx;
    rknn_input_output_num io_num;
    std::vector<rknn_tensor_attr> input_attrs;
    std::vector<rknn_tensor_attr> output_attrs;
    rknn_input inputs[1];

    int channel, width, height;
    cv::Mat resized_img_;
    int32_t out_zp_;
    float out_scale_;

    float nms_threshold, box_conf_threshold;
    detect_result_group_t last_detect_result_;
    YOLO11PostProcess post_ctx_;

public:
    YOLO11Engine(const std::string &model_path);

    // ---- IEngine 接口 ----
    int init() override;
    int detect(const cv::Mat &frame, detect_result_group_t *out) override;
    int getInputWidth() const override { return width; }
    int getInputHeight() const override { return height; }
    void setThresholds(float conf, float nms) override;

    // ---- rknn 特有 ----
    int rknn_init();
    detect_result_group_t getLastDetectResult() const {
        std::lock_guard<std::mutex> lock(mtx);
        return last_detect_result_;
    }

    // ---- 推理入口（供 rknnPool 模板调用） ----
    cv::Mat infer(cv::Mat &orig_img, detect_result_group_t *out_group = nullptr);

    ~YOLO11Engine() override;
};

#endif
