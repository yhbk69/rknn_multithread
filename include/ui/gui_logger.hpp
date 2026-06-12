/**
 * @file gui_logger.hpp
 * @brief GUI 日志输出 (带分类颜色)
 *
 * 在 QTextEdit 中输出带时间戳和分类颜色的日志。
 * 线程安全: 仅在主线程(UI线程)调用。
 */

#ifndef GUI_LOGGER_HPP
#define GUI_LOGGER_HPP

#include <QString>
#include <QTextEdit>
#include <QDateTime>
#include <functional>

class GuiLogger {
public:
    static void log(QTextEdit* logWidget, const QString& category, const QString& message);
    static QString currentTimestamp();

    /// 创建日志回调: (category, message) → 输出到 logWidget
    static std::function<void(const QString&, const QString&)>
    makeLogCallback(QTextEdit* logWidget);

    // 预定义类别常量
    inline static const QString CAT_SYSTEM    = QString::fromUtf8("\u7cfb\u7edf");   // 系统
    inline static const QString CAT_DETECT    = QString::fromUtf8("\u68c0\u6d4b");   // 检测
    inline static const QString CAT_ALERT     = QString::fromUtf8("\u544a\u8b66");   // 告警
    inline static const QString CAT_ERROR     = QString::fromUtf8("\u9519\u8bef");   // 错误
    inline static const QString CAT_RECORD    = QString::fromUtf8("\u5f55\u50cf");   // 录像
    inline static const QString CAT_WS        = QString::fromUtf8("WebSocket");
    inline static const QString CAT_MJPEG     = QString::fromUtf8("MJPEG");
    inline static const QString CAT_CONFIG    = QString::fromUtf8("\u914d\u7f6e");   // 配置
    inline static const QString CAT_MODEL     = QString::fromUtf8("\u6a21\u578b");   // 模型
};

#endif // GUI_LOGGER_HPP
