/*
 * FrameDetections.hpp - 帧检测结果结构
 *
 * 用于 DetectThread → MainWindow 之间的检测结果传递。
 * 包含一帧图像中所有检测到的目标信息。
 */

#ifndef FRAME_DETECTIONS_HPP
#define FRAME_DETECTIONS_HPP

#include <QString>
#include <QVector>

/* 单帧的检测结果集合 */
struct FrameDetections {
    int frame_id = 0;  // 帧编号（与视频源帧号对应）

    /* 单个检测目标 */
    struct Det {
        QString class_name;   // 类别名称（如 "person", "helmet"）
        float confidence = 0; // 置信度（0.0~1.0）
        int left = 0, top = 0, right = 0, bottom = 0;  // 边界框坐标（像素）
    };

    QVector<Det> detections;  // 该帧所有检测目标
};

#endif // FRAME_DETECTIONS_HPP
