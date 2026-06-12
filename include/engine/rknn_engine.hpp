/**
 * @file rknn_engine.hpp
 * @brief RKNN 推理引擎 — 封装现有 rkYolov5s 实现 IEngine 接口
 *
 * 使用场景: RK3588 上 YOLOv5s RKNN 模型推理。
 * 内部委托给已验证的 rkYolov5s 类，不修改原有代码。
 */

#ifndef RKNN_ENGINE_HPP
#define RKNN_ENGINE_HPP

#include "engine/inference_engine.hpp"
#include <memory>
#include <string>

#ifdef __arm__
#include "rknn_api.h"
#include "rkYolov5s.hpp"
#endif

class RknnEngine : public IEngine {
public:
    RknnEngine();
    ~RknnEngine() override;

    void load(const std::string& modelPath) override;
    EngineType type() const override { return EngineType::RKNN; }
    bool loaded() const override;
    int inputSize() const override;
    int inputWidth() const override  { return 640; }
    int inputHeight() const override { return 640; }

    void infer(const std::vector<float>& input,
               std::vector<Detection>& detections,
               int imgWidth, int imgHeight,
               float confThreshold, float iouThreshold) override;

private:
#ifdef __arm__
    std::unique_ptr<rkYolov5s> yolo_;
    rknn_context ctx_;
#else
    bool loaded_ = false;
#endif
};

#endif // RKNN_ENGINE_HPP
