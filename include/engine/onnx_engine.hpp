/**
 * @file onnx_engine.hpp
 * @brief ONNX Runtime 推理引擎实现 (Windows CPU)
 *
 * 封装 ONNX Runtime C++ API，实现 IEngine 接口。
 * 支持 YOLO11 标准 ONNX 模型 (单输出头 [C+4, 8400])。
 */

#ifndef ONNX_ENGINE_HPP
#define ONNX_ENGINE_HPP

#include "engine/inference_engine.hpp"
#include "core/config.hpp"
#include <onnxruntime_cxx_api.h>
#include <memory>

class OnnxEngine : public IEngine {
public:
    OnnxEngine();
    ~OnnxEngine() override;

    void load(const std::string& modelPath) override;
    EngineType type() const override { return EngineType::ONNX; }
    bool loaded() const override { return session_ != nullptr; }
    int inputSize() const override;
    int inputWidth() const override  { return inputW_; }
    int inputHeight() const override { return inputH_; }

    void infer(const std::vector<float>& input,
               std::vector<Detection>& detections,
               int imgWidth, int imgHeight,
               float confThreshold, float iouThreshold) override;

private:
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::MemoryInfo> memoryInfo_;
    Ort::SessionOptions sessionOptions_;

    std::vector<const char*> inputNames_;
    std::vector<const char*> outputNames_;
    std::vector<int64_t> inputShape_;
    int inputW_ = 640;
    int inputH_ = 640;
    int numClasses_ = 80;
    int numAnchors_ = 8400;

    // 内部后处理：解析 ONNX 输出 + NMS
    std::vector<Detection> decodeDetections(const float* output,
                                            int imgW, int imgH,
                                            float confThresh, float nmsThresh);
};

#endif // ONNX_ENGINE_HPP
