#include "alert_websocket_server.hpp"
#include "core/config.hpp"
#include <QObject>
#include <QHostAddress>
#include <QFileInfo>
#include <QDateTime>

AlertWebSocketServer::AlertWebSocketServer()
    : connCtx_(new QObject())
    , ackTimer_(new QTimer(connCtx_))
{
    QObject::connect(ackTimer_, &QTimer::timeout, connCtx_, [this]() {
        checkPendingAlarms();
    });
    ackTimer_->start(ACK_CHECK_INTERVAL_MS);
}

AlertWebSocketServer::~AlertWebSocketServer() {
    stop();
    delete connCtx_;
}

void AlertWebSocketServer::start(quint16 port) {
    stop();

    server_ = new QWebSocketServer("YOLO11-Alert", QWebSocketServer::NonSecureMode, connCtx_);
    if (!server_->listen(QHostAddress::Any, port)) {
        if (logCallback_) {
            logCallback_(QStringLiteral("告警"), QStringLiteral("服务启动失败, 端口: %1").arg(port));
        }
        delete server_;
        server_ = nullptr;
        return;
    }

    QObject::connect(server_, &QWebSocketServer::newConnection, connCtx_, [this]() {
        onNewConnection();
    });

    if (logCallback_) {
        logCallback_(QStringLiteral("告警"), QStringLiteral("服务已启动 ws://0.0.0.0:%1").arg(port));
    }
}

void AlertWebSocketServer::stop() {
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
        pendingAlarms_.clear();  // 清理待确认告警
    }
    for (auto* client : toClose) {
        if (client) {
            client->close();
            client->deleteLater();
        }
    }
}

bool AlertWebSocketServer::isRunning() const {
    return server_ && server_->isListening();
}

int AlertWebSocketServer::clientCount() const {
    QMutexLocker locker(&mutex_);
    return clients_.size();
}

void AlertWebSocketServer::onAlert(const AlertData& data) {
    qInfo() << "[告警服务器] *** onAlert 被调用! video=" << data.videoPath << ", image=" << data.imagePath;
    pushAlert(data);
}

void AlertWebSocketServer::pushAlert(const AlertData& data) {
    qInfo() << "[告警服务器] pushAlert 开始执行";
    QMutexLocker locker(&mutex_);
    if (clients_.isEmpty()) {
        qInfo() << "[告警服务器] pushAlert: 无客户端连接, 丢弃告警";
        if (logCallback_) {
            logCallback_(QStringLiteral("告警"), QStringLiteral("无客户端连接, 丢弃告警"));
        }
        return;
    }
    if (data.alertJson.isEmpty()) {
        if (logCallback_) {
            logCallback_(QStringLiteral("告警"), QStringLiteral("alertJson 为空, 丢弃告警"));
        }
        return;
    }

    // 解析 alarm_id
    QJsonDocument doc = QJsonDocument::fromJson(data.alertJson.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        if (logCallback_) {
            logCallback_(QStringLiteral("告警"), QStringLiteral("告警JSON格式错误"));
        }
        return;
    }

    QString alarmId = doc.object()["data"].toObject()["alarm_id"].toString();
    if (alarmId.isEmpty()) {
        if (logCallback_) {
            logCallback_(QStringLiteral("告警"), QStringLiteral("告警缺少 alarm_id 字段"));
        }
        return;
    }

    if (logCallback_) {
        logCallback_(QStringLiteral("告警"), QString("开始处理告警: alarmId=%1, imagePath=%2, videoPath=%3").arg(alarmId).arg(data.imagePath).arg(data.videoPath));
    }

    // 创建待确认告警
    PendingAlarm pending;
    pending.alarmId = alarmId;
    pending.alertJson = data.alertJson.toUtf8();
    pending.imagePath = data.imagePath;
    pending.videoPath = data.videoPath;
    pending.retryCount = 0;

    pendingAlarms_.append(pending);

    // 立即发送
    sendAlarmToClients(pendingAlarms_.last());

    if (logCallback_) {
        logCallback_(QStringLiteral("告警"), QString("告警已推送: %1 (等待ACK, 最多重试%2次)").arg(alarmId).arg(PendingAlarm::MAX_RETRIES));
    }
}

