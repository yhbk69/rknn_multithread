/*
 * rkYolov5s.hpp - YOLOv5s 模型推理类
 *
 * 本文件定义了 YOLOv5Engine 类，继承 IEngine 接口，
 * 封装了 YOLOv5s 模型加载、初始化和推理的完整生命周期。
 */

#ifndef RKYOLOV5S_H
#define RKYOLOV5S_H

#include "rknn_api.h"
#include "postprocess.h"
#include "core/IEngine.hpp"

#include "opencv2/core/core.hpp"

/*
 * YOLOv5Engine - YOLOv5s 目标检测推理引擎
 *
 * 继承 IEngine 抽象接口，内部使用互斥锁保证多线程推理安全。
 * 兼容 rkNpu 特有的上下文共享和核心绑定。
 */
class YOLOv5Engine : public IEngine
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
    std::vector<float> out_scales_;
    std::vector<int32_t> out_zps_;

    float nms_threshold, box_conf_threshold;

    detect_result_group_t last_detect_result_;
    PostProcessContext post_ctx_;

public:
    struct PipelineData {
        cv::Mat rgb_img;
        cv::Mat padded_img;
        BOX_RECT pads;
        float scale_w = 1.0f;
        float scale_h = 1.0f;
    };

    YOLOv5Engine(const std::string &model_path);

    // ---- IEngine 接口 ----
    int init() override;
    int detect(const cv::Mat &frame, detect_result_group_t *out) override;
    int getInputWidth() const override { return width; }
    int getInputHeight() const override { return height; }
    void setThresholds(float conf, float nms) override;

    // ---- rknn 特有（供 rknnPool 调用） ----
    int rknn_init(rknn_context *ctx_in, bool share_weight, int core_num = -1);
    rknn_context *get_pctx();
    detect_result_group_t getLastDetectResult() const {
        std::lock_guard<std::mutex> lock(mtx);
        return last_detect_result_;
    }

    // ---- 推理入口（供 rknnPool 模板调用） ----
    cv::Mat infer(cv::Mat &orig_img, detect_result_group_t *out_group = nullptr);

    ~YOLOv5Engine() override;
};

#endif
