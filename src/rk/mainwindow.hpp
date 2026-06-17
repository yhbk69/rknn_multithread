/*
 * mainwindow.hpp - RK3588 Qt GUI 主窗口
 * 四路视频 2x2 网格布局，支持单路放大/缩小
 */

#ifndef RK_MAINWINDOW_HPP
#define RK_MAINWINDOW_HPP

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QSlider>
#include <QSpinBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QThread>
#include <QImage>
#include <QDateTime>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QStatusBar>
#include <QPixmap>
#include <QSplitter>
#include <QListWidget>
#include <QComboBox>
#include <QProgressBar>
#include <QApplication>
#include <QScreen>
#include <QSettings>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QShortcut>
#include <QNetworkInterface>
#include <QAbstractSocket>
#include <QFileSystemWatcher>
#include <atomic>
#include <cstdio>
#include <memory>
#include <mutex>
#include <vector>
#include <array>
#include <chrono>

#include <opencv2/opencv.hpp>

#include "rkYolov5s.hpp"   // YOLOv5Engine
#include "rknnPool.hpp"
#include "config_loader.hpp"
#include "postprocess.h"
#include "websocket.hpp"
#include "pipeline/FrameReader.hpp"
#include "pipeline/StatsCollector.hpp"
#include "pipeline/ResultRenderer.hpp"
#include "pipeline/CascadePipeline.hpp"

static constexpr int MAX_CHANNELS = 4;

/* 单帧检测结果（用于批量传递，避免每个检测框发射一次信号） */
struct FrameDetections {
    int frame_id = 0;
    struct Det {
        QString class_name;
        float confidence = 0;
        int left = 0, top = 0, right = 0, bottom = 0;
    };
    QVector<Det> detections;
};

// ============================================================
// 检测工作线程（每路摄像头一个实例）
//
// 职责：读取视频帧 → 送入 CascadePipeline 推理 → 将结果传递给 GUI 线程
//
// 性能优化（Phase 1）：
//   P1-1: 移除 QThread::msleep(1)，避免人为阻塞
//   P1-2: 传递 cv::Mat 而非 QImage，GUI 线程按需转换，避免工作线程做 BGR→RGB + copy
//   P1-3: 每帧只发射一个 batch 信号，替代每个检测框一个信号
// ============================================================
class DetectThread : public QThread {
    Q_OBJECT
public:
    DetectThread(int channel_id, const std::string& model_path, const std::string& video_path,
                 int thread_num = 3, float conf_threshold = 0.25f, float nms_threshold = 0.45f)
        : channel_id_(channel_id), model_path_(model_path), video_path_(video_path),
          thread_num_(thread_num), conf_threshold_(conf_threshold), nms_threshold_(nms_threshold),
          running_(true) {
        /* 构造单模型 ModelConfig 用于 CascadePipeline */
        single_cfg_.path = model_path;
        single_cfg_.input_width = 640;
        single_cfg_.input_height = 640;
        single_cfg_.thread_num = thread_num;
        single_cfg_.type = "yolov5";
        single_cfg_.draw_result = true;
    }

