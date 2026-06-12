#ifndef VIDEO_WEBSOCKET_SERVER_HPP
#define VIDEO_WEBSOCKET_SERVER_HPP

#include <QString>
#include <QByteArray>
#include <QImage>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QList>
#include <QMutex>
#include <chrono>
#include <functional>
#include "output_sink.hpp"

class VideoWebSocketServer : public IOutputSink {
public:
    VideoWebSocketServer();
    ~VideoWebSocketServer();

    void start(quint16 port);
    void stop();
    bool isRunning() const;
    int clientCount() const;
    void onFrame(const FrameData& data) override;
    void setLogCallback(std::function<void(const QString&, const QString&)> callback);

private:
    void onNewConnection();
    void onClientDisconnected(QWebSocket* socket);
    void pushFrame(const QByteArray& jpegData);

    QObject* connCtx_;
    QWebSocketServer* server_ = nullptr;
    QList<QWebSocket*> clients_;
    mutable QMutex mutex_;
    std::chrono::steady_clock::time_point lastFrameTime_;
    std::function<void(const QString&, const QString&)> logCallback_;
};

#endif // VIDEO_WEBSOCKET_SERVER_HPP
