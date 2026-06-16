/*
 * websocket.cpp - WebSocket 通信服务模块实现
 *
 * 协议格式：
 *   报警: {"type":"alarm","data":{"alarm_id":"...","alarm_type":"...","timestamp":...,"video_url":"...","image_url":"..."}}
 *   确认: {"type":"ack","alarm_id":"..."}
 *   心跳: {"type":"ping"} -> {"type":"pong"}
 *   流列表: {"type":"get_streams"} -> {"type":"streams_list","data":[...]}
 *   围栏: {"type":"set_fence","stream_id":"...","fence":{...}}
 */

#include "websocket.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHostAddress>
#include <cstdio>

/* ============================================================
 * 构造 / 析构
 * ============================================================ */

WebSocket::WebSocket(QObject *parent)
    : QObject(parent)
{
}

WebSocket::~WebSocket()
{
    shutdown();
}

/* ============================================================
 * init - 初始化 WebSocket 服务器
 * ============================================================ */

int WebSocket::init(const WebSocketConfig &config)
{
    QMutexLocker lock(&config_mtx_);
    config_ = config;

    ws_server_ = new QWebSocketServer(
        QStringLiteral("YOLO Detection Server"),
        QWebSocketServer::NonSecureMode,
        this);

    QHostAddress addr(config_.server_host);
    if (!ws_server_->listen(addr, static_cast<quint16>(config_.server_port)))
    {
        QString err = ws_server_->errorString();
        fprintf(stderr, "[WebSocket] Server failed to start: %s\n",
                err.toUtf8().constData());
        emit serverError(err);
        return -1;
    }

    connect(ws_server_, &QWebSocketServer::newConnection,
            this, &WebSocket::onNewConnection);
    connect(ws_server_, &QWebSocketServer::closed,
            this, [this]() {
                printf("[WebSocket] Server closed.\n");
                emit serverClosed();
            });

    printf("[WebSocket] Server started on %s:%d\n",
           config_.server_host.toUtf8().constData(),
           ws_server_->serverPort());
    emit serverStarted(ws_server_->serverPort());
    return 0;
}

/* ============================================================
 * shutdown - 关闭服务器
 * ============================================================ */

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
    printf("[WebSocket] Server shutdown complete.\n");
}

/* ============================================================
 * clientCount / isListening
 * ============================================================ */

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

/* ============================================================
 * broadcast / broadcastBinary / sendToClient
 * ============================================================ */

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

/* ============================================================
 * checkAndAlarm - 检查检测结果并广播报警
 *
 * 协议格式：
 * {"type":"alarm","data":{"alarm_id":"a_001","alarm_type":"person",
 *  "timestamp":1700000000000,"video_url":"","image_url":""}}
 * ============================================================ */

int WebSocket::checkAndAlarm(const detect_result_group_t *detect_results, int frame_id)
{
    if (!ws_server_ || !detect_results)
        return 0;

    int alarm_count = 0;

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    long long now_ms = tv.tv_sec * 1000LL + tv.tv_usec / 1000;

    QMutexLocker configLock(&config_mtx_);

    for (int i = 0; i < detect_results->count; i++)
    {
        const detect_result_t &det = detect_results->results[i];

        if (det.name[0] == '\0')
            continue;

        std::string name(det.name);
        if (config_.alarm_class_names.count(name) == 0)
            continue;

        /* 速率限制：同一类别 1 秒内最多报警 1 次 */
        auto it = last_alarm_time_.find(name);
        if (it != last_alarm_time_.end() && (now_ms - it->second) < 1000)
            continue;
        last_alarm_time_[name] = now_ms;

        /* 构造报警消息 */
        QString alarmId = generateAlarmId();

        QJsonObject data;
        data["alarm_id"] = alarmId;
        data["alarm_type"] = QString::fromUtf8(det.name);
        data["confidence"] = QString("%1%").arg(det.prop * 100.0f, 0, 'f', 1);
        data["timestamp"] = static_cast<qint64>(now_ms);
        data["video_url"] = "";
        data["image_url"] = "";

        QJsonObject msg;
        msg["type"] = "alarm";
        msg["data"] = data;

        QString json = QString::fromUtf8(
            QJsonDocument(msg).toJson(QJsonDocument::Compact));

        /* 广播 */
        {
            QMutexLocker clientLock(&clients_mtx_);
            for (QWebSocket *client : clients_)
            {
                if (client->isValid())
                    client->sendTextMessage(json);
            }
        }

        alarm_count++;
        printf("[WebSocket] ALARM [%s]: %s at frame %d\n",
               alarmId.toUtf8().constData(), det.name, frame_id);

        emit alarmTriggered(alarmId, QString::fromUtf8(det.name),
                            frame_id, now_ms);
    }

    return alarm_count;
}

