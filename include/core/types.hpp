/**
 * @file types.hpp
 * @brief 统一 YOLO 检测平台 — 核心数据结构定义
 *
 * 所有模块通过此文件共享通用数据结构，降低耦合。
 */

#ifndef TYPES_HPP
#define TYPES_HPP

#include <vector>
#include <string>
#include <cstdio>
#include <QMetaType>

// 单个检测结果. 坐标系统以原图左上角为原点(0,0), 单位为像素
struct Detection {
    float x;       // 检测框左上角X坐标(像素)
    float y;       // 检测框左上角Y坐标(像素)
    float w;       // 检测框宽度(像素)
    float h;       // 检测框高度(像素)
    float conf;    // 置信度(0.0~1.0), 值越高表示检测结果越可信
    int class_id;  // 类别索引(对应类别名称列表中的索引)

    // 便捷方法: 获取右下角坐标
    float x2() const { return x + w; }
    float y2() const { return y + h; }

    // 便捷方法: 获取检测框面积
    float area() const { return w * h; }
};

// 摄像头信息
struct CameraInfo {
    int id;              // 摄像头ID
    std::string name;    // 摄像头名称
    std::string url;     // RTSP URL 或 文件路径
    std::string type;    // "rtsp", "file", "webcam"
    bool active;         // 是否正在推流

    CameraInfo() : id(0), active(false) {}
    CameraInfo(int i, const std::string& n, const std::string& u, const std::string& t)
        : id(i), name(n), url(u), type(t), active(false) {}
};

// 报警信息
struct AlertInfo {
    int cameraId;
    int classId;
    std::string className;
    float confidence;
    std::string timestamp;
    std::string imagePath;     // 触发帧截图路径
    std::string videoPath;     // 报警视频路径
};

// 注册 Detection 和向量为 Qt 元类型，支持跨线程信号槽传递
Q_DECLARE_METATYPE(Detection)
Q_DECLARE_METATYPE(std::vector<Detection>)

// 将检测结果格式化为字符串(调试用)
inline std::string detectionToString(const Detection& det) {
    char buf[256];
    snprintf(buf, sizeof(buf),
             "[class=%d, conf=%.3f, box=(%.1f,%.1f,%.1f,%.1f)]",
             det.class_id, det.conf, det.x, det.y, det.w, det.h);
    return std::string(buf);
}

#endif // TYPES_HPP
