/**
 * @file websocket_manager.cpp
 * @brief WebSocket 控制通道实现 (端口 9090)
 */

#include "websocket_manager.hpp"
#include "core/config.hpp"
#include "core/runtime_config.hpp"
#include <QHostAddress>
#include <QObject>
#include <QDateTime>

WebSocketManager::WebSocketManager()
    : connCtx_(new QObject())
{
}

WebSocketManager::~WebSocketManager() {
    stop();
    delete connCtx_;
}

void WebSocketManager::start(quint16 port) {
    quint16 actualPort = (port == 0) ? static_cast<quint16>(Config::WEBSOCKET_PORT) : port;

    stop();

    port_ = actualPort;
    server_ = new QWebSocketServer("YOLO11-Control", QWebSocketServer::NonSecureMode, connCtx_);

    if (!server_->listen(QHostAddress::Any, port_)) {
        if (logCallback_) {
            logCallback_("WS", QString("启动失败, 端口: %1").arg(port_));
        }
        return;
    }

    QObject::connect(server_, &QWebSocketServer::newConnection, connCtx_, [this]() {
        onNewConnection();
    });

    if (logCallback_) {
        logCallback_("WS", QString("服务已启动 ws://0.0.0.0:%1").arg(port_));
    }
}

void WebSocketManager::stop() {
    // 先停止服务器, 不再接受新连接
    if (server_) {
        server_->close();
        delete server_;
        server_ = nullptr;
    }

    // 关闭所有客户端连接 (释放锁后再关闭, 避免 deadlock)
    QList<QWebSocket*> toClose;
    {
        QMutexLocker locker(&mutex_);
        toClose = clients_;
        clients_.clear();
    }
    for (auto* client : toClose) {
        client->close();
        client->deleteLater();
    }
}

bool WebSocketManager::isRunning() const {
    return server_ && server_->isListening();
}

int WebSocketManager::clientCount() const {
    QMutexLocker locker(&mutex_);
    return clients_.size();
}

void WebSocketManager::setLogCallback(std::function<void(const QString&, const QString&)> callback) {
    QMutexLocker locker(&mutex_);
    logCallback_ = std::move(callback);
}

void WebSocketManager::setGetStreamsCallback(std::function<QList<StreamInfo>()> callback) {
    QMutexLocker locker(&mutex_);
    getStreamsCallback_ = std::move(callback);
}

void WebSocketManager::setViewStreamCallback(std::function<QString(const QString&)> callback) {
    QMutexLocker locker(&mutex_);
    viewStreamCallback_ = std::move(callback);
}

// ============================================================
// 消息广播
// ============================================================

void WebSocketManager::broadcast(const QString& message) {
    QMutexLocker locker(&mutex_);
    for (auto* client : clients_) {
        if (client && client->state() == QAbstractSocket::ConnectedState) {
            client->sendTextMessage(message);
        }
    }
}

// ============================================================
// 告警推送
// ============================================================

void WebSocketManager::pushAlarm(const QString& alarmJson) {
    QMutexLocker locker(&mutex_);
    if (clients_.isEmpty()) {
        if (logCallback_) {
            logCallback_("WS", "告警推送: 无客户端连接, 丢弃");
        }
        return;
    }
    int sentCount = 0;
    for (auto* client : clients_) {
        if (client && client->state() == QAbstractSocket::ConnectedState) {
            client->sendTextMessage(alarmJson);
            sentCount++;
        }
    }
    if (logCallback_) {
        logCallback_("WS", QString("告警已推送到 %1 个客户端").arg(sentCount));
    }
}

// ============================================================
// 围栏管理
// ============================================================

void WebSocketManager::setFence(const QString& streamId, const FenceRegion& fence) {
    QMutexLocker locker(&mutex_);
    fenceRegions_[streamId] = fence;

    if (logCallback_) {
        logCallback_("WS", QString("设置围栏: stream_id=%1, 区域=(%.2f,%.2f,%.2f,%.2f)")
            .arg(streamId).arg(fence.x1).arg(fence.y1).arg(fence.x2).arg(fence.y2));
    }
}

FenceRegion WebSocketManager::getFence(const QString& streamId) const {
    QMutexLocker locker(&mutex_);
    auto it = fenceRegions_.find(streamId);
    if (it != fenceRegions_.end()) {
        return it.value();
    }
    return FenceRegion{};
}

bool WebSocketManager::hasFence(const QString& streamId) const {
    QMutexLocker locker(&mutex_);
    return fenceRegions_.contains(streamId);
}

// ============================================================
// 客户端管理
// ============================================================

void WebSocketManager::replyToClient(QWebSocket* client, const QJsonObject& response) {
    if (client) {
        client->sendTextMessage(QJsonDocument(response).toJson(QJsonDocument::Compact));
    }
}

// ============================================================
// 内部处理
// ============================================================

