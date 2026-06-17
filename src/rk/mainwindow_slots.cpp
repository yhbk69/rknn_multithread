/*
 * mainwindow_slots.cpp - 所有槽函数实现
 */

#include "rk/mainwindow.hpp"
#include <QDateTime>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QTextStream>
#include <QNetworkInterface>

// ============================================================
// 缩放控制
// ============================================================

void MainWindow::onZoomIn(int ch) {
    if (ch < 0 || ch >= MAX_CHANNELS) return;
    expanded_ch_ = ch;
    updateZoomState();
    log("system", QString("通道 %1 放大显示").arg(ch + 1));
}

void MainWindow::onZoomOut() {
    expanded_ch_ = -1;
    updateZoomState();
    log("system", "恢复四分屏显示");
}

// ============================================================
// 截图
// ============================================================

void MainWindow::onScreenshot() {
    int ch = (expanded_ch_ >= 0) ? expanded_ch_ : 0;
    if (last_frames_[ch].isNull()) {
        log("system", "无可用帧进行截图。");
        return;
    }
    QString fileName = QFileDialog::getSaveFileName(this, "保存截图",
        QString("screenshot_ch%1_%2.png").arg(ch + 1).arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        "PNG (*.png);;JPEG (*.jpg)");
    if (!fileName.isEmpty()) {
        last_frames_[ch].save(fileName);
        log("system", QString("截图已保存: %1").arg(fileName));
    }
}

// ============================================================
// 模型/视频 浏览
// ============================================================

void MainWindow::onBrowseModel() {
    QString p = QFileDialog::getOpenFileName(this, "选择 RKNN 模型", "", "RKNN 模型 (*.rknn);;所有文件 (*.*)");
    if (!p.isEmpty()) model_edit_->setText(p);
}

void MainWindow::onLoadModel() {
    model_path_ = model_edit_->text().toStdString();
    if (model_path_.empty()) {
        QMessageBox::warning(this, "错误", "请选择模型文件。");
        return;
    }

    FILE* f = fopen(model_path_.c_str(), "rb");
    if (!f) {
        model_status_label_->setText("加载失败");
        model_status_label_->setStyleSheet("color: #e74c3c; font-weight: bold;");
        model_status_left_label_->setText("加载失败");
        model_status_left_label_->setStyleSheet("color: #e74c3c; font-weight: bold;");
        QMessageBox::warning(this, "错误", "模型文件未找到：" + QString::fromStdString(model_path_));
        return;
    }
    fseek(f, 0, SEEK_END);
    qint64 size = ftell(f);
    fclose(f);

    QFileInfo fi(QString::fromStdString(model_path_));
    model_status_label_->setText("已选择");
    model_status_label_->setStyleSheet("color: #4ec9b0; font-weight: bold;");
    model_status_left_label_->setText("已选择");
    model_status_left_label_->setStyleSheet("color: #4ec9b0; font-weight: bold;");
    model_name_label_->setText(fi.fileName());
    start_btn_->setEnabled(true);
    log("model", QString("已选择：%1 (%2)").arg(fi.fileName(), formatSize(size)));
}

void MainWindow::onBrowseVideo(int ch) {
    if (ch < 0 || ch >= MAX_CHANNELS) return;
    QString p = QFileDialog::getOpenFileName(this, QString("选择通道%1视频").arg(ch + 1),
        "", "视频文件 (*.mp4 *.avi *.mkv *.mov);;所有文件 (*.*)");
    if (!p.isEmpty()) video_edits_[ch]->setText(p);
}

// ============================================================
// 检测控制
// ============================================================

void MainWindow::onStartDetection() {
    if (model_path_.empty()) {
        QMessageBox::warning(this, "错误", "请先加载模型。");
        return;
    }

    int active_count = 0;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (!video_edits_[i]->text().trimmed().isEmpty())
            active_count++;
    }

    if (active_count == 0) {
        QMessageBox::warning(this, "错误", "请至少输入一个视频源。");
        return;
    }

    // P2-2: 根据活跃通道数动态调节 thread_num
    // 总实例数优化目标：4 路时每路 1 线程，3 路时每路 2 线程，1-2 路时每路 3 线程
    int user_set = thread_spin_->value();
    int recommended = (active_count >= 4) ? 1 : (active_count >= 3) ? 2 : 3;
    int thread_num = (user_set < recommended) ? user_set : recommended;
    if (thread_num < 1) thread_num = 1;
    export_records_.clear();

    for (int i = 0; i < MAX_CHANNELS; i++) {
        QString video_text = video_edits_[i]->text().trimmed();
        if (video_text.isEmpty()) continue;

        std::string video_path = video_text.toStdString();
        detect_threads_[i] = new DetectThread(i, model_path_, video_path, thread_num, conf_threshold_, nms_threshold_);
        detect_threads_[i]->setWebSocket(ws_server_);

        int ch = i;
        connect(detect_threads_[i], &DetectThread::frameReady, this,
            [this, ch](const QImage& img, double fps) { onFrameReady(ch, img, fps); });
        connect(detect_threads_[i], &DetectThread::statsUpdated, this,
            [this, ch](int frames, double fps, double inferTime) { onStatsUpdated(ch, frames, fps, inferTime); });
        connect(detect_threads_[i], &DetectThread::finished, this,
            [this, ch]() { onDetectFinished(ch); });
        connect(detect_threads_[i], &DetectThread::error, this,
            [this, ch](const QString& msg) { onDetectError(ch, msg); });
        connect(detect_threads_[i], &DetectThread::detectionResult, this,
            [this, ch](int frameId, const QString &cls, float conf, int l, int t, int r, int b) {
                export_records_.push_back({ch, frameId, cls.toStdString(), conf, l, t, r, b});
            });

        detect_threads_[i]->start();
        log("system", QString("通道%1 检测开始。来源：%2").arg(i + 1).arg(video_edits_[i]->text()));
    }

    log("system", QString("thread_num 自适应: %1 路 × %2 线程").arg(active_count).arg(thread_num));

    enableControls(true);
    model_status_label_->setText("运行中");
    model_status_label_->setStyleSheet("color: #ffb74d; font-weight: bold;");
    model_status_left_label_->setText("运行中");
    model_status_left_label_->setStyleSheet("color: #ffb74d; font-weight: bold;");
    status_label_->setText(QString("检测中... %1 路").arg(active_count));
}

