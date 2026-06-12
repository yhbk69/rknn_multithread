/**
 * @file gui_logger.cpp
 * @brief GUI 日志输出实现
 */

#include "ui/gui_logger.hpp"

void GuiLogger::log(QTextEdit* logWidget, const QString& category, const QString& message) {
    if (!logWidget) return;

    QString ts = currentTimestamp();
    QString color;

    if (category == CAT_ALERT || category == CAT_ERROR) {
        color = "#e74c3c";  // 红色
    } else if (category == CAT_DETECT) {
        color = "#27ae60";  // 绿色
    } else if (category == CAT_RECORD) {
        color = "#e67e22";  // 橙色
    } else if (category == CAT_WS || category == CAT_MJPEG) {
        color = "#3498db";  // 蓝色
    } else if (category == CAT_CONFIG) {
        color = "#9b59b6";  // 紫色
    } else if (category == CAT_SYSTEM) {
        color = "#34495e";  // 深灰
    } else if (category == CAT_MODEL) {
        color = "#5cb85c";  // 绿色
    } else {
        color = "#7f8c8d";  // 灰色
    }

    QString html = QString("<span style='color:%1; font-size:12px;'>[%2][%3] %4</span>")
                       .arg(color, ts, category, message);
    logWidget->append(html);
}

QString GuiLogger::currentTimestamp() {
    return QDateTime::currentDateTime().toString("hh:mm:ss");
}

std::function<void(const QString&, const QString&)>
GuiLogger::makeLogCallback(QTextEdit* logWidget) {
    return [logWidget](const QString& cat, const QString& msg) {
        log(logWidget, cat, msg);
    };
}
