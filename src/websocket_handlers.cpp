/*
 * websocket_handlers.cpp - WebSocket 消息处理函数实现
 *
 * 处理客户端发送的各类消息：
 *   - ping: 心跳检测
 *   - ack: 报警确认
 *   - get_stats: 获取统计数据
 *   - switch_model: 切换模型
 *   - get_models: 获取模型列表
 *   - get_streams: 获取视频流列表
 *   - set_fence: 设置围栏区域
 *   - set_threshold: 动态设置检测阈值
 *
 * 消息格式：
 *   请求: {"type": "消息类型", ...}
 *   响应: {"type": "响应类型", "data": {...}}
 */

#include "websocket.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "Logger.hpp"
#include "ModelManager.hpp"

/**
 * 处理心跳消息（ping → pong）
 *
 * 客户端定期发送 ping 检测连接是否存活，
 * 服务器回复 pong 表示连接正常。
 */
void WebSocket::handlePing(QWebSocket *client)
{
    QJsonObject pong;
    pong["type"] = "pong";
    client->sendTextMessage(QString::fromUtf8(
        QJsonDocument(pong).toJson(QJsonDocument::Compact)));
}

/**
 * 处理报警确认（ack）
 *
 * 客户端收到报警后发送 ack 确认，表示已知悉。
 *
 * 请求: {"type": "ack", "alarm_id": "uuid_000001"}
 */
void WebSocket::handleAck(QWebSocket *client, const QJsonObject &json)
{
    QString alarmId = json["alarm_id"].toString();
    LOG_INFO("[WebSocket]", "ACK from client for alarm: %s",
           alarmId.toUtf8().constData());
    // 发射 Qt 信号（用于 UI 显示确认状态）
    emit alarmAcknowledged(client, alarmId);
}

/**
 * 处理获取统计数据请求
 *
 * 返回当前检测的 FPS、帧数、检测数量等统计信息。
 *
 * 请求: {"type": "get_stats"}
 * 响应: {"type": "stats", "data": {"fps_current": 20, "fps_avg": 18.5, ...}}
 */
void WebSocket::handleGetStats(QWebSocket *client)
{
    QJsonObject data;

    if (stats_callback_) {
        // 调用统计回调函数获取数据
        nlohmann::json j = stats_callback_();
        data = QJsonDocument::fromJson(
            QByteArray::fromStdString(j.dump())).object();
    } else {
        // 无回调时返回默认值
        data["fps_current"] = 0;
        data["fps_avg"] = 0;
        data["total_frames"] = 0;
        data["total_detections"] = 0;
    }

    QJsonObject resp;
    resp["type"] = "stats";
    resp["data"] = data;

    client->sendTextMessage(QString::fromUtf8(
        QJsonDocument(resp).toJson(QJsonDocument::Compact)));
}

/**
 * 处理模型切换请求
 *
 * 通过 ModelManager 切换当前活跃模型。
 *
 * 请求: {"type": "switch_model", "model": "yolo11n"}
 * 响应: {"type": "model_switched", "model": "yolo11n", "success": true}
 */
void WebSocket::handleSwitchModel(QWebSocket *client, const QJsonObject &json)
{
    QString modelName = json["model"].toString();

    QJsonObject resp;
    resp["type"] = "model_switched";
    resp["model"] = modelName;

    // 检查 ModelManager 是否已初始化
    if (!model_manager_) {
        resp["success"] = false;
        resp["error"] = "Model manager not initialized";
        client->sendTextMessage(QString::fromUtf8(
            QJsonDocument(resp).toJson(QJsonDocument::Compact)));
        return;
    }

    // 执行模型切换
    bool success = model_manager_->switchModel(modelName.toStdString());
    resp["success"] = success;

    if (success) {
        LOG_INFO("[WebSocket]", "Model switched to: %s", modelName.toUtf8().constData());
    } else {
        LOG_WARN("[WebSocket]", "Failed to switch model: %s", modelName.toUtf8().constData());
    }

    client->sendTextMessage(QString::fromUtf8(
        QJsonDocument(resp).toJson(QJsonDocument::Compact)));
}

