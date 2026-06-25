#include "websocket.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHostAddress>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <cstdio>
#include <errno.h>

#include <opencv2/imgcodecs.hpp>

#include "Logger.hpp"
#include "ModelManager.hpp"

/* 递归创建目录（类似 mkdir -p） */
static void mkdirs(const std::string &path, mode_t mode) {
    std::string current;
    for (char c : path) {
        current += c;
        if (c == '/' || c == '\\') {
            if (!current.empty() && current.back() != ':') {
                struct stat st;
                if (stat(current.c_str(), &st) != 0)
                    mkdir(current.c_str(), mode);
            }
        }
    }
    struct stat st;
    if (stat(current.c_str(), &st) != 0)
        mkdir(current.c_str(), mode);
}

int WebSocket::checkAndAlarm(const detect_result_group_t *detect_results, int frame_id,
                             const cv::Mat &frame)
{
    if (!ws_server_ || !detect_results || detect_results->count == 0)
        return 0;

    int alarm_count = 0;

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    long long now_ms = tv.tv_sec * 1000LL + tv.tv_usec / 1000;

    QMutexLocker configLock(&config_mtx_);

    if (config_.alarm_class_names.empty())
        return 0;

    for (int i = 0; i < detect_results->count; i++)
    {
        const detect_result_t &det = detect_results->results[i];

        if (det.name[0] == '\0')
            continue;

        std::string name(det.name);
        if (config_.alarm_class_names.count(name) == 0)
            continue;

        {
            QMutexLocker rateLock(&alarm_rate_mtx_);
            auto it = last_alarm_time_.find(name);
            if (it != last_alarm_time_.end() && (now_ms - it->second) < 1000)
                continue;
            last_alarm_time_[name] = now_ms;
        }

        QString alarmId = generateAlarmId();

        QJsonObject data;
        data["alarm_id"] = alarmId;
        data["alarm_type"] = QString::fromUtf8(det.name);
        data["confidence"] = QString("%1%").arg(det.prop * 100.0f, 0, 'f', 1);
        data["timestamp"] = static_cast<qint64>(now_ms);
        data["video_url"] = "";
        data["image_url"] = "";

        if (!frame.empty() && !config_.alarm_screenshot_dir.empty()) {
            char screenshot_path[512];
            snprintf(screenshot_path, sizeof(screenshot_path), "%s/alarm_%s_frame%d_%lld.jpg",
                     config_.alarm_screenshot_dir.c_str(),
                     det.name, frame_id, now_ms);
            std::string dir = config_.alarm_screenshot_dir;
            struct stat st;
            if (stat(dir.c_str(), &st) != 0) {
                mkdirs(dir, 0755);
            }
            cv::imwrite(screenshot_path, frame);
            data["image_url"] = screenshot_path;
            LOG_INFO("[WebSocket]", "Screenshot saved: %s", screenshot_path);
        }

        QJsonObject msg;
        msg["type"] = "alarm";
        msg["data"] = data;

        QString json = QString::fromUtf8(
            QJsonDocument(msg).toJson(QJsonDocument::Compact));

        {
            QMutexLocker clientLock(&clients_mtx_);
            if (!clients_.isEmpty()) {
                for (QWebSocket *client : clients_)
                {
                    if (client->isValid())
                        client->sendTextMessage(json);
                }
            }
        }

        alarm_count++;
        LOG_ALARM("[WebSocket]", "ALARM [%s]: %s at frame %d",
               alarmId.toUtf8().constData(), det.name, frame_id);

        emit alarmTriggered(alarmId, QString::fromUtf8(det.name),
                            frame_id, now_ms);
    }

    return alarm_count;
}

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

    LOG_ALARM("[WebSocket]", "MANUAL ALARM [%s]: %s",
           alarmId.toUtf8().constData(),
           alarmType.toUtf8().constData());
}

QString WebSocket::generateAlarmId()
{
    static const QString base_uuid = []() {
        FILE *fp = fopen("/proc/sys/kernel/random/uuid", "r");
        if (fp) {
            char buf[40] = {0};
            if (fread(buf, 1, 36, fp) == 36) {
                fclose(fp);
                return QString::fromLatin1(buf, 36);
            }
            fclose(fp);
        }
        return QString("unknown");
    }();
    return QString("%1_%2").arg(base_uuid).arg(alarm_counter_++, 6, 10, QChar('0'));
}