void MainWindow::onStopDetection() {
    npu_usage_bar_->setValue(0);
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (detect_threads_[i]) {
            detect_threads_[i]->stop();
        }
    }
    log("system", "正在停止所有检测...");
    status_label_->setText("停止中...");
}

void MainWindow::onPauseToggle() {
    bool any_paused = false;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (detect_threads_[i] && detect_threads_[i]->isPaused()) {
            any_paused = true;
            break;
        }
    }
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (!detect_threads_[i]) continue;
        if (any_paused)
            detect_threads_[i]->resume();
        else
            detect_threads_[i]->pause();
    }
    pause_btn_->setText(any_paused ? "暂停" : "继续");
}

void MainWindow::onStepFrame() {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (detect_threads_[i]) {
            detect_threads_[i]->stepFrame();
        }
    }
}

// ============================================================
// 帧回调
// ============================================================

void MainWindow::onFrameReady(int ch, const QImage& image, double fps) {
    if (ch < 0 || ch >= MAX_CHANNELS) return;
    last_frames_[ch] = image;

    VideoCell& cell = video_cells_[ch];
    QPixmap pix = QPixmap::fromImage(image);
    cell.pixmap_item->setPixmap(pix);

    // P1-3: fitInView 只在尺寸变化时调用，避免每帧 layout 重算
    if (pix.size() != cell.last_pix_size) {
        cell.scene->setSceneRect(pix.rect());
        cell.view->fitInView(cell.pixmap_item, Qt::KeepAspectRatio);
        cell.last_pix_size = pix.size();
    }

    // P1-4: FPS 文本 5Hz 节流（每 200ms 更新一次）
    long long now = QDateTime::currentMSecsSinceEpoch();
    if (now - cell.last_fps_text_update >= 200) {
        cell.overlay->setText(QString("FPS: %1").arg(fps, 0, 'f', 1));
        cell.last_fps_text_update = now;
    }
    cell.last_fps = fps;
}

void MainWindow::onStatsUpdated(int ch, int frames, double fps, double inferTime) {
    if (ch < 0 || ch >= MAX_CHANNELS) return;
    // P1-4: 右侧面板 FPS/帧数 5Hz 节流
    long long now = QDateTime::currentMSecsSinceEpoch();
    static long long last_stats_update[MAX_CHANNELS] = {};
    if (now - last_stats_update[ch] < 200) return;
    last_stats_update[ch] = now;
    fps_labels_[ch]->setText(QString::number(fps, 'f', 1));
    frames_labels_[ch]->setText(QString::number(frames));

    int usage = 0;
    FILE *fp = fopen("/sys/class/devfreq/fdab0000.npu/load", "r");
    if (fp) {
        char buf[64] = {0};
        if (fgets(buf, sizeof(buf), fp))
            usage = qBound(0, atoi(buf), 100);
        fclose(fp);
    }
    npu_usage_bar_->setValue(usage);
}

void MainWindow::onDetectFinished(int ch) {
    if (ch < 0 || ch >= MAX_CHANNELS) return;
    if (detect_threads_[ch]) {
        detect_threads_[ch]->wait();
        delete detect_threads_[ch];
        detect_threads_[ch] = nullptr;
    }

    bool all_done = true;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (detect_threads_[i]) { all_done = false; break; }
    }

    if (all_done) {
        npu_usage_bar_->setValue(0);
        enableControls(false);
        model_status_label_->setText("已完成");
        model_status_label_->setStyleSheet("color: #8a9bb0; font-weight: bold;");
        model_status_left_label_->setText("已选择");
        model_status_left_label_->setStyleSheet("color: #4ec9b0; font-weight: bold;");
        status_label_->setText("所有通道检测完成。");
        log("system", "所有通道检测完成。");
    } else {
        log("system", QString("通道%1 检测完成。").arg(ch + 1));
    }
}

