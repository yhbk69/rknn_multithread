/*
 * mainwindow_slots.cpp - 所有槽函数实现
 */

#include "rk/mainwindow.hpp"
#include "pipeline/VideoRecorder.hpp"
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
    // 重置各通道统计更新时间，避免重启后前几帧被误节流
    for (int i = 0; i < MAX_CHANNELS; i++)
        last_stats_update_[i] = 0;

    for (int i = 0; i < MAX_CHANNELS; i++) {
        QString video_text = video_edits_[i]->text().trimmed();
        if (video_text.isEmpty()) continue;

        std::string video_path = video_text.toStdString();
        detect_threads_[i] = new DetectThread(i, model_path_, video_path, thread_num, conf_threshold_, nms_threshold_);
        detect_threads_[i]->setWebSocket(ws_server_);

        int ch = i;
        /* 设置共享帧队列，工作线程直接写入 */
        detect_threads_[i]->setFrameQueue(&frame_queues_[i]);
        connect(detect_threads_[i], &DetectThread::statsUpdated, this,
            [this, ch](int frames, double fps, double inferTime) { onStatsUpdated(ch, frames, fps, inferTime); });
        connect(detect_threads_[i], &DetectThread::finished, this,
            [this, ch]() { onDetectFinished(ch); });
        connect(detect_threads_[i], &DetectThread::error, this,
            [this, ch](const QString& msg) { onDetectError(ch, msg); });
        /* P1-3: 连接新的 detectionBatch 信号（每帧一次批量结果） */
        connect(detect_threads_[i], &DetectThread::detectionBatch, this,
            [this, ch](int frameId, const QVector<FrameDetections::Det>& dets) {
                onDetectionBatch(ch, frameId, dets);
            });
        /* 连接报警统计面板更新信号 */
        connect(detect_threads_[i], &DetectThread::statsPanelUpdated, this,
            [this, ch](long long totalAlarms, const QString& classStatsJson) {
                onStatsPanelUpdated(ch, totalAlarms, classStatsJson);
            });

        detect_threads_[i]->start();
        log("system", QString("通道%1 检测开始。来源：%2").arg(i + 1).arg(video_edits_[i]->text()));
    }

    log("system", QString("thread_num 自适应: %1 路 × %2 线程").arg(active_count).arg(thread_num));

    enableControls(true);
    record_btn_->setEnabled(true);
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
// 帧队列轮询（QTimer 驱动，GUI 线程执行）
//
// 每 16ms 被调用一次，从所有通道的共享队列中取帧：
//   1. cv::Mat BGR → QImage RGB（.copy() 深拷贝）
//   2. 更新 QGraphicsPixmapItem 显示
//   3. fitInView 只在尺寸变化时调用
//   4. FPS 文本 5Hz 节流
// ============================================================
void MainWindow::onPollFrames() {
    for (int ch = 0; ch < MAX_CHANNELS; ch++) {
        FrameQueue::Entry e;
        if (!frame_queues_[ch].pop(e)) continue;
        if (e.bgr_frame.empty()) continue;

        /* BGR→RGB 转换 + QImage 深拷贝 */
        cv::Mat rgb;
        cv::cvtColor(e.bgr_frame, rgb, cv::COLOR_BGR2RGB);
        if (rgb.empty()) continue;

        QImage qimg(rgb.data, rgb.cols, rgb.rows,
                    static_cast<int>(rgb.step), QImage::Format_RGB888);
        QImage owned = qimg.copy(); /* 深拷贝，确保数据独立于 cv::Mat */

        last_frames_[ch] = owned;

        VideoCell& cell = video_cells_[ch];
        QPixmap pix = QPixmap::fromImage(owned);
        cell.pixmap_item->setPixmap(pix);

        /* fitInView 只在尺寸变化时调用，避免每帧 layout 重算 */
        if (pix.size() != cell.last_pix_size) {
            cell.scene->setSceneRect(pix.rect());
            cell.view->fitInView(cell.pixmap_item, Qt::KeepAspectRatio);
            cell.last_pix_size = pix.size();
        }

        /* FPS 文本 5Hz 节流（每 200ms 更新一次） */
        long long now = QDateTime::currentMSecsSinceEpoch();
        if (now - cell.last_fps_text_update >= 200) {
            cell.overlay->setText(QString("FPS: %1").arg(e.fps, 0, 'f', 1));
            cell.last_fps_text_update = now;
        }
        cell.last_fps = e.fps;
    }
}

