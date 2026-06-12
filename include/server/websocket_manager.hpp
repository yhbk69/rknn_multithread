/**
 * @file websocket_manager.hpp
 * @brief WebSocket 控制通道 (端口 9090)
 *
 * 处理心跳(ping/pong)、摄像头列表、围栏设置、告警同步等控制消息。
 * 告警推送已迁移到 AlertWebSocketServer(端口 9091)。
 *
 * 设计要点:
 * - 不继承 QObject: 用 lambda 回调代替信号槽,避免 MOC 复杂化
 * - 线程安全: 客户端列表用 QMutex 保护
 * - 摄像头列表/实时流: 通过回调从外部获取
 */

#ifndef WEBSOCKET_MANAGER_HPP
#define WEBSOCKET_MANAGER_HPP

#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QList>
#include <QMutex>
#include <QMap>
#include <functional>
#include <memory>
#include "output_sink.hpp"

// 围栏区域定义
struct FenceRegion {
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 1.0f;
    float y2 = 1.0f;
};

// 摄像头流信息(供外部提供)
struct StreamInfo {
    QString streamId;
    QString name;
    QString url;
};

class WebSocketManager : public IOutputSink {
public:
    WebSocketManager();
    ~WebSocketManager();

    void start(quint16 port = 0);
    void stop();
    bool isRunning() const;
    int clientCount() const;
    void setLogCallback(std::function<void(const QString&, const QString&)> callback);

    // ============================================================
    // 外部数据提供者回调 (由 MainWindow 设置)
    // ============================================================

    void setGetStreamsCallback(std::function<QList<StreamInfo>()> callback);
    void setViewStreamCallback(std::function<QString(const QString&)> callback);

    // ============================================================
    // 告警推送 (通过 9090 控制通道)
    // ============================================================

    void pushAlarm(const QString& alarmJson);

    // ============================================================
    // 消息广播
    // ============================================================

    void broadcast(const QString& message);

    // ============================================================
    // 围栏管理
    // ============================================================

    void setFence(const QString& streamId, const FenceRegion& fence);
    FenceRegion getFence(const QString& streamId) const;
    bool hasFence(const QString& streamId) const;

    // ============================================================
    // 客户端管理
    // ============================================================

    void replyToClient(QWebSocket* client, const QJsonObject& response);

private:
    void onNewConnection();
    void onTextMessageForClient(QWebSocket* client, const QString& message);
    void onClientDisconnected(QWebSocket* client);
    void handlePing(QWebSocket* client);
    void handleGetStreams(QWebSocket* client);
    void handleSetFence(QWebSocket* client, const QString& streamId, const QJsonObject& fence);
    void handleViewStream(QWebSocket* client, const QString& streamId);

    QObject* connCtx_ = nullptr;
    QWebSocketServer* server_ = nullptr;
    quint16 port_ = 0;
    QList<QWebSocket*> clients_;
    mutable QMutex mutex_;

    // 围栏区域
    QMap<QString, FenceRegion> fenceRegions_;

    // 外部回调
    std::function<QList<StreamInfo>()> getStreamsCallback_;
    std::function<QString(const QString&)> viewStreamCallback_;
    std::function<void(const QString&, const QString&)> logCallback_;
};

#endif // WEBSOCKET_MANAGER_HPP
