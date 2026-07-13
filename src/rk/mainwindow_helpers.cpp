/*
 * mainwindow_helpers.cpp - 辅助函数实现
 *
 * 提供以下功能：
 *   - 日志输出（带颜色分类）
 *   - 设置保存/恢复（QSettings）
 *   - UI 控件启用/禁用控制
 *   - 缩放状态管理
 *   - 报警截图目录同步
 */

#include "rk/mainwindow.hpp"
#include <QDateTime>
#include <QSettings>
#include <QTextCharFormat>
#include <QTextCursor>
#include <fstream>
#include "nlohmann/json.hpp"
using json = nlohmann::json;

/**
 * 获取当前时间戳字符串
 * 格式：yyyy-MM-dd HH:mm:ss
 */
QString MainWindow::currentTimestamp() {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
}

/**
 * 添加日志消息（根据类别自动选择颜色）
 *
 * 类别颜色映射：
 *   - alarm/error: 红色
 *   - warning/warn: 黄色
 *   - system: 蓝色
 *   - info: 绿色
 *   - 其他: 灰色
 *
 * @param category 日志类别（如 "alarm", "info"）
 * @param message  日志消息
 */
void MainWindow::log(const QString& category, const QString& message) {
    QColor color;
    QString cat_lower = category.toLower();

    if (cat_lower == "alarm" || cat_lower == "error") {
        color = QColor(220, 50, 50);    // 红色
    } else if (cat_lower == "warning" || cat_lower == "warn") {
        color = QColor(220, 180, 0);    // 黄色
    } else if (cat_lower == "system") {
        color = QColor(50, 120, 220);   // 蓝色
    } else if (cat_lower == "info") {
        color = QColor(50, 180, 50);    // 绿色
    } else {
        color = QColor(180, 180, 180);  // 灰色（默认）
    }

    logWithColor(category, message, color);
}

/**
 * 添加带颜色的日志消息到 QTextEdit
 *
 * 格式：[时间戳][类别] 消息内容
 *
 * @param category 日志类别
 * @param message  日志消息
 * @param color    文字颜色
 */
void MainWindow::logWithColor(const QString& category, const QString& message, const QColor& color) {
    QTextCursor cursor = log_edit_->textCursor();
    cursor.movePosition(QTextCursor::End);  // 移动到末尾

    // 添加时间戳（灰色）
    QTextCharFormat tsFormat;
    tsFormat.setForeground(QColor(128, 128, 128));
    cursor.insertText(QString("[%1]").arg(currentTimestamp()), tsFormat);

    // 添加类别（带颜色，加粗）
    QTextCharFormat catFormat;
    catFormat.setForeground(color);
    catFormat.setFontWeight(QFont::Bold);
    cursor.insertText(QString("[%1] ").arg(category), catFormat);

    // 添加消息（带颜色）
    QTextCharFormat msgFormat;
    msgFormat.setForeground(color);
    cursor.insertText(message + "\n", msgFormat);

    // 滚动到底部
    log_edit_->setTextCursor(cursor);
    log_edit_->ensureCursorVisible();
}

/**
 * 更新阈值显示标签
 */
void MainWindow::updateThresholdLabels() {
    conf_value_label_->setText(QString::number(conf_threshold_, 'f', 2));
    nms_value_label_->setText(QString::number(nms_threshold_, 'f', 2));
}

/**
 * 启用/禁用 UI 控件
 *
 * @param running true=检测运行中（禁用部分控件），false=停止（启用控件）
 */
void MainWindow::enableControls(bool running) {
    start_btn_->setEnabled(!running);      // 开始按钮：停止时可用
    stop_btn_->setEnabled(running);        // 停止按钮：运行时可用
    pause_btn_->setEnabled(running);       // 暂停按钮：运行时可用
    step_btn_->setEnabled(running);        // 单帧按钮：运行时可用
    screenshot_btn_->setEnabled(running);  // 截图按钮：运行时可用
    export_btn_->setEnabled(!running);     // 导出按钮：停止时可用
    model_btn_->setEnabled(!running);      // 浏览按钮：停止时可用
    load_btn_->setEnabled(!running);       // 加载按钮：停止时可用
    for (int i = 0; i < MAX_CHANNELS; i++)
        video_btns_[i]->setEnabled(!running);  // 视频浏览按钮：停止时可用
    thread_spin_->setEnabled(!running);    // 线程数：停止时可调
}