void WebSocketManager::onNewConnection() {
    auto* socket = server_->nextPendingConnection();
    if (!socket) return;

    QString peerIp = socket->peerAddress().toString();
    quint16 peerPort = socket->peerPort();

    {
        QMutexLocker locker(&mutex_);
        clients_.append(socket);
    }

    QObject::connect(socket, &QWebSocket::textMessageReceived, connCtx_, [this, socket](const QString& message) {
        onTextMessageForClient(socket, message);
    });

    QObject::connect(socket, &QWebSocket::disconnected, connCtx_, [this, socket]() {
        onClientDisconnected(socket);
    });

    if (logCallback_) {
        QMutexLocker locker(&mutex_);
        logCallback_("WS", QString("客户端已连接 [%1:%2], 当前连接数: %3").arg(peerIp).arg(peerPort).arg(clients_.size()));
    }
}

void WebSocketManager::onTextMessageForClient(QWebSocket* client, const QString& message) {
    if (logCallback_) {
        logCallback_("WS", QString("收到消息: %1").arg(message.left(100)));
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isNull()) {
        if (logCallback_) {
            logCallback_("WS", "消息解析失败: 无效JSON");
        }
        return;
    }

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    // 处理 ping 心跳
    if (type == "ping") {
        handlePing(client);
        return;
    }

    // 处理 get_streams 获取摄像头列表
    if (type == "get_streams") {
        handleGetStreams(client);
        return;
    }

    // 处理 set_fence 围栏设置
    if (type == "set_fence") {
        QString streamId = obj["stream_id"].toString();
        QJsonObject fence = obj["fence"].toObject();
        handleSetFence(client, streamId, fence);
        return;
    }

    // 处理 view_stream 查看实时视频
    if (type == "view_stream") {
        QString streamId = obj["stream_id"].toString();
        handleViewStream(client, streamId);
        return;
    }

    // 处理告警 ACK
    if (type == "ack") {
        QString alarmId = obj["alarm_id"].toString();
        if (logCallback_) {
            logCallback_("WS", QString("收到告警ACK: %1").arg(alarmId));
        }
        return;
    }
}

void WebSocketManager::handlePing(QWebSocket* client) {
    QJsonObject pong;
    pong["type"] = "pong";
    
    if (client && client->state() == QAbstractSocket::ConnectedState) {
        client->sendTextMessage(QJsonDocument(pong).toJson(QJsonDocument::Compact));
        if (logCallback_) {
            logCallback_("WS", "收到 ping, 已回复 pong");
        }
    } else {
        if (logCallback_) {
            logCallback_("WS", "收到 ping, 但客户端已断开, 无法回复");
        }
    }
}

void WebSocketManager::handleGetStreams(QWebSocket* client) {
    QJsonArray streamsArray;

    if (getStreamsCallback_) {
        auto streams = getStreamsCallback_();
        for (const auto& stream : streams) {
            QJsonObject s;
            s["stream_id"] = stream.streamId;
            s["name"] = stream.name;
            s["url"] = stream.url;
            streamsArray.append(s);
        }
    }

    QJsonObject response;
    response["type"] = "streams_list";
    response["data"] = streamsArray;

    replyToClient(client, response);

    if (logCallback_) {
        logCallback_("WS", QString("响应摄像头列表: %1 个流").arg(streamsArray.size()));
    }
}

void WebSocketManager::handleSetFence(QWebSocket* client, const QString& streamId, const QJsonObject& fence) {
    float x1 = fence["x1"].toDouble(0.0);
    float y1 = fence["y1"].toDouble(0.0);
    float x2 = fence["x2"].toDouble(1.0);
    float y2 = fence["y2"].toDouble(1.0);

    FenceRegion region;
    region.x1 = x1;
    region.y1 = y1;
    region.x2 = x2;
    region.y2 = y2;

    setFence(streamId, region);

    QJsonObject response;
    response["type"] = "fence_set";
    response["stream_id"] = streamId;
    response["status"] = "success";

    replyToClient(client, response);
}

void WebSocketManager::handleViewStream(QWebSocket* client, const QString& streamId) {
    QString streamUrl;

    if (viewStreamCallback_) {
        streamUrl = viewStreamCallback_(streamId);
    }

    if (streamUrl.isEmpty()) {
        QJsonObject error;
        error["type"] = "stream_error";
        error["stream_id"] = streamId;
        error["message"] = "摄像头不存在或流不可用";

        replyToClient(client, error);
        return;
    }

    QJsonObject response;
    response["type"] = "stream_url";
    response["stream_id"] = streamId;
    response["url"] = streamUrl;
    response["message"] = "请使用WebSocket客户端连接此地址查看实时视频";

    replyToClient(client, response);

    if (logCallback_) {
        logCallback_("WS", QString("请求查看摄像头%1的实时视频").arg(streamId));
    }
}

void WebSocketManager::onClientDisconnected(QWebSocket* socket) {
    {
        QMutexLocker locker(&mutex_);
        clients_.removeAll(socket);
    }
    socket->deleteLater();

    if (logCallback_) {
        QMutexLocker locker(&mutex_);
        logCallback_("WS", QString("客户端断开连接, 剩余: %1").arg(clients_.size()));
    }
}
