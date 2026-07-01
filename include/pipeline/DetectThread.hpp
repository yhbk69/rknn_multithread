#ifndef DETECT_THREAD_HPP
#define DETECT_THREAD_HPP

#include <QThread>
#include <QRect>
#include <QVector>
#include <QMutex>
#include <atomic>
#include <memory>
#include <string>
#include <map>
#include <vector>

#include <opencv2/core.hpp>
#include <nlohmann/json.hpp>

#include "config_loader.hpp"
#include "pipeline/CascadePipeline.hpp"
#include "pipeline/FrameReader.hpp"
#include "pipeline/StatsCollector.hpp"
#include "pipeline/ResultRenderer.hpp"
#include "pipeline/FrameDetections.hpp"
#include "pipeline/FrameQueue.hpp"

class WebSocket;
class VideoRecorder;

class DetectThread : public QThread {
    Q_OBJECT
public:
    DetectThread(int channel_id, const std::string& model_path, const std::string& video_path,
                 int thread_num = 3, float conf_threshold = 0.25f, float nms_threshold = 0.45f)
        : channel_id_(channel_id), model_path_(model_path), video_path_(video_path),
          thread_num_(thread_num), conf_threshold_(conf_threshold), nms_threshold_(nms_threshold),
          running_(true) {
        single_cfg_.path = model_path;
        single_cfg_.input_width = 640;
        single_cfg_.input_height = 640;
        single_cfg_.thread_num = thread_num;
        single_cfg_.type = "yolov5";
        single_cfg_.draw_result = true;
    }

    void setCascadeModels(const std::vector<ModelConfig>& models) {
        cascade_models_ = models;
        use_cascade_ = true;
    }

    void stop() { running_.store(false); }
    void pause() { paused_.store(true); }
    void resume() { paused_.store(false); }
    void stepFrame() { step_once_.store(true); paused_.store(false); }
    bool isPaused() const { return paused_.load(); }
    void set_thread_num(int n) { thread_num_ = n; }
    void setThresholds(float conf, float nms) {
        conf_threshold_ = conf;
        nms_threshold_ = nms;
    }
    void setWebSocket(std::weak_ptr<WebSocket> ws) { ws_ = ws; }
    void setRoi(const QRect& roi) { roi_rect_ = roi; roi_enabled_ = !roi.isNull(); }
    void clearRoi() { roi_rect_ = QRect(); roi_enabled_ = false; }
    int channelId() const { return channel_id_; }
    void setFrameQueue(FrameQueue* q) { frame_queue_ = q; }
    void setRecorder(VideoRecorder* rec) { recorder_ = rec; }
    void setSkipFrames(bool skip) { skip_frames_ = skip; }

signals:
    void statsUpdated(int framesProcessed, double avgFps, double inferenceTime);
    void finished();
    void error(const QString& msg);
    void detectionBatch(int frameId, const QVector<FrameDetections::Det>& dets);
    void statsPanelUpdated(long long totalAlarms, const QString& classStatsJson);
    void frameDropped(int channel, int totalDropped);

protected:
    void run() override;

private:
    int channel_id_;
    std::string model_path_;
    std::string video_path_;
    int thread_num_;
    float conf_threshold_;
    float nms_threshold_;
    std::atomic<bool> running_;
    std::atomic<bool> paused_{false};
    std::atomic<bool> step_once_{false};
    std::weak_ptr<WebSocket> ws_;
    FrameQueue* frame_queue_ = nullptr;
    QRect roi_rect_;
    bool roi_enabled_ = false;
    ModelConfig single_cfg_;
    bool use_cascade_ = false;
    std::vector<ModelConfig> cascade_models_;
    std::map<std::string, long long> class_counts_;
    std::map<std::string, long long> alarm_counts_;
    VideoRecorder* recorder_ = nullptr;  // 视频录制器（可选）
    bool skip_frames_ = false;  // 跳帧模式：流水线忙时丢弃新帧
    int dropped_frames_ = 0;    // 已丢弃帧数
};

#endif // DETECT_THREAD_HPP
