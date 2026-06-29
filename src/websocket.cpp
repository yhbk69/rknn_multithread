#include "websocket.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHostAddress>

#include "Logger.hpp"

WebSocket::WebSocket(QObject *parent)
    : QObject(parent)
{
}

WebSocket::~WebSocket()
{
    shutdown();
}

int WebSocket::init(const WebSocketConfig &config)
{
    QMutexLocker lock(&config_mtx_);
    config_ = config;

    if (ws_server_) {
        ws_server_->close();
        ws_server_->deleteLater();
        ws_server_ = nullptr;
    }

    ws_server_ = new QWebSocketServer(
        QStringLiteral("YOLO Detection Server"),
        QWebSocketServer::NonSecureMode,
        this);

    QHostAddress addr(config_.server_host);
    if (!ws_server_->listen(addr, static_cast<quint16>(config_.server_port)))
    {
        QString err = ws_server_->errorString();
        LOG_ERROR("[WebSocket]", "Server failed to start: %s",
                err.toUtf8().constData());
        emit serverError(err);
        return -1;
    }

    connect(ws_server_, &QWebSocketServer::newConnection,
            this, &WebSocket::onNewConnection);
    connect(ws_server_, &QWebSocketServer::closed,
            this, [this]() {
                LOG_INFO("[WebSocket]", "Server closed.");
                emit serverClosed();
            });

    LOG_INFO("[WebSocket]", "Server started on %s:%d",
           config_.server_host.toUtf8().constData(),
           ws_server_->serverPort());
    emit serverStarted(ws_server_->serverPort());
    return 0;
}

void WebSocket::shutdown()
{
    if (!ws_server_)
        return;

    {
        QMutexLocker lock(&clients_mtx_);
        for (QWebSocket *client : clients_)
        {
            client->abort();
            client->deleteLater();
        }
        clients_.clear();
    }

    ws_server_->close();
    ws_server_->deleteLater();
    ws_server_ = nullptr;
    LOG_INFO("[WebSocket]", "Server shutdown complete.");
}

int WebSocket::clientCount() const
{
    QMutexLocker lock(&clients_mtx_);
    return clients_.size();
}

bool WebSocket::isListening() const
{
    return ws_server_ && ws_server_->isListening();
}

bool WebSocket::isAlarmEnabled() const
{
    QMutexLocker lock(&config_mtx_);
    return config_.enable_alarm;
}

void WebSocket::broadcast(const QString &message)
{
    QMutexLocker lock(&clients_mtx_);
    for (QWebSocket *client : clients_)
    {
        if (client->isValid())
            client->sendTextMessage(message);
    }
}

void WebSocket::broadcastBinary(const QByteArray &data)
{
    QMutexLocker lock(&clients_mtx_);
    for (QWebSocket *client : clients_)
    {
        if (client->isValid())
            client->sendBinaryMessage(data);
    }
}

void WebSocket::sendToClient(QWebSocket *client, const QString &message)
{
    if (client && client->isValid())
        client->sendTextMessage(message);
}

void WebSocket::addStream(const StreamInfo &stream)
{
    QMutexLocker lock(&streams_mtx_);
    for (int i = 0; i < streams_.size(); i++)
    {
        if (streams_[i].stream_id == stream.stream_id)
        {
            streams_[i] = stream;
            return;
        }
    }
    streams_.append(stream);
}

void WebSocket::removeStream(const QString &streamId)
{
    QMutexLocker lock(&streams_mtx_);
    for (int i = streams_.size() - 1; i >= 0; i--)
    {
        if (streams_[i].stream_id == streamId)
            streams_.removeAt(i);
    }
}

QList<StreamInfo> WebSocket::streams() const
{
    QMutexLocker lock(&streams_mtx_);
    return streams_;
}

void WebSocket::setFence(const FenceRegion &fence)
{
    QMutexLocker lock(&fences_mtx_);
    for (int i = 0; i < fences_.size(); i++)
    {
        if (fences_[i].stream_id == fence.stream_id)
        {
            fences_[i] = fence;
            return;
        }
    }
    fences_.append(fence);
}

QList<FenceRegion> WebSocket::fences() const
{
    QMutexLocker lock(&fences_mtx_);
    return fences_;
}