    void setCascadeModels(const std::vector<ModelConfig> &models) {
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
    void setRoi(const QRect &roi) { roi_rect_ = roi; roi_enabled_ = !roi.isNull(); }
    void clearRoi() { roi_rect_ = QRect(); roi_enabled_ = false; }
    int channelId() const { return channel_id_; }

signals:
    /* 传递 QImage 给 GUI 线程显示 */
    void frameReady(const QImage& image, double fps);
    void statsUpdated(int framesProcessed, double avgFps, double inferenceTime);
    void finished();
    void error(const QString& msg);
    /* P1-3: 每帧一次批量检测结果信号，替代逐框发射 */
    void detectionBatch(int frameId, const QVector<FrameDetections::Det>& dets);

protected:
    void run() override {
        /* 初始化流水线：单模型（rknnPool 兼容）或级联模式 */
        auto pipeline = std::make_unique<CascadePipeline>();
        std::vector<ModelConfig> model_configs;
        if (use_cascade_)
            model_configs = cascade_models_;
        else
            model_configs.push_back(single_cfg_);
        if (pipeline->init(model_configs, channel_id_) != 0) {
            emit error(QString("[Ch%1] CascadePipeline init failed!").arg(channel_id_));
            emit finished();
            return;
        }
        pipeline->set_thresholds(conf_threshold_, nms_threshold_);

        /* 打开视频源（文件/摄像头/RTSP） */
        FrameReader reader(video_path_);
        if (!reader.open()) {
            emit error(QString("[Ch%1] Cannot open video: %2").arg(channel_id_).arg(QString::fromStdString(video_path_)));
            emit finished();
            return;
        }

        StatsCollector stats;
        stats.start();
        ResultRenderer renderer;

        int frames = 0;

        while (running_.load()) {
            /* 暂停状态下等待恢复或单步触发 */
            while (paused_.load() && running_.load()) {
                if (step_once_.exchange(false)) break;
                QThread::msleep(50);
            }
            if (!running_.load()) break;

            /* 1. 读取一帧 */
            cv::Mat img;
            if (!reader.read(img)) {
                emit error(QString("[Ch%1] Failed to read frame").arg(channel_id_));
                break;
            }

            long long infer_start = stats.elapsedMs();

            /* 2. ROI 裁剪（如果启用了 ROI 模式） */
            int roi_x = 0, roi_y = 0;
            cv::Mat detect_img;
            if (roi_enabled_ && !roi_rect_.isNull()) {
                int x1 = qBound(0, roi_rect_.x(), img.cols - 1);
                int y1 = qBound(0, roi_rect_.y(), img.rows - 1);
                int x2 = qBound(x1 + 1, roi_rect_.x() + roi_rect_.width(), img.cols);
                int y2 = qBound(y1 + 1, roi_rect_.y() + roi_rect_.height(), img.rows);
                roi_x = x1; roi_y = y1;
                detect_img = img(cv::Range(y1, y2), cv::Range(x1, x2)).clone();
            } else {
                detect_img = img;
            }

            /* 3. 送入推理流水线（非阻塞） */
            if (pipeline->put(detect_img) != 0) break;

            /* 4. 等待推理结果（阻塞，直到 NPU 完成） */
            if (frames >= thread_num_ && pipeline->get(detect_img) != 0) break;

            /* 5. 处理推理结果（仅在流水线预热完成后） */
            if (frames >= thread_num_) {
                detect_result_group_t result = pipeline->getLastDetectResult();

                /* 5a. ROI 坐标偏移还原 */
                if (roi_enabled_ && !roi_rect_.isNull()) {
                    for (int i = 0; i < result.count; i++) {
                        result.results[i].box.left += roi_x;
                        result.results[i].box.right += roi_x;
                        result.results[i].box.top += roi_y;
                        result.results[i].box.bottom += roi_y;
                    }
                    detect_img.copyTo(img(cv::Range(roi_y, roi_y + detect_img.rows),
                                          cv::Range(roi_x, roi_x + detect_img.cols)));
                }

                /* 5b. WebSocket 报警检查（仅在启用且有报警类别时执行） */
                if (auto ws = ws_.lock()) {
                    if (ws->isAlarmEnabled())
                        ws->checkAndAlarm(&result, frames, detect_img);
                }

                /* 5c. P1-3: 批量收集检测结果，每帧只发射一次信号 */
                QVector<FrameDetections::Det> dets;
                for (int i = 0; i < result.count; i++) {
                    const detect_result_t &det = result.results[i];
                    dets.append({QString::fromUtf8(det.name), det.prop,
                                 det.box.left, det.box.top, det.box.right, det.box.bottom});
                }
                if (!dets.isEmpty())
                    emit detectionBatch(frames, dets);
            }

            /* 6. 绘制 FPS 文字到帧上 */
            cv::Mat& out_frame = (frames >= thread_num_) ? detect_img : img;
            double infer_time = (double)(stats.elapsedMs() - infer_start);
            double current_fps = stats.updateFps(frames);
            renderer.drawFps(out_frame, current_fps);

            /* 7. 传递 QImage 给 GUI 线程（BGR→RGB 转换 + 深拷贝） */
            emit frameReady(renderer.toQImage(out_frame), current_fps);

            /* 8. 定期发射统计信息（每 5 帧一次） */
            if (frames % 5 == 0)
                emit statsUpdated(frames, current_fps, infer_time);

            frames++;
            /* P1-1: 移除 msleep(1)，不再人为阻塞，pipeline.get() 已提供同步 */
        }

        /* 排空 pipeline 中剩余的推理结果 */
        while (true) {
            cv::Mat img;
            if (pipeline->get(img) != 0) break;
            emit frameReady(renderer.toQImage(img), stats.getCurrentFps());
        }

        double avg_fps = stats.calcAvgFps(frames);
        emit statsUpdated(frames, avg_fps, 0);
        emit error(QString("[Ch%1] Finished. Frames: %2, Avg FPS: %3")
                       .arg(channel_id_).arg(frames).arg(avg_fps, 0, 'f', 2));
        emit finished();
    }

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
    QRect roi_rect_;
    bool roi_enabled_ = false;
    ModelConfig single_cfg_;
    bool use_cascade_ = false;
    std::vector<ModelConfig> cascade_models_;
};

// ============================================================
// 单路视频显示单元
// ============================================================
struct VideoCell {
    QWidget* container = nullptr;
    QGraphicsView* view = nullptr;
    QGraphicsScene* scene = nullptr;
    QGraphicsPixmapItem* pixmap_item = nullptr;
    QLabel* overlay = nullptr;
    QPushButton* zoom_in_btn = nullptr;
    QPushButton* zoom_out_btn = nullptr;
    QLabel* channel_label = nullptr;
    QImage last_frame;
    double last_fps = 0.0;
    QSize last_pix_size;                       // 用于 fitInView 尺寸变化检测
    long long last_fps_text_update = 0;        // 用于 FPS 文本 5Hz 节流
};

// ============================================================
// MainWindow: RK3588 主窗口 (四路 2x2 网格)
// ============================================================
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onBrowseModel();
    void onLoadModel();
    void onBrowseVideo(int ch);
    void onStartDetection();
    void onStopDetection();
    void onFrameReady(int ch, const QImage& image, double fps);
    void onStatsUpdated(int ch, int frames, double fps, double inferTime);
    void onDetectFinished(int ch);
    void onDetectError(int ch, const QString& msg);
    void onConfThresholdChanged(int value);
    void onNmsThresholdChanged(int value);
    void onClearLog();
    void onPauseToggle();
    void onStepFrame();
    void onScreenshot();
    void onExportResults();
    /* P1-3: 接收批量检测结果（每帧一次） */
    void onDetectionBatch(int ch, int frameId, const QVector<FrameDetections::Det>& dets);

