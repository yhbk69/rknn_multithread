/**
 * @file inference_engine.hpp
 * @brief YOLO 推理引擎抽象接口
 *
 * 所有推理后端 (ONNX, RKNN, TensorRT等) 均实现此接口，
 * 上层代码通过 IEngine* 统一调用。
 *
 * 设计要点:
 * - load() + reload() 支持模型热切换
 * - 输入约定: 预处理后的 float 张量 (CHW, [0,1] 归一化)
 * - 输出: 统一为 Detection 列表
 */

#ifndef INFERENCE_ENGINE_HPP
#define INFERENCE_ENGINE_HPP

#include <vector>
#include <string>
#include "core/types.hpp"

enum class EngineType { ONNX, RKNN, RKNN_POOL, TensorRT, OpenVINO, PyTorch };

class IEngine {
public:
    virtual ~IEngine() = default;

    /// 加载模型文件
    virtual void load(const std::string& modelPath) = 0;

    /// 热切换模型（默认同 load，可覆盖实现更高效的切换）
    virtual void reload(const std::string& newPath) { load(newPath); }

    /// 引擎类型标识
    virtual EngineType type() const = 0;

    /// 是否已成功加载
    virtual bool loaded() const = 0;

    /// 输入张量元素个数 (供预处理器分配缓冲区)
    virtual int inputSize() const = 0;

    /// 模型输入尺寸 (width, height)
    virtual int inputWidth() const = 0;
    virtual int inputHeight() const = 0;

    /**
     * @brief 单帧推理
     * @param input         预处理后的 float 张量 (CHW, [0,1] 归一化)
     * @param detections    [out] 检测结果列表
     * @param imgWidth      原始图像宽度 (用于坐标缩放)
     * @param imgHeight     原始图像高度
     * @param confThreshold 置信度阈值
     * @param iouThreshold  NMS 阈值
     */
    virtual void infer(const std::vector<float>& input,
                       std::vector<Detection>& detections,
                       int imgWidth, int imgHeight,
                       float confThreshold, float iouThreshold) = 0;
};

#endif // INFERENCE_ENGINE_HPP
