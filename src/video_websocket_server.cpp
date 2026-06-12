#include "video_websocket_server.hpp"
#include <QObject>
#include <QBuffer>
#include <QHostAddress>
#include <QtGlobal>

VideoWebSocketServer::VideoWebSocketServer()
    : connCtx_(new QObject())
    , lastFrameTime_(std::chrono::steady_clock::now())
{
}

VideoWebSocketServer::~VideoWebSocketServer() {
    stop();
    delete connCtx_;
}

void VideoWebSocketServer::start(quint16 port) {
    stop();

    server_ = new QWebSocketServer("YOLO11-VideoStream", QWebSocketServer::NonSecureMode, connCtx_);
    if (!server_->listen(QHostAddress::Any, port)) {
        if (logCallback_) {
            logCallback_(QStringLiteral("视频流"), QStringLiteral("服务启动失败, 端口: %1").arg(port));
        }
        delete server_;
        server_ = nullptr;
        return;
    }

    QObject::connect(server_, &QWebSocketServer::newConnection, connCtx_, [this]() {
        onNewConnection();
    });

    if (logCallback_) {
        logCallback_(QStringLiteral("视频流"), QStringLiteral("服务已启动 ws://0.0.0.0:%1").arg(port));
    }
}

void VideoWebSocketServer::stop() {
    if (!server_) return;

    // 先停止服务器
    server_->close();
    delete server_;
    server_ = nullptr;

    // 关闭所有客户端 (释放锁后关闭, 避免 deadlock)
    QList<QWebSocket*> toClose;
    {
        QMutexLocker locker(&mutex_);
        toClose = clients_;
        clients_.clear();
    }
    for (auto* client : toClose) {
        if (client) {
            client->close();
            client->deleteLater();
        }
    }
}

bool VideoWebSocketServer::isRunning() const {
    return server_ && server_->isListening();
}

int VideoWebSocketServer::clientCount() const {
    QMutexLocker locker(&mutex_);
    return clients_.size();
}

void VideoWebSocketServer::onFrame(const FrameData& data) {
    if (data.image.isNull()) {
        if (logCallback_) {
            logCallback_(QStringLiteral("视频流"), QStringLiteral("收到空帧, 跳过"));
        }
        return;
    }

    QByteArray jpegData;
    QBuffer buf(&jpegData);
    buf.open(QIODevice::WriteOnly);
    if (!data.image.save(&buf, "JPEG", 60)) {
        if (logCallback_) {
            logCallback_(QStringLiteral("视频流"), QStringLiteral("JPEG 编码失败"));
        }
        return;
    }
    pushFrame(jpegData);
}

void VideoWebSocketServer::pushFrame(const QByteArray& jpegData) {
    if (jpegData.isEmpty()) {
        if (logCallback_) {
            logCallback_(QStringLiteral("视频流"), QStringLiteral("JPEG 数据为空, 跳过"));
        }
        return;
    }

    QMutexLocker locker(&mutex_);
    if (clients_.isEmpty()) return;

    // 帧率限制 ~30fps
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameTime_).count();
    if (elapsed < 33) return;
    lastFrameTime_ = now;

    // 发送二进制帧 (JPEG 原始数据)
    int sentCount = 0;
    for (auto* client : clients_) {
        if (client && client->state() == QAbstractSocket::ConnectedState) {
            client->sendBinaryMessage(jpegData);
            sentCount++;
        }
    }

    // 每60帧 (~2秒) 输出一次日志
    static int frameCounter = 0;
    if (++frameCounter % 60 == 0 && logCallback_) {
        logCallback_(QStringLiteral("视频流"), QStringLiteral("推送帧 %1 bytes 到 %2 个客户端").arg(jpegData.size()).arg(sentCount));
    }
}

void VideoWebSocketServer::setLogCallback(std::function<void(const QString&, const QString&)> callback) {
    logCallback_ = std::move(callback);
}

void VideoWebSocketServer::onNewConnection() {
    auto* socket = server_->nextPendingConnection();
    if (!socket) return;

    {
        QMutexLocker locker(&mutex_);
        clients_.append(socket);
    }

    QObject::connect(socket, &QWebSocket::disconnected, connCtx_, [this, socket]() {
        onClientDisconnected(socket);
    });

    if (logCallback_) {
        logCallback_(QStringLiteral("视频流"), QStringLiteral("新客户端已连接"));
    }
}

void VideoWebSocketServer::onClientDisconnected(QWebSocket* socket) {
    if (!socket) return;

    {
        QMutexLocker locker(&mutex_);
        clients_.removeAll(socket);
    }

    if (logCallback_) {
        logCallback_(QStringLiteral("视频流"), QStringLiteral("客户端已断开"));
    }

    socket->deleteLater();
}
