#include "websocket.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "Logger.hpp"
#include "ModelManager.hpp"

void WebSocket::handlePing(QWebSocket *client)
{
    QJsonObject pong;
    pong["type"] = "pong";
    client->sendTextMessage(QString::fromUtf8(
        QJsonDocument(pong).toJson(QJsonDocument::Compact)));
}

void WebSocket::handleAck(QWebSocket *client, const QJsonObject &json)
{
    QString alarmId = json["alarm_id"].toString();
    LOG_INFO("[WebSocket]", "ACK from client for alarm: %s",
           alarmId.toUtf8().constData());
    emit alarmAcknowledged(client, alarmId);
}

void WebSocket::handleGetStats(QWebSocket *client)
{
    QJsonObject data;

    if (stats_callback_) {
        nlohmann::json j = stats_callback_();
        data = QJsonDocument::fromJson(
            QByteArray::fromStdString(j.dump())).object();
    } else {
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

void WebSocket::handleSwitchModel(QWebSocket *client, const QJsonObject &json)
{
    QString modelName = json["model"].toString();

    QJsonObject resp;
    resp["type"] = "model_switched";
    resp["model"] = modelName;

    if (!model_manager_) {
        resp["success"] = false;
        resp["error"] = "Model manager not initialized";
        client->sendTextMessage(QString::fromUtf8(
            QJsonDocument(resp).toJson(QJsonDocument::Compact)));
        return;
    }

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

    LOG_INFO("[WebSocket]", "Fence set for stream %s: (%.2f,%.2f)-(%.2f,%.2f)",
           fence.stream_id.toUtf8().constData(),
           fence.x1, fence.y1, fence.x2, fence.y2);

    QJsonObject resp;
    resp["type"] = "set_fence_ack";
    resp["stream_id"] = fence.stream_id;
    resp["success"] = true;
    client->sendTextMessage(QString::fromUtf8(
        QJsonDocument(resp).toJson(QJsonDocument::Compact)));

    emit fenceSet(fence.stream_id, fence);
}

void WebSocket::handleSetThreshold(QWebSocket *client, const QJsonObject &json)
{
    float conf = json["conf"].toDouble(-1.0f);
    float nms = json["nms"].toDouble(-1.0f);

    QJsonObject resp;
    resp["type"] = "threshold_set";

    // 参数校验：阈值范围 [0, 1]
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

    emit thresholdChanged(conf, nms);
}
