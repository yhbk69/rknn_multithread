/*
 * websocket.cpp - WebSocket 通信服务实现
 *
 * 基于 Qt5WebSockets 的通用 WebSocket 服务器，支持：
 *   - 客户端连接管理（连接、断开、消息收发）
 *   - 报警消息推送（检测到指定类别时自动报警）
 *   - 应用层心跳（ping/pong）
 *   - 视频流列表查询（get_streams）
 *   - 围栏设置（set_fence）
 *   - 动态阈值调整（set_threshold）
 *   - 模型切换（switch_model/get_models）
 *   - 统计数据查询（get_stats）
 *
 * 线程安全：
 *   - 客户端列表使用 QMutex 保护
 *   - 配置使用 QMutex 保护
 *   - 通过 Qt 信号槽与主线程通信
 */

#include "websocket.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHostAddress>

#include "Logger.hpp"

/**
 * 构造函数
 * @param parent Qt 对象树父节点
 */
WebSocket::WebSocket(QObject *parent)
    : QObject(parent)
{
}

/**
 * 析构函数：关闭服务器并清理资源
 */
WebSocket::~WebSocket()
{
    shutdown();
}

/**
 * 初始化 WebSocket 服务器
 *
 * 流程：
 *   1. 关闭旧服务器（如果存在）
 *   2. 创建新的 QWebSocketServer
 *   3. 绑定地址和端口开始监听
 *   4. 连接信号槽（新连接、服务器关闭）
 *
 * @param config 服务器配置
 * @return 0 成功，-1 失败
 */
int WebSocket::init(const WebSocketConfig &config)
{
    QMutexLocker lock(&config_mtx_);
    config_ = config;

    // 关闭旧服务器（如果存在）
    if (ws_server_) {
        ws_server_->close();
        ws_server_->deleteLater();
        ws_server_ = nullptr;
    }

    // 创建新的 WebSocket 服务器（非安全模式，即 ws:// 而非 wss://）
    ws_server_ = new QWebSocketServer(
        QStringLiteral("YOLO Detection Server"),
        QWebSocketServer::NonSecureMode,
        this);

    // 绑定地址和端口开始监听
    QHostAddress addr(config_.server_host);
    if (!ws_server_->listen(addr, static_cast<quint16>(config_.server_port)))
    {
        QString err = ws_server_->errorString();
        LOG_ERROR("[WebSocket]", "Server failed to start: %s",
                err.toUtf8().constData());
        emit serverError(err);
        return -1;
    }

    // 连接信号槽
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

/**
 * 关闭 WebSocket 服务器
 *
 * 流程：
 *   1. 中断所有客户端连接
 *   2. 关闭服务器监听
 *   3. 释放资源
 */
void WebSocket::shutdown()
{
    if (!ws_server_)
        return;

    // 中断所有客户端连接
    {
        QMutexLocker lock(&clients_mtx_);
        for (QWebSocket *client : clients_)
        {
            client->abort();  // 立即关闭，不等待握手
            client->deleteLater();  // 延迟删除，避免悬空指针
        }
        clients_.clear();
    }

    // 关闭服务器
    ws_server_->close();
    ws_server_->deleteLater();
    ws_server_ = nullptr;
    LOG_INFO("[WebSocket]", "Server shutdown complete.");
}

/**
 * 获取当前连接的客户端数量
 */
int WebSocket::clientCount() const
{
    QMutexLocker lock(&clients_mtx_);
    return clients_.size();
}

/**
 * 检查服务器是否正在监听
 */
bool WebSocket::isListening() const
{
    return ws_server_ && ws_server_->isListening();
}

/**
 * 检查报警功能是否启用
 */
bool WebSocket::isAlarmEnabled() const
{
    QMutexLocker lock(&config_mtx_);
    return config_.enable_alarm;
}

/**
 * 广播文本消息给所有客户端
 * @param message JSON 格式的文本消息
 */
void WebSocket::broadcast(const QString &message)
{
    QMutexLocker lock(&clients_mtx_);
    for (QWebSocket *client : clients_)
    {
        if (client->isValid())
            client->sendTextMessage(message);
    }
}

/**
 * 广播二进制数据给所有客户端
 * @param data 二进制数据
 */
void WebSocket::broadcastBinary(const QByteArray &data)
{
    QMutexLocker lock(&clients_mtx_);
    for (QWebSocket *client : clients_)
    {
        if (client->isValid())
            client->sendBinaryMessage(data);
    }
}

/**
 * 向指定客户端发送消息
 * @param client  目标客户端
 * @param message 文本消息
 */
void WebSocket::sendToClient(QWebSocket *client, const QString &message)
{
    if (client && client->isValid())
        client->sendTextMessage(message);
}

/**
 * 添加或更新视频流信息
 * @param stream 视频流信息
 */
void WebSocket::addStream(const StreamInfo &stream)
{
    QMutexLocker lock(&streams_mtx_);
    // 查找是否已存在，存在则更新
    for (int i = 0; i < streams_.size(); i++)
    {
        if (streams_[i].stream_id == stream.stream_id)
        {
            streams_[i] = stream;
            return;
        }
    }
    // 不存在则添加
    streams_.append(stream);
}

/**
 * 移除视频流
 * @param streamId 流 ID
 */
void WebSocket::removeStream(const QString &streamId)
{
    QMutexLocker lock(&streams_mtx_);
    for (int i = streams_.size() - 1; i >= 0; i--)
    {
        if (streams_[i].stream_id == streamId)
            streams_.removeAt(i);
    }
}

/**
 * 获取所有视频流列表
 */
QList<StreamInfo> WebSocket::streams() const
{
    QMutexLocker lock(&streams_mtx_);
    return streams_;
}

/**
 * 设置围栏区域
 * @param fence 围栏信息（归一化坐标）
 */
void WebSocket::setFence(const FenceRegion &fence)
{
    QMutexLocker lock(&fences_mtx_);
    // 查找是否已存在，存在则更新
    for (int i = 0; i < fences_.size(); i++)
    {
        if (fences_[i].stream_id == fence.stream_id)
        {
            fences_[i] = fence;
            return;
        }
    }
    // 不存在则添加
    fences_.append(fence);
}

/**
 * 获取所有围栏区域
 */
QList<FenceRegion> WebSocket::fences() const
{
    QMutexLocker lock(&fences_mtx_);
    return fences_;
}

/**
 * 更新服务器配置
 */
void WebSocket::updateConfig(const WebSocketConfig &config)
{
    QMutexLocker lock(&config_mtx_);
    config_ = config;
}

/**
 * 获取当前配置（线程安全，返回拷贝）
 */
WebSocketConfig WebSocket::config() const
{
    QMutexLocker lock(&config_mtx_);
    return config_;
}

/**
 * 新客户端连接处理
 *
 * 流程：
 *   1. 获取新连接的 socket
 *   2. 连接信号槽（消息接收、断开连接）
 *   3. 加入客户端列表
 *   4. 发射 clientConnected 信号
 */
void WebSocket::onNewConnection()
{
    QWebSocket *client = ws_server_->nextPendingConnection();
    if (!client)
        return;

    LOG_INFO("[WebSocket]", "Client connected: %s",
           client->peerAddress().toString().toUtf8().constData());

    // 连接信号槽
    connect(client, &QWebSocket::textMessageReceived,
            this, &WebSocket::onTextMessageReceived);
    connect(client, &QWebSocket::binaryMessageReceived,
            this, &WebSocket::onBinaryMessageReceived);
    connect(client, &QWebSocket::disconnected,
            this, &WebSocket::onSocketDisconnected);

    // 加入客户端列表
    {
        QMutexLocker lock(&clients_mtx_);
        clients_.append(client);
    }

    emit clientConnected(client);
}

/**
 * 客户端断开连接处理
 */
void WebSocket::onSocketDisconnected()
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (!client)
        return;

    LOG_INFO("[WebSocket]", "Client disconnected: %s",
           client->peerAddress().toString().toUtf8().constData());

    removeClient(client);
    emit clientDisconnected(client);
    client->deleteLater();  // 延迟删除
}

