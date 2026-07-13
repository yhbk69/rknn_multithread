/*
 * DetectThread.hpp - 单通道检测线程
 *
 * 封装一个完整的检测流程：读帧 → NPU推理 → 报警 → UI更新
 * 每个通道一个 DetectThread 实例，支持暂停/单帧/跳帧控制。
 *
 * 数据流：
 *   FrameReader → CascadePipeline.put() → NPU推理
 *     → CascadePipeline.get() → 检测结果处理
 *       → WebSocket报警 → UI显示 → 帧队列
 *
 * 线程安全：
 *   - stop()/pause()/resume() 通过 atomic 变量控制，线程安全
 *   - 通过 Qt 信号槽机制与主线程通信
 */

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
    /*
     * 构造函数
     * @param channel_id    通道编号（0-3，对应4路检测）
     * @param model_path    RKNN 模型文件路径
     * @param video_path    视频源路径（文件/摄像头ID/RTSP URL）
     * @param thread_num    NPU 推理线程数（= 模型实例数）
     * @param conf_threshold 置信度阈值
     * @param nms_threshold  NMS IoU 阈值
     */
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

    /* 设置级联模型配置（多模型场景） */
    void setCascadeModels(const std::vector<ModelConfig>& models) {
        cascade_models_ = models;
        use_cascade_ = true;
    }

    /* 线程控制 */
    void stop() { running_.store(false); }            // 停止检测线程
    void pause() { paused_.store(true); }             // 暂停检测
    void resume() { paused_.store(false); }           // 恢复检测
    void stepFrame() { step_once_.store(true); paused_.store(false); }  // 单帧步进
    bool isPaused() const { return paused_.load(); }  // 查询暂停状态

    /* 参数设置 */
    void set_thread_num(int n) { thread_num_ = n; }   // 设置 NPU 线程数
    void setThresholds(float conf, float nms) {
        conf_threshold_ = conf;
        nms_threshold_ = nms;
    }
    void setWebSocket(std::weak_ptr<WebSocket> ws) { ws_ = ws; }  // 绑定 WebSocket 服务
    void setRoi(const QRect& roi) { roi_rect_ = roi; roi_enabled_ = !roi.isNull(); }  // 设置 ROI 区域
    void clearRoi() { roi_rect_ = QRect(); roi_enabled_ = false; }  // 清除 ROI
    int channelId() const { return channel_id_; }      // 获取通道编号
    void setFrameQueue(FrameQueue* q) { frame_queue_ = q; }  // 绑定帧队列（用于 GUI 显示）
    void setRecorder(VideoRecorder* rec) { recorder_ = rec; }  // 绑定录制器（可选）
    void setSkipFrames(bool skip) { skip_frames_ = skip; }    // 启用跳帧模式

signals:
    /* 统计更新（每5帧触发） */
    void statsUpdated(int framesProcessed, double avgFps, double inferenceTime);
    /* 线程完成 */
    void finished();
    /* 错误/日志信息 */
    void error(const QString& msg);
    /* 检测结果批量推送（每帧触发，含所有检测框） */
    void detectionBatch(int frameId, const QVector<FrameDetections::Det>& dets);
    /* 统计面板更新（每30帧触发，含报警总数和类别统计JSON） */
    void statsPanelUpdated(long long totalAlarms, const QString& classStatsJson);
    /* 丢帧通知（跳帧模式下触发） */
    void frameDropped(int channel, int totalDropped);

protected:
    void run() override;  // 线程主循环：读帧→推理→报警→UI更新

private:
    /* 通道与模型配置 */
    int channel_id_;                    // 通道编号（0-3）
    std::string model_path_;            // RKNN 模型文件路径
    std::string video_path_;            // 视频源路径
    int thread_num_;                    // NPU 推理线程数
    float conf_threshold_;              // 置信度阈值
    float nms_threshold_;               // NMS IoU 阈值

    /* 线程控制标志 */
    std::atomic<bool> running_;         // 运行标志（false=停止）
    std::atomic<bool> paused_{false};   // 暂停标志
    std::atomic<bool> step_once_{false};// 单帧步进标志

    /* 外部服务绑定 */
    std::weak_ptr<WebSocket> ws_;       // WebSocket 服务（弱引用避免循环）
    FrameQueue* frame_queue_ = nullptr; // 帧队列（用于 GUI 显示）
    VideoRecorder* recorder_ = nullptr; // 视频录制器（可选）

    /* ROI 围栏 */
    QRect roi_rect_;                    // ROI 矩形区域
    bool roi_enabled_ = false;          // ROI 是否启用

    /* 模型配置 */
    ModelConfig single_cfg_;            // 单模型配置
    bool use_cascade_ = false;          // 是否使用级联模式
    std::vector<ModelConfig> cascade_models_;  // 级联模型配置列表

    /* 统计计数器 */
    std::map<std::string, long long> class_counts_;   // 各类别检测计数
    std::map<std::string, long long> alarm_counts_;   // 各类别报警计数

    /* 跳帧控制 */
    bool skip_frames_ = false;          // 跳帧模式：流水线忙时丢弃新帧
    int dropped_frames_ = 0;            // 已丢弃帧数
};

#endif // DETECT_THREAD_HPP