/* ============================================================
 * sendAlarm - 手动发送报警
 * ============================================================ */

void WebSocket::sendAlarm(const QString &alarmType,
                          const QString &videoUrl,
                          const QString &imageUrl,
                          float confidence)
{
    if (!ws_server_)
        return;

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    long long now_ms = tv.tv_sec * 1000LL + tv.tv_usec / 1000;

    QString alarmId = generateAlarmId();

    QJsonObject data;
    data["alarm_id"] = alarmId;
    data["alarm_type"] = alarmType;
    data["confidence"] = QString("%1%").arg(confidence * 100.0f, 0, 'f', 1);
    data["timestamp"] = static_cast<qint64>(now_ms);
    data["video_url"] = videoUrl;
    data["image_url"] = imageUrl;

    QJsonObject msg;
    msg["type"] = "alarm";
    msg["data"] = data;

    QString json = QString::fromUtf8(
        QJsonDocument(msg).toJson(QJsonDocument::Compact));
    broadcast(json);

    printf("[WebSocket] MANUAL ALARM [%s]: %s\n",
           alarmId.toUtf8().constData(),
           alarmType.toUtf8().constData());
}

/* ============================================================
 * 流管理
 * ============================================================ */

void WebSocket::addStream(const StreamInfo &stream)
{
    QMutexLocker lock(&streams_mtx_);
    /* 检查是否已存在，存在则更新 */
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

/* ============================================================
 * 围栏管理
 * ============================================================ */

void WebSocket::setFence(const FenceRegion &fence)
{
    QMutexLocker lock(&fences_mtx_);
    /* 按 stream_id 更新或追加 */
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

/* ============================================================
 * 配置
 * ============================================================ */

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

/* ============================================================
 * onNewConnection
 * ============================================================ */

void WebSocket::onNewConnection()
{
    QWebSocket *client = ws_server_->nextPendingConnection();
    if (!client)
        return;

    printf("[WebSocket] Client connected: %s\n",
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

/* ============================================================
 * onSocketDisconnected
 * ============================================================ */

void WebSocket::onSocketDisconnected()
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (!client)
        return;

    printf("[WebSocket] Client disconnected: %s\n",
           client->peerAddress().toString().toUtf8().constData());

    removeClient(client);
    emit clientDisconnected(client);
    client->deleteLater();
}

/* ============================================================
 * onTextMessageReceived
 * ============================================================ */

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
        printf("[WebSocket] Invalid JSON from client: %s\n",
               message.left(100).toUtf8().constData());
        return;
    }

    dispatchMessage(client, doc.object());
}

/* ============================================================
 * onBinaryMessageReceived
 * ============================================================ */

void WebSocket::onBinaryMessageReceived(const QByteArray &data)
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (client)
        emit binaryMessageReceived(client, data);
}

/* ============================================================
 * dispatchMessage - 按 type 分发处理
 * ============================================================ */