void MainWindow::onStatsUpdated(int ch, int frames, double fps, double inferTime) {
    if (ch < 0 || ch >= MAX_CHANNELS) return;
    // P1-4: 右侧面板 FPS/帧数 5Hz 节流
    long long now = QDateTime::currentMSecsSinceEpoch();
    if (now - last_stats_update_[ch] < 200) return;
    last_stats_update_[ch] = now;
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
        // 停止录制
        if (record_btn_->isChecked()) {
            record_btn_->setChecked(false);
            onRecordToggle();
        }
        record_btn_->setEnabled(false);
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

/* P1-3: 批量检测结果回调（每帧一次，替代逐框发射） */
void MainWindow::onDetectionBatch(int ch, int frameId, const QVector<FrameDetections::Det>& dets) {
    for (const auto &d : dets) {
        export_records_.push_back({ch, frameId, d.class_name.toStdString(),
                                   d.confidence, d.left, d.top, d.right, d.bottom});
    }
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
    ws_status_label_->setStyleSheet("color: #4ec9b0; font-weight: bold;");
}

void MainWindow::onWebSocketClientConnected(QWebSocket *client) {
    Q_UNUSED(client);
    ws_clients_label_->setText(QString("WS 客户端: %1").arg(ws_server_->clientCount()));
}

void MainWindow::onWebSocketClientDisconnected(QWebSocket *client) {
    Q_UNUSED(client);
    ws_clients_label_->setText(QString("WS 客户端: %1").arg(ws_server_->clientCount()));
}

void MainWindow::onWebSocketAlarm(const QString &alarmId, const QString &alarmType,
                                   int frameId, long long timestampMs) {
    Q_UNUSED(alarmId); Q_UNUSED(frameId); Q_UNUSED(timestampMs);
    log("alarm", QString("报警: %1").arg(alarmType));
}

// ============================================================
// 报警统计面板更新
// ============================================================

void MainWindow::onStatsPanelUpdated(int ch, long long totalAlarms, const QString& classStatsJson) {
    Q_UNUSED(ch);

    // 更新报警总数（如果有的话）
    if (stats_alarm_label_) {
        stats_alarm_label_->setText(QString(" 报警: %1 ").arg(totalAlarms));
        if (totalAlarms > 0)
            stats_alarm_label_->setStyleSheet(
                "background-color: #922b21; color: #ffffff; border-radius: 3px; "
                "padding: 2px 8px; font-weight: bold; font-size: 11px;");
        else
            stats_alarm_label_->setStyleSheet(
                "background-color: #2c3e50; color: #ecf0f1; border-radius: 3px; "
                "padding: 2px 8px; font-size: 11px;");
    }

    // 解析类别 JSON
    QJsonDocument doc = QJsonDocument::fromJson(classStatsJson.toUtf8());
    if (!doc.isObject()) return;
    QJsonObject obj = doc.object();

    // 先把所有标签重置为灰色（未检测到）
    for (auto& [name, lbl] : stats_class_labels_) {
        lbl->setText(QString::fromStdString(name) + ": 0");
        lbl->setStyleSheet(
            "background-color: #2c2c3e; color: #666; border-radius: 3px; "
            "padding: 2px 6px; font-size: 11px;");
    }

    // 高亮有检测结果的类别
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        std::string cls = it.key().toStdString();
        long long count = it.value().toVariant().toLongLong();
        auto lit = stats_class_labels_.find(cls);
        if (lit != stats_class_labels_.end()) {
            lit->second->setText(QString::fromStdString(cls) + ": " + QString::number(count));
            lit->second->setStyleSheet(
                "background-color: #1a5276; color: #2ecc71; border-radius: 3px; "
                "padding: 2px 6px; font-weight: bold; font-size: 11px;");
        }
    }
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

// ============================================================
// 视频录制
// ============================================================

void MainWindow::onRecordToggle() {
    if (record_btn_->isChecked()) {
        // 开始录制：为每个活跃通道创建录制器
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        bool any_recording = false;

        for (int i = 0; i < MAX_CHANNELS; i++) {
            if (!detect_threads_[i]) continue;
            QString video_text = video_edits_[i]->text().trimmed();
            if (video_text.isEmpty()) continue;

            // 获取视频帧尺寸
            QImage frame = last_frames_[i];
            if (frame.isNull()) continue;

            QString path = QString("record_ch%1_%2.mp4").arg(i + 1).arg(timestamp);
            recorders_[i] = std::make_shared<VideoRecorder>();
            if (recorders_[i]->open(path.toStdString(), 25, frame.width(), frame.height())) {
                detect_threads_[i]->setRecorder(recorders_[i].get());
                any_recording = true;
                log("system", QString("通道%1 开始录制: %2").arg(i + 1).arg(path));
            } else {
                log("system", QString("通道%1 录制失败: 无法打开 %2").arg(i + 1).arg(path));
                recorders_[i].reset();
            }
        }

        if (any_recording) {
            record_btn_->setText("停止录制");
            record_btn_->setStyleSheet("background-color: #e74c3c; color: white; font-weight: bold;");
        } else {
            record_btn_->setChecked(false);
            log("system", "无可用帧进行录制。");
        }
    } else {
        // 停止录制
        for (int i = 0; i < MAX_CHANNELS; i++) {
            if (recorders_[i]) {
                detect_threads_[i]->setRecorder(nullptr);
                recorders_[i]->close();
                log("system", QString("通道%1 停止录制，共 %2 帧")
                    .arg(i + 1).arg(recorders_[i]->frameCount()));
                recorders_[i].reset();
            }
        }
        record_btn_->setText("录制");
        record_btn_->setStyleSheet("");
    }
}
