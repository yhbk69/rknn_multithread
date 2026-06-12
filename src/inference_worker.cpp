/**
 * @file inference_worker.cpp
 * @brief 推理工作线程实现
 */

#include "manager/inference_worker.hpp"
#include "pipeline/preprocessor.hpp"
#include "core/config.hpp"
#include <QThread>
#include <QDebug>
#include <QUuid>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <chrono>

InferenceWorker::InferenceWorker(IEngine* engine, int cameraId,
                                 const QString& cameraName)
    : engine_(engine), cameraId_(cameraId), cameraName_(cameraName) {}

void InferenceWorker::process(std::unique_ptr<IVideoSource> source,
                               float confThresh, float nmsThresh,
                               int inputW, int inputH) {
    if (!source) {
        emit finished(cameraId_);
        return;
    }

    running_ = true;
    cv::Mat frame;
    bool firstFrame = true;
    int64_t frameCount = 0;

    while (running_) {
        if (!source->readFrame(frame)) {
            if (firstFrame) {
                emit errorOccurred(cameraId_,
                    QString::fromUtf8("\u65e0\u6cd5\u6253\u5f00\u89c6\u9891\u6e90: ") + source->name());
            }
            break;
        }
        firstFrame = false;
        frameCount++;

        auto t0 = std::chrono::steady_clock::now();

        // 预处理
        cv::Mat processed = Preprocessor::letterbox(frame, inputW, inputH,
            Config::LETTERBOX_FILL_COLOR);
        std::vector<float> tensor = Preprocessor::imageToTensor(processed);

        // 推理
        std::vector<Detection> detections;
        engine_->infer(tensor, detections, frame.cols, frame.rows,
                       confThresh, nmsThresh);

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();

        // 绘制检测框
        cv::Mat display = frame.clone();
        for (const auto& det : detections) {
            cv::Scalar color(0, 255, 0);
            cv::rectangle(display,
                cv::Point(static_cast<int>(det.x), static_cast<int>(det.y)),
                cv::Point(static_cast<int>(det.x + det.w), static_cast<int>(det.y + det.h)),
                color, 2);
            char label[128];
            snprintf(label, sizeof(label), "class %d: %.2f", det.class_id, det.conf);
            cv::putText(display, label,
                cv::Point(static_cast<int>(det.x), static_cast<int>(det.y) - 5),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
        }

        // 转换为 QImage
        cv::cvtColor(display, display, cv::COLOR_BGR2RGB);
        QImage qimg(display.data, display.cols, display.rows,
                    display.step, QImage::Format_RGB888);

        emit frameProcessed(cameraId_, qimg.copy(), detections,
                            static_cast<double>(elapsed));

        // --- 环形缓冲区 + 报警检测 ---
        auto sharedFrame = std::make_shared<cv::Mat>(frame.clone());
        cv::cvtColor(*sharedFrame, *sharedFrame, cv::COLOR_BGR2RGB);
        {
            std::lock_guard<std::mutex> lk(bufferMutex_);
            frameBuffer_.push_back(sharedFrame);
            if ((int)frameBuffer_.size() > Config::RING_BUFFER_FRAMES)
                frameBuffer_.pop_front();
        }
        checkAlert(detections, sharedFrame);

        // 报警录制后续帧
        if (alertRecording_ && alertRemainingFrames_ > 0) {
            alertBuffer_.push_back(sharedFrame);
            alertRemainingFrames_--;
            if (alertRemainingFrames_ == 0) {
                saveAlertFiles(QUuid::createUuid().toString(QUuid::WithoutBraces),
                               pendingAlarmType_);
                alertRecording_ = false;
                alertBuffer_.clear();
                pendingAlarmType_.clear();
            }
        }

        // 帧率控制
        if (!source->isLive()) {
            auto procMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0).count();
            if (procMs < 80) QThread::msleep(80 - procMs);
        } else {
            cv::Mat skip; int n = 0;
            while (running_ && source->readFrame(skip) && n < 2) n++;
        }
    }
    qDebug() << "[InferenceWorker] camera" << cameraId_ << "finished";
    emit finished(cameraId_);
}

void InferenceWorker::checkAlert(const std::vector<Detection>& dets,
                                  const std::shared_ptr<cv::Mat>& frame) {
    if (alertRecording_ || alertClassIds_.empty()) return;
    for (const auto& det : dets) {
        // 检查是否在报警类别列表中
        bool match = false;
        for (int aid : alertClassIds_) {
            if (det.class_id == aid) { match = true; break; }
        }
        if (!match) continue;

        auto now = std::chrono::steady_clock::now();
        auto it = lastAlertTime_.find(det.class_id);
        if (it != lastAlertTime_.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - it->second).count();
            if (elapsed < Config::ALERT_COOLDOWN_MS) continue;
        }
        lastAlertTime_[det.class_id] = now;

        alertRecording_ = true;
        alertRemainingFrames_ = Config::ALERT_AFTER_FRAMES;
        pendingAlarmType_ = QString("class_%1").arg(det.class_id);
        triggerFrame_ = frame;

        {
            std::lock_guard<std::mutex> lk(bufferMutex_);
            alertBuffer_ = frameBuffer_;
        }
        alertBuffer_.push_back(frame);

        // 立即推送报警 JSON
        QString alarmId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QJsonObject data;
        data["alarm_id"] = alarmId;
        data["alarm_type"] = pendingAlarmType_;
        data["class_id"] = det.class_id;
        data["confidence"] = det.conf;
        data["timestamp"] = QDateTime::currentDateTime().toMSecsSinceEpoch();
        QJsonObject root;
        root["type"] = "alarm";
        root["data"] = data;
        emit alertSaved(cameraId_, QString(), QString(),
                        QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
        return;
    }
}

void InferenceWorker::saveAlertFiles(const QString& alarmId, const QString& alarmType) {
    if (alertBuffer_.empty()) return;
    auto toBgr = [](const std::shared_ptr<cv::Mat>& f) {
        cv::Mat bgr; cv::cvtColor(*f, bgr, cv::COLOR_RGB2BGR); return bgr;
    };
    int fw = alertBuffer_.back()->cols, fh = alertBuffer_.back()->rows;
    if (fw <= 0 || fh <= 0) return;
    QString base = QString("alarm_%1_%2").arg(alarmId).arg(alarmType);
    QString vp = outputDir_ + "/" + base + ".mp4";
    QString ip = outputDir_ + "/" + base + ".jpg";
    QDir().mkpath(outputDir_);
    cv::VideoWriter w(vp.toStdString(), cv::VideoWriter::fourcc('m','p','4','v'), 30.0, cv::Size(fw,fh));
    if (w.isOpened()) { for (auto& f : alertBuffer_) w.write(toBgr(f)); w.release(); }
    if (triggerFrame_) cv::imwrite(ip.toStdString(), toBgr(triggerFrame_));
    triggerFrame_.reset();
}

void InferenceWorker::stop() { running_ = false; }