void WebSocket::updateConfig(const WebSocketConfig &config)
{
    QMutexLocker lock(&config_mtx_);
    config_ = config;
}

WebSocketConfig WebSocket::config() const
{
    QMutexLocker lock(&config_mtx_);
    return config_;
}

void WebSocket::onNewConnection()
{
    QWebSocket *client = ws_server_->nextPendingConnection();
    if (!client)
        return;

    LOG_INFO("[WebSocket]", "Client connected: %s",
           client->peerAddress().toString().toUtf8().constData());

    connect(client, &QWebSocket::textMessageReceived,
            this, &WebSocket::onTextMessageReceived);
    connect(client, &QWebSocket::binaryMessageReceived,
            this, &WebSocket::onBinaryMessageReceived);
    connect(client, &QWebSocket::disconnected,
            this, &WebSocket::onSocketDisconnected);

    {
        QMutexLocker lock(&clients_mtx_);
        clients_.append(client);
    }

    emit clientConnected(client);
}

void WebSocket::onSocketDisconnected()
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (!client)
        return;

    LOG_INFO("[WebSocket]", "Client disconnected: %s",
           client->peerAddress().toString().toUtf8().constData());

    removeClient(client);
    emit clientDisconnected(client);
    client->deleteLater();
}

void WebSocket::onTextMessageReceived(const QString &message)
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (!client)
        return;

    emit textMessageReceived(client, message);

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        LOG_WARN("[WebSocket]", "Invalid JSON from client: %s",
               message.left(100).toUtf8().constData());
        return;
    }

    dispatchMessage(client, doc.object());
}

void WebSocket::onBinaryMessageReceived(const QByteArray &data)
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (client)
        emit binaryMessageReceived(client, data);
}

void WebSocket::dispatchMessage(QWebSocket *client, const QJsonObject &json)
{
    QString type = json["type"].toString();

    if (type == "ping")
        handlePing(client);
    else if (type == "ack")
        handleAck(client, json);
    else if (type == "get_streams")
        handleGetStreams(client);
    else if (type == "set_fence")
        handleSetFence(client, json);
    else if (type == "get_stats")
        handleGetStats(client);
    else if (type == "switch_model")
        handleSwitchModel(client, json);
    else if (type == "get_models")
        handleGetModels(client);
    else if (type == "set_threshold")
        handleSetThreshold(client, json);
    else
    {
        QJsonObject err;
        err["type"] = "error";
        err["message"] = "Unknown message type: " + type;
        client->sendTextMessage(QString::fromUtf8(
            QJsonDocument(err).toJson(QJsonDocument::Compact)));
    }
}

void WebSocket::removeClient(QWebSocket *client)
{
    QMutexLocker lock(&clients_mtx_);
    clients_.removeAll(client);
}

QJsonObject WebSocket::streamToJson(const StreamInfo &s) const
{
    QJsonObject obj;
    obj["stream_id"] = s.stream_id;
    obj["name"] = s.name;
    obj["url"] = s.url;
    return obj;
}

QJsonObject WebSocket::fenceToJson(const FenceRegion &f) const
{
    QJsonObject obj;
    obj["stream_id"] = f.stream_id;
    QJsonObject rect;
    rect["x1"] = f.x1;
    rect["y1"] = f.y1;
    rect["x2"] = f.x2;
    rect["y2"] = f.y2;
    obj["fence"] = rect;
    return obj;
}

void WebSocket::broadcastStats(const nlohmann::json& stats)
{
    if (!ws_server_) return;

    {
        QMutexLocker clientLock(&clients_mtx_);
        if (clients_.isEmpty()) return;
    }

    QJsonObject data = QJsonDocument::fromJson(
        QByteArray::fromStdString(stats.dump())).object();

    QJsonObject msg;
    msg["type"] = "stats";
    msg["data"] = data;

    QString json_str = QString::fromUtf8(
        QJsonDocument(msg).toJson(QJsonDocument::Compact));

    QMutexLocker clientLock(&clients_mtx_);
    for (QWebSocket *client : clients_)
    {
        if (client->isValid())
            client->sendTextMessage(json_str);
    }
}
