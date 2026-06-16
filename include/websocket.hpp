/*
 * websocket.hpp - WebSocket 通信服务模块
 *
 * 基于 Qt5WebSockets 的通用 WebSocket 服务器，支持：
 *   - 客户端连接管理
 *   - 报警消息推送（检测到指定类别时）
 *   - 文本/二进制消息收发
 *   - 应用层 ping/pong
 *   - 摄像头流列表查询（get_streams）
 *   - 围栏设置（set_fence）
 *   - 报警确认（ack）
 *   - 可扩展的消息类型处理
 *
 * 协议格式：
 *   报警: {"type":"alarm","data":{"alarm_id":"...","alarm_type":"...","timestamp":...,"video_url":"...","image_url":"..."}}
 *   确认: {"type":"ack","alarm_id":"..."}
 *   心跳: {"type":"ping"} -> {"type":"pong"}
 *   流列表: {"type":"get_streams"} -> {"type":"streams_list","data":[...]}
 *   围栏: {"type":"set_fence","stream_id":"...","fence":{...}}
 *
 * 使用 Qt 事件循环，线程安全通过信号槽机制保证。
 */

#ifndef WEBSOCKET_HPP
#define WEBSOCKET_HPP

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QList>
#include <QSet>
#include <QMap>
#include <QMutex>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <map>
#include <string>
#include <set>
#include <sys/time.h>

#include "postprocess.h"

/* ============================================================
 * 视频流信息
 * ============================================================ */
struct StreamInfo
{
    QString stream_id;    // 流 ID
    QString name;         // 流名称（如"摄像头1"）
    QString url;          // 流地址
};

/* ============================================================
 * 围栏区域（归一化坐标 0.0~1.0）
 * ============================================================ */
struct FenceRegion
{
    QString stream_id;    // 关联的流 ID
    double x1 = 0.0;     // 左上角 x（归一化）
    double y1 = 0.0;     // 左上角 y（归一化）
    double x2 = 1.0;     // 右下角 x（归一化）
    double y2 = 1.0;     // 右下角 y（归一化）
};

/* ============================================================
 * WebSocket 配置
 * ============================================================ */
struct WebSocketConfig
{
    QString server_host = "0.0.0.0";  // 监听地址
    int server_port = 9002;           // 监听端口

    /* 报警相关配置 */
    bool enable_alarm = true;             // 是否启用检测报警
    std::set<int> alarm_class_ids;        // 需要报警的类别 ID 集合
    std::set<std::string> alarm_class_names; // 需要报警的类别名称集合
};

/* ============================================================
 * WebSocket - 通信服务类
 *
 * 继承 QObject，使用信号槽机制与 Qt 事件循环集成。
 *
 * 使用示例：
 *   WebSocket ws;
 *   WebSocketConfig cfg;
 *   cfg.alarm_class_names = {"person", "no_helmet"};
 *   ws.init(cfg);
 *
 *   // 添加摄像头流
 *   ws.addStream({"1", "摄像头1", "http://192.168.1.100:8080/stream1"});
 *
 *   // 在检测循环中
 *   ws.checkAndAlarm(&detect_result_group, frame_id);
 *
 *   // 程序退出时
 *   ws.shutdown();
 * ============================================================ */
class WebSocket : public QObject
{
    Q_OBJECT

public:
    explicit WebSocket(QObject *parent = nullptr);
    ~WebSocket() override;

    WebSocket(const WebSocket &) = delete;
    WebSocket &operator=(const WebSocket &) = delete;
    WebSocket(WebSocket &&) = delete;
    WebSocket &operator=(WebSocket &&) = delete;

    /* ---- 服务器生命周期 ---- */

    int init(const WebSocketConfig &config);
    void shutdown();
    int clientCount() const;
    bool isListening() const;
    bool isAlarmEnabled() const;

    /* ---- 消息收发 ---- */

    void broadcast(const QString &message);
    void broadcastBinary(const QByteArray &data);
    void sendToClient(QWebSocket *client, const QString &message);

    /* ---- 报警 ---- */

    /*
     * 检查检测结果，对匹配类别的目标生成报警并广播
     * @param detect_results 检测结果组
     * @param frame_id       帧编号
     * @return 本次触发的报警数量
     */
    int checkAndAlarm(const detect_result_group_t *detect_results, int frame_id);

    /* 手动发送报警（外部调用） */
    void sendAlarm(const QString &alarmType, const QString &videoUrl = "",
                   const QString &imageUrl = "", float confidence = 1.0f);

    /* ---- 流管理 ---- */

    /* 添加/移除视频流（供 get_streams 查询） */
    void addStream(const StreamInfo &stream);
    void removeStream(const QString &streamId);
    QList<StreamInfo> streams() const;

    /* ---- 围栏管理 ---- */

    /* 设置/获取围栏区域 */
    void setFence(const FenceRegion &fence);
    QList<FenceRegion> fences() const;

    /* ---- 配置 ---- */

    void updateConfig(const WebSocketConfig &config);
    WebSocketConfig config() const;

signals:
    /* 客户端事件 */
    void clientConnected(QWebSocket *client);
    void clientDisconnected(QWebSocket *client);
    void textMessageReceived(QWebSocket *client, const QString &message);
    void binaryMessageReceived(QWebSocket *client, const QByteArray &data);

    /* 服务器事件 */
    void serverStarted(quint16 port);
    void serverClosed();
    void serverError(const QString &errorString);

    /* 报警事件 */
    void alarmTriggered(const QString &alarmId, const QString &alarmType,
                        int frameId, long long timestampMs);

    /* 收到报警确认 */
    void alarmAcknowledged(QWebSocket *client, const QString &alarmId);

    /* 收到围栏设置 */
    void fenceSet(const QString &streamId, const FenceRegion &fence);

    /* 服务器关闭完成 */
    void closed();

private slots:
    void onNewConnection();
    void onSocketDisconnected();
    void onTextMessageReceived(const QString &message);
    void onBinaryMessageReceived(const QByteArray &data);

private:
    /* 消息分发 */
    void dispatchMessage(QWebSocket *client, const QJsonObject &json);

    /* 协议处理 */
    void handlePing(QWebSocket *client);
    void handleAck(QWebSocket *client, const QJsonObject &json);
    void handleGetStreams(QWebSocket *client);
    void handleSetFence(QWebSocket *client, const QJsonObject &json);

    /* 工具 */
    QString generateAlarmId();
    void removeClient(QWebSocket *client);
    QJsonObject streamToJson(const StreamInfo &s) const;
    QJsonObject fenceToJson(const FenceRegion &f) const;

    QWebSocketServer *ws_server_ = nullptr;
    QList<QWebSocket *> clients_;
    mutable QMutex clients_mtx_;

    WebSocketConfig config_;
    mutable QMutex config_mtx_;

    QList<StreamInfo> streams_;
    mutable QMutex streams_mtx_;

    QList<FenceRegion> fences_;
    mutable QMutex fences_mtx_;

    std::map<std::string, long long> last_alarm_time_;  // 速率限制：每类别最后报警时间

    int alarm_counter_ = 0;  // 报警计数器（用于生成 alarm_id）
};

#endif /* WEBSOCKET_HPP */