    // 缩放
    void onZoomIn(int ch);
    void onZoomOut();

    // WebSocket 相关
    void onWebSocketStarted(quint16 port);
    void onWebSocketClientConnected(QWebSocket *client);
    void onWebSocketClientDisconnected(QWebSocket *client);
    void onWebSocketAlarm(const QString &alarmId, const QString &alarmType,
                          int frameId, long long timestampMs);

    // 配置热更新
    void onConfigFileChanged(const QString &path);

private:
    void log(const QString& category, const QString& message);
    QString currentTimestamp();
    void updateThresholdLabels();
    void enableControls(bool enabled);
    void setupUI();
    void setupStyle();
    QString formatSize(qint64 bytes);
    void saveSettings();
    void restoreSettings();
    void updateZoomState();
    void syncAlarmScreenshotDirToConfig(const std::string &dir);

    // --- 视频显示网格 ---
    VideoCell video_cells_[MAX_CHANNELS];
    QGridLayout* grid_layout_ = nullptr;
    int expanded_ch_ = -1;  // -1 = 2x2 网格, 0-3 = 放大的通道

    // --- 控制区 ---
    QLineEdit* model_edit_;
    QLineEdit* video_edits_[MAX_CHANNELS];
    QLineEdit* video_alias_edits_[MAX_CHANNELS];
    QPushButton* model_btn_;
    QPushButton* video_btns_[MAX_CHANNELS];
    QPushButton* video_del_btns_[MAX_CHANNELS];
    QPushButton* load_btn_;
    QPushButton* start_btn_;
    QPushButton* stop_btn_;
    QPushButton* pause_btn_;
    QPushButton* step_btn_;
    QPushButton* screenshot_btn_;
    QPushButton* export_btn_;
    QSpinBox* thread_spin_;
    QSlider* conf_slider_;
    QSlider* nms_slider_;
    QLabel* conf_value_label_;
    QLabel* nms_value_label_;
    QLabel* model_status_left_label_;  // 控制区模型状态
    QLineEdit* alarm_screenshot_edit_;
    QPushButton* alarm_screenshot_btn_;

    // --- 右侧面板 ---
    QLabel* model_status_label_;
    QLabel* model_name_label_;
    QLabel* fps_labels_[MAX_CHANNELS];
    QLabel* frames_labels_[MAX_CHANNELS];
    QLabel* infer_time_labels_[MAX_CHANNELS];
    QProgressBar* npu_usage_bar_;

    // 日志
    QTextEdit* log_edit_;
    QPushButton* clear_log_btn_;

    // 状态栏
    QLabel* status_label_;

    // WebSocket 状态
    QLabel* ws_status_label_;
    QLabel* ws_clients_label_;

    // 逻辑
    DetectThread* detect_threads_[MAX_CHANNELS] = {};
    std::shared_ptr<WebSocket> ws_server_;
    std::unique_ptr<QFileSystemWatcher> config_watcher_;
    float conf_threshold_ = 0.25f;
    float nms_threshold_ = 0.45f;
    std::string model_path_;
    QImage last_frames_[MAX_CHANNELS];

    // 检测结果历史（用于导出）
    struct ExportRecord {
        int channel;
        int frame_id;
        std::string class_name;
        float confidence;
        int left, top, right, bottom;
    };
    std::vector<ExportRecord> export_records_;
};

#endif // RK_MAINWINDOW_HPP
