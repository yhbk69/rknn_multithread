/**
 * @file onnx_engine.cpp
 * @brief ONNX Runtime 推理引擎实现
 */

#include "engine/onnx_engine.hpp"
#include "core/logger.hpp"
#include <opencv2/dnn.hpp>
#include <algorithm>
#include <cmath>

OnnxEngine::OnnxEngine() {
    env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "YOLO_Detection");
    sessionOptions_.SetIntraOpNumThreads(4);
    sessionOptions_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
}

OnnxEngine::~OnnxEngine() {
    // Ort 对象需按特定顺序析构（session 在 env 之前）
    session_.reset();
    memoryInfo_.reset();
    env_.reset();
}

void OnnxEngine::load(const std::string& modelPath) {
    try {
        // 尝试 CPU EP
        session_ = std::make_unique<Ort::Session>(*env_,
            std::wstring(modelPath.begin(), modelPath.end()).c_str(),
            sessionOptions_);

        Ort::AllocatorWithDefaultOptions allocator;

        // 获取输入信息
        size_t numInputs = session_->GetInputCount();
        inputNames_.clear();
        for (size_t i = 0; i < numInputs; i++) {
            auto name = session_->GetInputNameAllocated(i, allocator);
            inputNames_.push_back(name.get());
            auto typeInfo = session_->GetInputTypeInfo(i);
            auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
            inputShape_ = tensorInfo.GetShape();
        }

        // 获取输出信息
        size_t numOutputs = session_->GetOutputCount();
        outputNames_.clear();
        for (size_t i = 0; i < numOutputs; i++) {
            auto name = session_->GetOutputNameAllocated(i, allocator);
            outputNames_.push_back(name.get());
        }

        // 从输入形状推断尺寸
        // 典型: [1, 3, 640, 640] 或 [1, 3, H, W]
        if (inputShape_.size() >= 4) {
            inputH_ = static_cast<int>(inputShape_[2]);
            inputW_ = static_cast<int>(inputShape_[3]);
        }

        // 从输出形状推断类别数
        // 典型: [1, 84, 8400] → numClasses = 84 - 4 = 80
        // 也可能是 [1, 8400, 84]
        if (numOutputs > 0) {
            auto typeInfo = session_->GetOutputTypeInfo(0);
            auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
            auto outShape = tensorInfo.GetShape();
            if (outShape.size() == 3) {
                // 取第二维和第三维中较小的为类别+4，较大的为锚点数
                int64_t dim1 = outShape[1];
                int64_t dim2 = outShape[2];
                numClasses_ = static_cast<int>(std::min(dim1, dim2) - 4);
                numAnchors_ = static_cast<int>(std::max(dim1, dim2));
            }
        }

        memoryInfo_ = std::make_unique<Ort::MemoryInfo>(
            Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));

        LOG_INFO("model") << "ONNX model loaded: " << modelPath
                          << " (" << inputW_ << "x" << inputH_
                          << ", " << numClasses_ << " classes, "
                          << numAnchors_ << " anchors)";
    } catch (const Ort::Exception& e) {
        LOG_ERROR("model") << "Failed to load ONNX model: " << e.what();
        throw std::runtime_error(std::string("ONNX load error: ") + e.what());
    }
}

int OnnxEngine::inputSize() const {
    return 3 * inputW_ * inputH_;
}

void OnnxEngine::infer(const std::vector<float>& input,
                       std::vector<Detection>& detections,
                       int imgWidth, int imgHeight,
                       float confThreshold, float iouThreshold) {
    if (!session_) {
        detections.clear();
        return;
    }

    try {
        std::vector<int64_t> inShape = {1, 3, inputH_, inputW_};

        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            *memoryInfo_,
            const_cast<float*>(input.data()),
            input.size(),
            inShape.data(),
            inShape.size());

        auto outputs = session_->Run(Ort::RunOptions{nullptr},
            inputNames_.data(), &inputTensor, 1,
            outputNames_.data(), outputNames_.size());

        // 取第一个输出
        auto& outputTensor = outputs[0];
        const float* outputData = outputTensor.GetTensorData<float>();

        detections = decodeDetections(outputData, imgWidth, imgHeight,
                                      confThreshold, iouThreshold);
    } catch (const Ort::Exception& e) {
        LOG_ERROR("model") << "Inference error: " << e.what();
        detections.clear();
    }
}

std::vector<Detection> OnnxEngine::decodeDetections(const float* output,
                                                     int imgW, int imgH,
                                                     float confThresh, float nmsThresh) {
    std::vector<cv::Rect> boxes;
    std::vector<float> confidences;
    std::vector<int> classIds;

    // 确定输出格式: [C+4, N] 列优先 or [N, C+4] 行优先
    // 我们以 [N, C+4] (行优先: 每个锚点84个值) 和 [C+4, N] (列优先) 两种格式尝试验证
    int stride = numClasses_ + 4;
    int N = numAnchors_;

    float scaleX = static_cast<float>(imgW) / inputW_;
    float scaleY = static_cast<float>(imgH) / inputH_;

    // 自动检测输出格式: 检查第 4 个元素（第二个锚点的 cx）
    // 如果 output[N] 的值合理（0~1 范围），则是列优先 [C+4, N]
    // 如果 output[stride] 的值合理，则是行优先 [N, C+4]
    bool columnMajor = (N > 0 && output[N] >= 0.0f && output[N] <= 1.0f);

    for (int i = 0; i < N; ++i) {
        float cx, cy, w, h;

        if (columnMajor) {
            cx = output[0 * N + i];
            cy = output[1 * N + i];
            w  = output[2 * N + i];
            h  = output[3 * N + i];
        } else {
            int base = i * stride;
            cx = output[base + 0];
            cy = output[base + 1];
            w  = output[base + 2];
            h  = output[base + 3];
        }

        // 找最大类别分数
        int classId = 0;
        float maxConf = -1.0f;
        for (int j = 0; j < numClasses_; ++j) {
            float score;
            if (columnMajor) {
                score = output[(4 + j) * N + i];
            } else {
                score = output[i * stride + 4 + j];
            }
            if (score > maxConf) {
                maxConf = score;
                classId = j;
            }
        }

        if (maxConf < confThresh) continue;

        // 坐标缩放
        float x = (cx - w / 2.0f) * scaleX;
        float y = (cy - h / 2.0f) * scaleY;
        float bw = w * scaleX;
        float bh = h * scaleY;

        int bx = std::max(0, static_cast<int>(x));
        int by = std::max(0, static_cast<int>(y));
        int bw_int = std::min(imgW - bx, static_cast<int>(bw));
        int bh_int = std::min(imgH - by, static_cast<int>(bh));

        boxes.emplace_back(bx, by, bw_int, bh_int);
        confidences.push_back(maxConf);
        classIds.push_back(classId);
    }

    // NMS
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, confThresh, nmsThresh, indices);

    std::vector<Detection> detections;
    detections.reserve(indices.size());
    for (int idx : indices) {
        Detection det;
        det.x = static_cast<float>(boxes[idx].x);
        det.y = static_cast<float>(boxes[idx].y);
        det.w = static_cast<float>(boxes[idx].width);
        det.h = static_cast<float>(boxes[idx].height);
        det.conf = confidences[idx];
        det.class_id = classIds[idx];
        detections.push_back(det);
    }

    return detections;
}
