/**
 * @file mjpeg_streamer.hpp
 * @brief MJPEG 推流服务模块
 *
 * 独立的 MJPEG over HTTP 推流服务,支持多客户端同时观看。
 * 客户端通过浏览器访问 http://host:port/stream 即可观看实时视频。
 *
 * 设计要点:
 * - 多线程安全: 客户端列表用 mutex 保护
 * - 帧率控制: 自动限制约 30fps,避免带宽浪费
 * - CORS 支持: 允许跨域访问
 * - 不继承 QObject: 避免 MOC 复杂化,用 lambda 回调代替信号槽
 */

#ifndef MJPEG_STREAMER_HPP
#define MJPEG_STREAMER_HPP

#include <QString>
#include <QByteArray>
#include <QImage>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMutex>
#include <QList>
#include <chrono>
#include <functional>
#include "output_sink.hpp"

class MjpegStreamer : public IOutputSink {
public:
    MjpegStreamer();
    ~MjpegStreamer();

    void start(quint16 port, const QString& hostIp);
    void pushFrame(const QByteArray& jpegData);
    void pushImage(const QImage& image, int quality = 60);
    void onFrame(const FrameData& data) override;
    void stop();
    int clientCount() const;
    void setLogCallback(std::function<void(const QString&, const QString&)> callback);

private:
    void onNewConnection();
    void onClientDisconnected(QTcpSocket* socket);

    QObject* connCtx_;          // 信号槽上下文(自动管理连接生命周期)
    QTcpServer* server_ = nullptr;
    QList<QTcpSocket*> clients_;
    QMutex clientsMutex_;
    std::chrono::steady_clock::time_point lastFrameTime_;
    std::function<void(const QString&, const QString&)> logCallback_;
};

#endif // MJPEG_STREAMER_HPP