void MainWindow::onDetectError(int ch, const QString& msg) {
    log("info", msg);
}

// ============================================================
// 阈值 / 日志
// ============================================================

void MainWindow::onConfThresholdChanged(int value) {
    conf_threshold_ = value / 100.0f;
    updateThresholdLabels();
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (detect_threads_[i])
            detect_threads_[i]->setThresholds(conf_threshold_, nms_threshold_);
    }
}

void MainWindow::onNmsThresholdChanged(int value) {
    nms_threshold_ = value / 100.0f;
    updateThresholdLabels();
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (detect_threads_[i])
            detect_threads_[i]->setThresholds(conf_threshold_, nms_threshold_);
    }
}

void MainWindow::onClearLog() {
    log_edit_->clear();
}

// ============================================================
// 导出
// ============================================================

void MainWindow::onExportResults() {
    if (export_records_.empty()) {
        QMessageBox::information(this, "导出", "没有检测结果可导出。");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, "导出检测结果",
        QString("detection_%1.json").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        "JSON (*.json);;CSV (*.csv)");
    if (fileName.isEmpty()) return;

    if (fileName.endsWith(".json")) {
        QJsonArray arr;
        for (const auto& rec : export_records_) {
            QJsonObject obj;
            obj["channel"] = rec.channel + 1;
            obj["frame_id"] = rec.frame_id;
            obj["class"] = QString::fromStdString(rec.class_name);
            obj["confidence"] = rec.confidence;
            obj["left"] = rec.left;
            obj["top"] = rec.top;
            obj["right"] = rec.right;
            obj["bottom"] = rec.bottom;
            arr.append(obj);
        }
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(arr).toJson());
            file.close();
            log("system", QString("导出 %1 条检测结果到 %2").arg(export_records_.size()).arg(fileName));
        }
    } else {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&file);
            ts << "channel,frame_id,class,confidence,left,top,right,bottom\n";
            for (const auto& rec : export_records_) {
                ts << QString("%1,%2,%3,%4,%5,%6,%7,%8\n")
                      .arg(rec.channel + 1).arg(rec.frame_id)
                      .arg(QString::fromStdString(rec.class_name))
                      .arg(rec.confidence, 0, 'f', 4)
                      .arg(rec.left).arg(rec.top).arg(rec.right).arg(rec.bottom);
            }
            file.close();
            log("system", QString("导出 %1 条结果到 %2").arg(export_records_.size()).arg(fileName));
        }
    }
}

// ============================================================
// WebSocket 回调
// ============================================================

void MainWindow::onWebSocketStarted(quint16 port) {
    QString real_ip = "0.0.0.0";
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsUp) &&
            iface.flags().testFlag(QNetworkInterface::IsRunning) &&
            !iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    real_ip = entry.ip().toString();
                    break;
                }
            }
            if (real_ip != "0.0.0.0") break;
        }
    }
    ws_status_label_->setText(QString("ws://%1:%2").arg(real_ip).arg(port));
}

void MainWindow::onWebSocketClientConnected(QWebSocket *client) {
    Q_UNUSED(client);
    ws_clients_label_->setText(QString("客户端: %1").arg(ws_server_->clientCount()));
}

void MainWindow::onWebSocketClientDisconnected(QWebSocket *client) {
    Q_UNUSED(client);
    ws_clients_label_->setText(QString("客户端: %1").arg(ws_server_->clientCount()));
}

void MainWindow::onWebSocketAlarm(const QString &alarmId, const QString &alarmType,
                                   int frameId, long long timestampMs) {
    Q_UNUSED(alarmId); Q_UNUSED(frameId); Q_UNUSED(timestampMs);
    log("alarm", QString("报警: %1").arg(alarmType));
}

// ============================================================
// 配置热更新
// ============================================================

void MainWindow::onConfigFileChanged(const QString &path) {
    Q_UNUSED(path);
    log("system", "检测到 config.json 变化，重新加载配置。");
    AppConfig cfg = reload_config();
    conf_threshold_ = cfg.box_threshold;
    nms_threshold_ = cfg.nms_threshold;
    conf_slider_->setValue((int)(conf_threshold_ * 100));
    nms_slider_->setValue((int)(nms_threshold_ * 100));
    updateThresholdLabels();
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (detect_threads_[i])
            detect_threads_[i]->setThresholds(conf_threshold_, nms_threshold_);
    }
    config_watcher_->removePath(QFileInfo("config.json").absoluteFilePath());
    if (ws_server_) {
        WebSocketConfig wscfg = ws_server_->config();
        wscfg.alarm_screenshot_dir = cfg.alarm_screenshot_dir;
        ws_server_->updateConfig(wscfg);
    }
    config_watcher_->addPath(QFileInfo("config.json").absoluteFilePath());
}