/**
 * 格式化文件大小为可读字符串
 * @param bytes 字节数
 * @return 格式化字符串（如 "1.5 MB"）
 */
QString MainWindow::formatSize(qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
}

/**
 * 保存用户设置到 QSettings
 *
 * 保存内容：
 *   - 模型路径
 *   - 各通道视频源路径和别名
 *   - 线程数
 *   - 检测阈值
 *   - 窗口几何信息
 */
void MainWindow::saveSettings() {
    QSettings s("RK3588", "YOLO_Detector");
    s.setValue("model_path", model_edit_->text());
    for (int i = 0; i < MAX_CHANNELS; i++) {
        s.setValue(QString("video_path_%1").arg(i), video_edits_[i]->text());
        s.setValue(QString("video_alias_%1").arg(i), video_alias_edits_[i]->text());
    }
    s.setValue("thread_num", thread_spin_->value());
    s.setValue("conf_threshold", conf_threshold_);
    s.setValue("nms_threshold", nms_threshold_);
    s.setValue("geometry", saveGeometry());
}

/**
 * 从 QSettings 恢复用户设置
 */
void MainWindow::restoreSettings() {
    QSettings s("RK3588", "YOLO_Detector");
    if (s.contains("model_path"))
        model_edit_->setText(s.value("model_path").toString());
    for (int i = 0; i < MAX_CHANNELS; i++) {
        QString key = QString("video_path_%1").arg(i);
        if (s.contains(key))
            video_edits_[i]->setText(s.value(key).toString());
        QString alias_key = QString("video_alias_%1").arg(i);
        if (s.contains(alias_key))
            video_alias_edits_[i]->setText(s.value(alias_key).toString());
    }
    if (s.contains("thread_num"))
        thread_spin_->setValue(s.value("thread_num", 3).toInt());
    if (s.contains("conf_threshold")) {
        conf_threshold_ = s.value("conf_threshold", 0.25f).toFloat();
        conf_slider_->setValue((int)(conf_threshold_ * 100));
    }
    if (s.contains("nms_threshold")) {
        nms_threshold_ = s.value("nms_threshold", 0.45f).toFloat();
        nms_slider_->setValue((int)(nms_threshold_ * 100));
    }
    if (s.contains("geometry"))
        restoreGeometry(s.value("geometry").toByteArray());
}

/**
 * 将报警截图目录同步到 config.json
 *
 * 当用户在 GUI 中修改截图目录时，同步更新配置文件。
 *
 * @param dir 截图目录路径
 */
void MainWindow::syncAlarmScreenshotDirToConfig(const std::string &dir) {
    // 读取现有配置
    std::ifstream ifs("config.json");
    if (!ifs.is_open()) return;
    json j;
    try { ifs >> j; } catch (...) { return; }
    ifs.close();

    // 更新截图目录
    if (j.contains("websocket")) {
        j["websocket"]["alarm_screenshot_dir"] = dir;
    } else if (j.contains("alarm_screenshot_dir")) {
        j["alarm_screenshot_dir"] = dir;
    } else {
        j["alarm_screenshot_dir"] = dir;
    }

    // 写回配置文件
    std::ofstream ofs("config.json");
    if (ofs.is_open()) {
        ofs << j.dump(4);  // 缩进 4 空格，便于阅读
    }
}

/**
 * 更新缩放状态（单通道放大/缩小切换）
 *
 * - 无放大时：显示 2x2 网格
 * - 放大时：当前通道占满整个显示区，其他通道隐藏
 */
void MainWindow::updateZoomState() {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (!video_cells_[i].container) continue;

        if (expanded_ch_ == -1) {
            // 无放大：显示所有通道（2x2 网格）
            video_cells_[i].container->show();
            video_cells_[i].zoom_in_btn->show();
            video_cells_[i].zoom_out_btn->hide();
            grid_layout_->addWidget(video_cells_[i].container, i / 2, i % 2);
        } else if (i == expanded_ch_) {
            // 当前通道：占满整个显示区
            video_cells_[i].container->show();
            video_cells_[i].zoom_in_btn->hide();
            video_cells_[i].zoom_out_btn->show();
            grid_layout_->addWidget(video_cells_[i].container, 0, 0, 2, 2);
        } else {
            // 其他通道：隐藏
            video_cells_[i].container->hide();
        }
    }
    grid_layout_->invalidate();  // 刷新布局
}
