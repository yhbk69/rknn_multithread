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
#include <deque>
#include <mutex>
#include <unordered_map>
#include <chrono>
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

    void process(std::unique_ptr<IVideoSource> source,
                 float confThresh, float nmsThresh,
                 int inputW, int inputH);

    /// 设置报警类别 ID 列表
    void setAlertClassIds(const std::vector<int>& ids) { alertClassIds_ = ids; }
    /// 设置报警输出目录
    void setOutputDir(const QString& dir) { outputDir_ = dir; }

public slots:
    void setBatchInference(bool) {}
    void stop();

signals:
    void frameProcessed(int cameraId, QImage image,
                        std::vector<Detection> detections, double elapsedMs);
    void finished(int cameraId);
    void errorOccurred(int cameraId, const QString& message);
    /// 报警触发: cameraId, 视频路径, 截图路径, JSON
    void alertSaved(int cameraId, QString videoPath, QString imagePath, QString alertJson);

private:
    void checkAlert(const std::vector<Detection>& dets,
                    const std::shared_ptr<cv::Mat>& frame);
    void saveAlertFiles(const QString& alarmId, const QString& alarmType);

    IEngine* engine_;
    int cameraId_;
    QString cameraName_;
    std::atomic<bool> running_{false};

    // 报警状态
    std::vector<int> alertClassIds_;      // 触发报警的类别 ID
    QString outputDir_ = "output";
    std::deque<std::shared_ptr<cv::Mat>> frameBuffer_;  // 环形缓冲区
    std::mutex bufferMutex_;
    std::unordered_map<int, std::chrono::steady_clock::time_point> lastAlertTime_;
    std::atomic<bool> alertRecording_{false};
    int alertRemainingFrames_ = 0;
    std::deque<std::shared_ptr<cv::Mat>> alertBuffer_;
    QString pendingAlarmType_;
    std::shared_ptr<cv::Mat> triggerFrame_;
};

#endif // INFERENCE_WORKER_HPP