void WebSocket::dispatchMessage(QWebSocket *client, const QJsonObject &json)
{
    QString type = json["type"].toString();

    if (type == "ping")
    {
        handlePing(client);
    }
    else if (type == "ack")
    {
        handleAck(client, json);
    }
    else if (type == "get_streams")
    {
        handleGetStreams(client);
    }
    else if (type == "set_fence")
    {
        handleSetFence(client, json);
    }
    /*
     * TODO: 用户扩展其他消息类型
     *
     *   - "set_threshold"    : 动态设置检测阈值
     *   - "get_detections"   : 获取当前帧检测结果
     *   - "start_recording"  : 开始录制
     *   - "stop_recording"   : 停止录制
     *   - "get_config"       : 获取完整配置
     *   - "set_config"       : 更新配置
     */
    else
    {
        QJsonObject err;
        err["type"] = "error";
        err["message"] = "Unknown message type: " + type;
        client->sendTextMessage(QString::fromUtf8(
            QJsonDocument(err).toJson(QJsonDocument::Compact)));
    }
}

/* ============================================================
 * 协议处理方法
 * ============================================================ */

/* {"type":"ping"} -> {"type":"pong"} */
void WebSocket::handlePing(QWebSocket *client)
{
    QJsonObject pong;
    pong["type"] = "pong";
    client->sendTextMessage(QString::fromUtf8(
        QJsonDocument(pong).toJson(QJsonDocument::Compact)));
}

/* {"type":"ack","alarm_id":"..."} */
void WebSocket::handleAck(QWebSocket *client, const QJsonObject &json)
{
    QString alarmId = json["alarm_id"].toString();
    printf("[WebSocket] ACK from client for alarm: %s\n",
           alarmId.toUtf8().constData());
    emit alarmAcknowledged(client, alarmId);
}

/*
 * {"type":"get_streams"}
 * ->
 * {"type":"streams_list","data":[
 *   {"stream_id":"1","name":"摄像头1","url":"http://..."},
 *   {"stream_id":"2","name":"摄像头2","url":"http://..."}
 * ]}
 */
void WebSocket::handleGetStreams(QWebSocket *client)
{
    QMutexLocker lock(&streams_mtx_);

    QJsonArray arr;
    for (const auto &s : streams_)
        arr.append(streamToJson(s));

    QJsonObject resp;
    resp["type"] = "streams_list";
    resp["data"] = arr;

    client->sendTextMessage(QString::fromUtf8(
        QJsonDocument(resp).toJson(QJsonDocument::Compact)));
}

/*
 * {"type":"set_fence","stream_id":"1","fence":{"x1":0.1,"y1":0.2,"x2":0.8,"y2":0.9}}
 */
void WebSocket::handleSetFence(QWebSocket *client, const QJsonObject &json)
{
    FenceRegion fence;
    fence.stream_id = json["stream_id"].toString();

    QJsonObject f = json["fence"].toObject();
    fence.x1 = f["x1"].toDouble();
    fence.y1 = f["y1"].toDouble();
    fence.x2 = f["x2"].toDouble();
    fence.y2 = f["y2"].toDouble();

    setFence(fence);

    printf("[WebSocket] Fence set for stream %s: (%.2f,%.2f)-(%.2f,%.2f)\n",
           fence.stream_id.toUtf8().constData(),
           fence.x1, fence.y1, fence.x2, fence.y2);

    /* 回复确认 */
    QJsonObject resp;
    resp["type"] = "set_fence_ack";
    resp["stream_id"] = fence.stream_id;
    resp["success"] = true;
    client->sendTextMessage(QString::fromUtf8(
        QJsonDocument(resp).toJson(QJsonDocument::Compact)));

    emit fenceSet(fence.stream_id, fence);
}

/* ============================================================
 * 工具方法
 * ============================================================ */

QString WebSocket::generateAlarmId()
{
    FILE *fp = fopen("/proc/sys/kernel/random/uuid", "r");
    if (fp)
    {
        char buf[40] = {0};
        if (fread(buf, 1, 36, fp) == 36)
        {
            fclose(fp);
            return QString::fromLatin1(buf, 36);
        }
        fclose(fp);
    }
    /* fallback */
    alarm_counter_++;
    return QString("a_%1").arg(alarm_counter_, 6, 10, QChar('0'));
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
