#ifndef ALERT_WEBSOCKET_SERVER_HPP
#define ALERT_WEBSOCKET_SERVER_HPP

#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QList>
#include <QMutex>
#include <QFile>
#include <QTimer>
#include <QScopedPointer>
#include <functional>
#include "output_sink.hpp"

// 待确认告警
struct PendingAlarm {
    QString alarmId;
    QByteArray alertJson;  // 告警JSON
    QString imagePath;     // 截图路径
    QString videoPath;     // 视频路径
    int retryCount = 0;    // 已重试次数
    static constexpr int MAX_RETRIES = 10;  // 最大重试次数
};

class AlertWebSocketServer : public IOutputSink {
public:
    AlertWebSocketServer();
    ~AlertWebSocketServer();

    void start(quint16 port);
    void stop();
    bool isRunning() const;
    int clientCount() const;
    void onAlert(const AlertData& data) override;
    void setLogCallback(std::function<void(const QString&, const QString&)> callback);

private:
    void onNewConnection();
    void onTextMessageReceived(QWebSocket* socket, const QString& message);
    void onClientDisconnected(QWebSocket* socket);
    void pushAlert(const AlertData& data);
    void sendAlarmToClients(const PendingAlarm& alarm);
    void checkPendingAlarms();

    QObject* connCtx_;
    QWebSocketServer* server_ = nullptr;
    QList<QWebSocket*> clients_;
    mutable QMutex mutex_;
    std::function<void(const QString&, const QString&)> logCallback_;

    // 待ACK告警列表
    QList<PendingAlarm> pendingAlarms_;
    QTimer* ackTimer_ = nullptr;  // ACK超时检查定时器
    static constexpr int ACK_CHECK_INTERVAL_MS = 5000;  // 每5秒检查一次
};

#endif // ALERT_WEBSOCKET_SERVER_HPP
