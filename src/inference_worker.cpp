/**
 * @file inference_worker.cpp
 * @brief 推理工作线程实现
 */

#include "manager/inference_worker.hpp"
#include "pipeline/preprocessor.hpp"
#include "core/config.hpp"
#include <QThread>
#include <QDebug>
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

        // 实时源丢积压帧降低延迟
        if (source->isLive()) {
            cv::Mat skip;
            int drained = 0;
            while (running_ && source->readFrame(skip) && drained < 5) {
                drained++;
            }
        }
    }

    qDebug() << "[InferenceWorker] camera" << cameraId_
             << "finished, frames:" << frameCount;
    emit finished(cameraId_);
}

void InferenceWorker::stop() {
    running_ = false;
}