/**
 * 处理获取模型列表请求
 *
 * 返回所有已加载的模型和当前活跃模型。
 *
 * 请求: {"type": "get_models"}
 * 响应: {"type": "models_list", "data": {"active_model": "yolov5s", "models": [...]}}
 */
void WebSocket::handleGetModels(QWebSocket *client)
{
    QJsonObject resp;
    resp["type"] = "models_list";

    if (model_manager_) {
        nlohmann::json j = model_manager_->toJson();
        resp["data"] = QJsonDocument::fromJson(
            QByteArray::fromStdString(j.dump())).object();
    } else {
        QJsonObject data;
        data["active_model"] = "";
        data["models"] = QJsonArray();
        resp["data"] = data;
    }

    client->sendTextMessage(QString::fromUtf8(
        QJsonDocument(resp).toJson(QJsonDocument::Compact)));
}

/**
 * 处理获取视频流列表请求
 *
 * 返回所有已注册的视频流信息。
 *
 * 请求: {"type": "get_streams"}
 * 响应: {"type": "streams_list", "data": [{"stream_id": "1", "name": "摄像头1", ...}]}
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

/**
 * 处理设置围栏请求
 *
 * 为指定视频流设置围栏区域（归一化坐标 0.0~1.0）。
 *
 * 请求: {"type": "set_fence", "stream_id": "1", "fence": {"x1":0.1, "y1":0.1, "x2":0.9, "y2":0.9}}
 * 响应: {"type": "set_fence_ack", "stream_id": "1", "success": true}
 */
void WebSocket::handleSetFence(QWebSocket *client, const QJsonObject &json)
{
    // 解析围栏参数
    FenceRegion fence;
    fence.stream_id = json["stream_id"].toString();

    QJsonObject f = json["fence"].toObject();
    fence.x1 = f["x1"].toDouble();
    fence.y1 = f["y1"].toDouble();
    fence.x2 = f["x2"].toDouble();
    fence.y2 = f["y2"].toDouble();

    // 保存围栏配置
    setFence(fence);

    LOG_INFO("[WebSocket]", "Fence set for stream %s: (%.2f,%.2f)-(%.2f,%.2f)",
           fence.stream_id.toUtf8().constData(),
           fence.x1, fence.y1, fence.x2, fence.y2);

    // 发送确认响应
    QJsonObject resp;
    resp["type"] = "set_fence_ack";
    resp["stream_id"] = fence.stream_id;
    resp["success"] = true;
    client->sendTextMessage(QString::fromUtf8(
        QJsonDocument(resp).toJson(QJsonDocument::Compact)));

    // 发射 Qt 信号（通知 GUI 更新围栏显示）
    emit fenceSet(fence.stream_id, fence);
}

/**
 * 处理动态设置检测阈值请求
 *
 * 实时修改置信度和 NMS 阈值，无需重启。
 *
 * 请求: {"type": "set_threshold", "conf": 0.5, "nms": 0.45}
 * 响应: {"type": "threshold_set", "success": true, "conf": 0.5, "nms": 0.45}
 */
void WebSocket::handleSetThreshold(QWebSocket *client, const QJsonObject &json)
{
    float conf = json["conf"].toDouble(-1.0f);
    float nms = json["nms"].toDouble(-1.0f);

    QJsonObject resp;
    resp["type"] = "threshold_set";

    // 参数校验：阈值范围必须在 [0, 1]
    if (conf < 0.0f || conf > 1.0f || nms < 0.0f || nms > 1.0f) {
        resp["success"] = false;
        resp["error"] = "Threshold values must be in [0, 1]";
        client->sendTextMessage(QString::fromUtf8(
            QJsonDocument(resp).toJson(QJsonDocument::Compact)));
        return;
    }

    LOG_INFO("[WebSocket]", "Threshold set: conf=%.2f, nms=%.2f", conf, nms);

    resp["success"] = true;
    resp["conf"] = conf;
    resp["nms"] = nms;
    client->sendTextMessage(QString::fromUtf8(
        QJsonDocument(resp).toJson(QJsonDocument::Compact)));

    // 发射 Qt 信号（通知 DetectThread 更新阈值）
    emit thresholdChanged(conf, nms);
}