void AlertWebSocketServer::sendAlarmToClients(const PendingAlarm& alarm) {
    // NOTE: caller must hold mutex_ lock
    if (logCallback_) {
        logCallback_(QStringLiteral("告警"), QString("开始发送告警[%1]: 客户端数=%2, 图片=%3, 视频=%4").arg(alarm.alarmId).arg(clients_.size()).arg(alarm.imagePath).arg(alarm.videoPath));
    }
    // 仅发送告警元数据 JSON, 客户端通过 HTTP 下载文件
    int sentCount = 0;
    for (auto* client : clients_) {
        if (client && client->state() == QAbstractSocket::ConnectedState) {
            client->sendTextMessage(alarm.alertJson);
            sentCount++;
        }
    }
    if (logCallback_ && sentCount > 0) {
        logCallback_(QStringLiteral("告警"), QString("已发送告警元数据[%1]到 %2 个客户端").arg(alarm.alarmId).arg(sentCount));
    }
}

void AlertWebSocketServer::checkPendingAlarms() {
    QMutexLocker locker(&mutex_);
    if (pendingAlarms_.isEmpty()) return;
    if (clients_.isEmpty()) {
        if (logCallback_) {
            logCallback_(QStringLiteral("告警"), QStringLiteral("无客户端, 跳过ACK检查"));
        }
        return;
    }

    for (int i = pendingAlarms_.size() - 1; i >= 0; --i) {
        PendingAlarm& alarm = pendingAlarms_[i];
        alarm.retryCount++;

        if (alarm.retryCount >= PendingAlarm::MAX_RETRIES) {
            // 超过最大重试次数, 放弃
            if (logCallback_) {
                logCallback_(QStringLiteral("告警"), QString("告警[%1]已重试%2次, 超时放弃").arg(alarm.alarmId).arg(alarm.retryCount));
            }
            pendingAlarms_.removeAt(i);
        } else {
            // 重传
            if (logCallback_) {
                logCallback_(QStringLiteral("告警"), QString("告警[%1]未收到ACK, 第%2次重传").arg(alarm.alarmId).arg(alarm.retryCount));
            }
            sendAlarmToClients(alarm);
        }
    }
}

void AlertWebSocketServer::setLogCallback(std::function<void(const QString&, const QString&)> callback) {
    logCallback_ = std::move(callback);
}

void AlertWebSocketServer::onNewConnection() {
    auto* socket = server_->nextPendingConnection();
    if (!socket) return;

    QString peerIp = socket->peerAddress().toString();
    quint16 peerPort = socket->peerPort();

    {
        QMutexLocker locker(&mutex_);
        clients_.append(socket);
    }

    QObject::connect(socket, &QWebSocket::textMessageReceived, connCtx_, [this, socket](const QString& message) {
        onTextMessageReceived(socket, message);
    });

    QObject::connect(socket, &QWebSocket::disconnected, connCtx_, [this, socket]() {
        onClientDisconnected(socket);
    });

    if (logCallback_) {
        QMutexLocker locker(&mutex_);
        logCallback_(QStringLiteral("告警"), QString("新客户端已连接 [%1:%2], 当前连接数: %3").arg(peerIp).arg(peerPort).arg(clients_.size()));
    }
}

void AlertWebSocketServer::onTextMessageReceived(QWebSocket* socket, const QString& message) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isNull()) return;

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    // 处理 ACK
    if (type == "ack") {
        QString alarmId = obj["alarm_id"].toString();
        if (alarmId.isEmpty()) return;

        QMutexLocker locker(&mutex_);
        for (int i = 0; i < pendingAlarms_.size(); ++i) {
            if (pendingAlarms_[i].alarmId == alarmId) {
                pendingAlarms_.removeAt(i);
                if (logCallback_) {
                    logCallback_(QStringLiteral("告警"), QString("告警已确认(ACK): %1").arg(alarmId));
                }
                return;
            }
        }
    }
}

void AlertWebSocketServer::onClientDisconnected(QWebSocket* socket) {
    if (!socket) return;

    QString peerIp = socket->peerAddress().toString();
    quint16 peerPort = socket->peerPort();

    int remainingCount = 0;
    {
        QMutexLocker locker(&mutex_);
        clients_.removeAll(socket);
        remainingCount = clients_.size();
    }

    if (logCallback_) {
        logCallback_(QStringLiteral("告警"), QString("客户端已断开 [%1:%2], 剩余: %3").arg(peerIp).arg(peerPort).arg(remainingCount));
    }

    socket->deleteLater();
}
