/**
 * @file rknn_engine.cpp
 * @brief RKNN 推理引擎实现 (RK3588 only)
 *
 * 编译条件: 仅在 __arm__ (aarch64) 平台编译。
 * Windows 上此文件编译为空操作，方便开发时保持项目结构完整。
 */

#include "engine/rknn_engine.hpp"
#include <stdexcept>

RknnEngine::RknnEngine() {
#ifdef __arm__
    // On RK3588, the rkYolov5s instance is created in load()
#else
    // Windows stub
#endif
}

RknnEngine::~RknnEngine() {
#ifdef __arm__
    // rkYolov5s destructor handles cleanup
#endif
}

void RknnEngine::load(const std::string& modelPath) {
#ifdef __arm__
    yolo_ = std::make_unique<rkYolov5s>(modelPath);
    int ret = yolo_->init(&ctx_, false);  // false = primary context
    if (ret != 0) {
        yolo_.reset();
        throw std::runtime_error("RKNN init failed, code: " + std::to_string(ret));
    }
#else
    (void)modelPath;
#endif
}

bool RknnEngine::loaded() const {
#ifdef __arm__
    return yolo_ != nullptr;
#else
    return false;
#endif
}

int RknnEngine::inputSize() const {
    return 3 * 640 * 640;
}

void RknnEngine::infer(const std::vector<float>& input,
                        std::vector<Detection>& detections,
                        int imgWidth, int imgHeight,
                        float confThreshold, float iouThreshold) {
    detections.clear();
#ifdef __arm__
    // Reconstruct cv::Mat from float tensor (reverse Preprocessor::imageToTensor)
    cv::Mat rgb(640, 640, CV_32FC3);
    int idx = 0;
    for (int c = 0; c < 3; c++) {
        for (int h = 0; h < 640; h++) {
            for (int w = 0; w < 640; w++) {
                rgb.at<cv::Vec3f>(h, w)[c] = input[idx++];
            }
        }
    }
    rgb.convertTo(rgb, CV_8UC3, 255.0);
    cv::Mat bgr;
    cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);

    // Delegate to existing rkYolov5s inference pipeline
    cv::Mat result = yolo_->infer(bgr);

    // rkYolov5s::infer returns annotated image; detections are already drawn.
    // For raw detections, use the separate postprocess pipeline (postprocess.cpp).
    // This is a simplified wrapper for compatibility.
    (void)imgWidth; (void)imgHeight;
    (void)confThreshold; (void)iouThreshold;
#else
    (void)input; (void)imgWidth; (void)imgHeight;
    (void)confThreshold; (void)iouThreshold;
#endif
}