/**
 * 收到文本消息处理
 *
 * 流程：
 *   1. 解析 JSON
 *   2. 调用 dispatchMessage 分发到对应的处理函数
 */
void WebSocket::onTextMessageReceived(const QString &message)
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (!client)
        return;

    // 发射信号（供外部监听原始消息）
    emit textMessageReceived(client, message);

    // 解析 JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        LOG_WARN("[WebSocket]", "Invalid JSON from client: %s",
               message.left(100).toUtf8().constData());
        return;
    }

    // 分发到对应的处理函数
    dispatchMessage(client, doc.object());
}

/**
 * 收到二进制消息处理（转发信号）
 */
void WebSocket::onBinaryMessageReceived(const QByteArray &data)
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());
    if (client)
        emit binaryMessageReceived(client, data);
}

/**
 * 消息分发：根据 type 字段调用对应的处理函数
 *
 * 支持的消息类型：
 *   - ping: 心跳检测
 *   - ack: 报警确认
 *   - get_streams: 获取视频流列表
 *   - set_fence: 设置围栏
 *   - get_stats: 获取统计数据
 *   - switch_model: 切换模型
 *   - get_models: 获取模型列表
 *   - set_threshold: 动态设置阈值
 */
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
        // 未知消息类型，返回错误
        QJsonObject err;
        err["type"] = "error";
        err["message"] = "Unknown message type: " + type;
        client->sendTextMessage(QString::fromUtf8(
            QJsonDocument(err).toJson(QJsonDocument::Compact)));
    }
}

/**
 * 从客户端列表中移除指定客户端
 */
void WebSocket::removeClient(QWebSocket *client)
{
    QMutexLocker lock(&clients_mtx_);
    clients_.removeAll(client);
}

/**
 * 将 StreamInfo 转换为 JSON 对象
 */
QJsonObject WebSocket::streamToJson(const StreamInfo &s) const
{
    QJsonObject obj;
    obj["stream_id"] = s.stream_id;
    obj["name"] = s.name;
    obj["url"] = s.url;
    return obj;
}

/**
 * 将 FenceRegion 转换为 JSON 对象
 */
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

/**
 * 广播统计数据给所有客户端
 * @param stats 统计数据（nlohmann::json 格式）
 */
void WebSocket::broadcastStats(const nlohmann::json& stats)
{
    if (!ws_server_) return;

    // 快速检查是否有客户端连接
    {
        QMutexLocker clientLock(&clients_mtx_);
        if (clients_.isEmpty()) return;
    }

    // 转换为 Qt JSON 并广播
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
