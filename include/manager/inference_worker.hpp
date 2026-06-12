/**
 * @file inference_worker.hpp
 * @brief 推理工作线程
 *
 * 独立于 MainWindow 的后台推理线程，从视频源持续读帧、推理、推送结果。
 * 通过 Qt 信号槽将处理后的帧发送到主线程显示。
 */

#ifndef INFERENCE_WORKER_HPP
#define INFERENCE_WORKER_HPP

#include <QObject>
#include <QImage>
#include <atomic>
#include "core/types.hpp"
#include "engine/inference_engine.hpp"
#include "io/video_source.hpp"

class InferenceWorker : public QObject {
    Q_OBJECT

public:
    InferenceWorker(IEngine* engine, int cameraId, const QString& cameraName);
    ~InferenceWorker() override = default;

    int cameraId() const { return cameraId_; }
    QString cameraName() const { return cameraName_; }

    /// 主推理循环（阻塞，应在 QThread 中运行）
    void process(std::unique_ptr<IVideoSource> source,
                 float confThresh, float nmsThresh,
                 int inputW, int inputH);

public slots:
    void setBatchInference(bool) {}  // stub
    void stop();

signals:
    /// 帧处理完成: cameraId, 图像, 检测结果, 耗时(ms)
    void frameProcessed(int cameraId, QImage image,
                        std::vector<Detection> detections, double elapsedMs);
    void finished(int cameraId);
    void errorOccurred(int cameraId, const QString& message);

private:
    IEngine* engine_;
    int cameraId_;
    QString cameraName_;
    std::atomic<bool> running_{false};
};

#endif // INFERENCE_WORKER_HPP
