/*
 * websocket_alarm.cpp - WebSocket 报警模块实现
 *
 * 核心功能：
 *   - checkAndAlarm(): 遍历检测结果，对匹配 alarm_class_names 的目标生成报警
 *   - sendAlarm(): 外部手动发送报警
 *   - 报警消息通过 WebSocket 广播给所有连接的客户端
 *   - 支持频率限制：同类报警间隔 >= 1秒，防止刷屏
 *   - 自动保存报警截图到指定目录
 *
 * 报警 JSON 格式：
 *   {
 *     "type": "alarm",
 *     "data": {
 *       "alarm_id": "uuid_000001",
 *       "alarm_type": "person",
 *       "confidence": "85.3%",
 *       "timestamp": 1719900000000,
 *       "video_url": "",
 *       "image_url": "/path/to/screenshot.jpg"
 *     }
 *   }
 */

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

/**
 * 递归创建目录（类似 mkdir -p）
 * @param path 目录路径（如 "/home/user/output"）
 * @param mode 目录权限（默认 0755）
 */
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
    // 创建最后一级目录
    struct stat st;
    if (stat(current.c_str(), &st) != 0)
        mkdir(current.c_str(), mode);
}

/**
 * 检查检测结果并触发报警
 *
 * 遍历当前帧的所有检测目标，如果类别名在 alarm_class_names 中，
 * 则生成报警消息并通过 WebSocket 广播。
 *
 * @param detect_results 检测结果组（包含所有检测框）
 * @param frame_id       当前帧编号
 * @param frame          当前帧图像（用于保存报警截图，可选）
 * @return 本次触发的报警数量
 */
int WebSocket::checkAndAlarm(const detect_result_group_t *detect_results, int frame_id,
                             const cv::Mat &frame)
{
    // 前置检查：服务未启动、无检测结果、结果为空
    if (!ws_server_ || !detect_results || detect_results->count == 0)
        return 0;

    int alarm_count = 0;

    // 获取当前时间戳（毫秒），用于频率限制
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    long long now_ms = tv.tv_sec * 1000LL + tv.tv_usec / 1000;

    // 加锁读取配置（alarm_class_names 可能被主线程修改）
    QMutexLocker configLock(&config_mtx_);

    // 如果未配置报警类别，直接返回
    if (config_.alarm_class_names.empty())
        return 0;

    // 遍历所有检测目标
    for (int i = 0; i < detect_results->count; i++)
    {
        const detect_result_t &det = detect_results->results[i];

        // 跳过空名称的检测（可能是填充项）
        if (det.name[0] == '\0')
            continue;

        // 检查类别是否在报警列表中
        std::string name(det.name);
        if (config_.alarm_class_names.count(name) == 0)
            continue;

        // --- 频率限制：同类报警间隔 >= 1秒 ---
        {
            QMutexLocker rateLock(&alarm_rate_mtx_);
            auto it = last_alarm_time_.find(name);
            if (it != last_alarm_time_.end() && (now_ms - it->second) < 1000)
                continue;  // 距上次同类报警不足1秒，跳过
            last_alarm_time_[name] = now_ms;
        }

        // 生成唯一报警 ID（UUID + 计数器）
        QString alarmId = generateAlarmId();

        // 构造报警数据 JSON
        QJsonObject data;
        data["alarm_id"] = alarmId;
        data["alarm_type"] = QString::fromUtf8(det.name);
        data["confidence"] = QString("%1%").arg(det.prop * 100.0f, 0, 'f', 1);
        data["timestamp"] = static_cast<qint64>(now_ms);
        data["video_url"] = "";
        data["image_url"] = "";

        // 保存报警截图（如果配置了截图目录）
        if (!frame.empty() && !config_.alarm_screenshot_dir.empty()) {
            char screenshot_path[512];
            snprintf(screenshot_path, sizeof(screenshot_path), "%s/alarm_%s_frame%d_%lld.jpg",
                     config_.alarm_screenshot_dir.c_str(),
                     det.name, frame_id, now_ms);

            // 确保截图目录存在
            std::string dir = config_.alarm_screenshot_dir;
            struct stat st;
            if (stat(dir.c_str(), &st) != 0) {
                mkdirs(dir, 0755);
            }

            // 保存帧图像为 JPEG
            cv::imwrite(screenshot_path, frame);
            data["image_url"] = screenshot_path;
            LOG_INFO("[WebSocket]", "Screenshot saved: %s", screenshot_path);
        }

        // 构造完整报警消息
        QJsonObject msg;
        msg["type"] = "alarm";
        msg["data"] = data;

        // 序列化为 JSON 字符串
        QString json = QString::fromUtf8(
            QJsonDocument(msg).toJson(QJsonDocument::Compact));

        // 广播给所有连接的 WebSocket 客户端
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

        // 日志输出（ALARM 级别，红色加粗显示）
        LOG_ALARM("[WebSocket]", "ALARM [%s]: %s at frame %d",
               alarmId.toUtf8().constData(), det.name, frame_id);

        // 发射 Qt 信号（用于 UI 显示报警动画等）
        emit alarmTriggered(alarmId, QString::fromUtf8(det.name),
                            frame_id, now_ms);
    }

    return alarm_count;
}

/**
 * 手动发送报警（外部调用）
 *
 * 用于非检测场景的报警，如系统异常、手动触发等。
 *
 * @param alarmType  报警类型（如 "system_error"）
 * @param videoUrl   视频 URL（可选）
 * @param imageUrl   截图 URL（可选）
 * @param confidence 置信度（默认 1.0）
 */
void WebSocket::sendAlarm(const QString &alarmType,
                          const QString &videoUrl,
                          const QString &imageUrl,
                          float confidence)
{
    if (!ws_server_)
        return;

    // 获取当前时间戳
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    long long now_ms = tv.tv_sec * 1000LL + tv.tv_usec / 1000;

    // 生成唯一报警 ID
    QString alarmId = generateAlarmId();

    // 构造报警数据
    QJsonObject data;
    data["alarm_id"] = alarmId;
    data["alarm_type"] = alarmType;
    data["confidence"] = QString("%1%").arg(confidence * 100.0f, 0, 'f', 1);
    data["timestamp"] = static_cast<qint64>(now_ms);
    data["video_url"] = videoUrl;
    data["image_url"] = imageUrl;

    // 构造完整消息
    QJsonObject msg;
    msg["type"] = "alarm";
    msg["data"] = data;

    // 序列化并广播
    QString json = QString::fromUtf8(
        QJsonDocument(msg).toJson(QJsonDocument::Compact));
    broadcast(json);

    LOG_ALARM("[WebSocket]", "MANUAL ALARM [%s]: %s",
           alarmId.toUtf8().constData(),
           alarmType.toUtf8().constData());
}

/**
 * 生成唯一报警 ID
 *
 * 格式：{base_uuid}_{counter:06d}
 * base_uuid 在首次调用时从 /proc/sys/kernel/random/uuid 读取
 * counter 是全局原子计数器，每次调用递增
 */
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
