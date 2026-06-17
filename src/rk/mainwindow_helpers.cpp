/*
 * mainwindow_helpers.cpp - 辅助函数、设置保存/恢复、日志
 */

#include "rk/mainwindow.hpp"
#include <QDateTime>
#include <QSettings>
#include <fstream>
#include "nlohmann/json.hpp"
using json = nlohmann::json;

QString MainWindow::currentTimestamp() {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
}

void MainWindow::log(const QString& category, const QString& message) {
    log_edit_->append(QString("[%1][%2] %3").arg(currentTimestamp(), category, message));
}

void MainWindow::updateThresholdLabels() {
    conf_value_label_->setText(QString::number(conf_threshold_, 'f', 2));
    nms_value_label_->setText(QString::number(nms_threshold_, 'f', 2));
}

void MainWindow::enableControls(bool running) {
    start_btn_->setEnabled(!running);
    stop_btn_->setEnabled(running);
    pause_btn_->setEnabled(running);
    step_btn_->setEnabled(running);
    screenshot_btn_->setEnabled(running);
    export_btn_->setEnabled(!running);
    model_btn_->setEnabled(!running);
    load_btn_->setEnabled(!running);
    for (int i = 0; i < MAX_CHANNELS; i++)
        video_btns_[i]->setEnabled(!running);
    thread_spin_->setEnabled(!running);
}

QString MainWindow::formatSize(qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
}

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

void MainWindow::syncAlarmScreenshotDirToConfig(const std::string &dir) {
    std::ifstream ifs("config.json");
    if (!ifs.is_open()) return;
    json j;
    try { ifs >> j; } catch (...) { return; }
    ifs.close();

    if (j.contains("websocket")) {
        j["websocket"]["alarm_screenshot_dir"] = dir;
    } else if (j.contains("alarm_screenshot_dir")) {
        j["alarm_screenshot_dir"] = dir;
    } else {
        j["alarm_screenshot_dir"] = dir;
    }

    std::ofstream ofs("config.json");
    if (ofs.is_open()) {
        ofs << j.dump(4);
    }
}

void MainWindow::updateZoomState() {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (!video_cells_[i].container) continue;
        if (expanded_ch_ == -1) {
            video_cells_[i].container->show();
            video_cells_[i].zoom_in_btn->show();
            video_cells_[i].zoom_out_btn->hide();
            grid_layout_->addWidget(video_cells_[i].container, i / 2, i % 2);
        } else if (i == expanded_ch_) {
            video_cells_[i].container->show();
            video_cells_[i].zoom_in_btn->hide();
            video_cells_[i].zoom_out_btn->show();
            grid_layout_->addWidget(video_cells_[i].container, 0, 0, 2, 2);
        } else {
            video_cells_[i].container->hide();
        }
    }
    grid_layout_->invalidate();
}
