#ifndef IENGINE_H
#define IENGINE_H

#include "opencv2/core/core.hpp"
#include "postprocess.h"

/**
 * IEngine — 推理引擎抽象接口
 *
 * 所有模型引擎（YOLOv5、YOLO11 等）实现此接口，
 * 支持运行时多态调用和统一的 CascadePipeline 编排。
 */
class IEngine {
public:
    virtual ~IEngine() = default;

    /* 初始化引擎（加载模型、分配资源） */
    virtual int init() = 0;

    /* 执行目标检测 */
    virtual int detect(const cv::Mat& frame, detect_result_group_t* out) = 0;

    /* 返回模型输入尺寸 */
    virtual int getInputWidth() const = 0;
    virtual int getInputHeight() const = 0;

    /* 动态设置阈值 */
    virtual void setThresholds(float conf, float nms) = 0;
};

#endif
