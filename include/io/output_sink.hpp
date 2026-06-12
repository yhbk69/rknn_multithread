/**
 * @file output_sink.hpp
 * @brief 输出通道统一抽象接口
 *
 * MJPEG 推流、WebSocket 告警、录像等输出通道均实现此接口，
 * 新增输出 (RTMP, SRT, 本地文件存档) 只需实现 IOutputSink 并注册到 sinks_。
 */

#ifndef OUTPUT_SINK_HPP
#define OUTPUT_SINK_HPP

#include <QImage>
#include <QString>
#include <vector>
#include "types.hpp"

struct FrameData {
    int cameraId;
    QImage image;
    std::vector<Detection> detections;
    double elapsedMs;
};

struct AlertData {
    int cameraId;
    QString alertJson;
    QString imagePath;   // 告警截图 JPEG 本地路径
    QString videoPath;   // 告警视频 MP4 本地路径
};

class IOutputSink {
public:
    virtual ~IOutputSink() = default;
    virtual void onFrame(const FrameData& /*data*/) {}
    virtual void onAlert(const AlertData& /*data*/) {}
};

#endif // OUTPUT_SINK_HPP
