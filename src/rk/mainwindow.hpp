/*
 * mainwindow.hpp - RK3588 Qt GUI 主窗口
 * 16:9 自适应布局，左右分栏
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
#include <sys/time.h>

#include <opencv2/opencv.hpp>

#include "rkYolov5s.hpp"
#include "rknnPool.hpp"
#include "config_loader.hpp"
#include "postprocess.h"
#include "websocket.hpp"

// ============================================================
// 检测工作线程
// ============================================================
class DetectThread : public QThread {
    Q_OBJECT
public:
    DetectThread(const std::string& model_path, const std::string& video_path,
                 int thread_num = 3, float conf_threshold = 0.25f, float nms_threshold = 0.45f)
        : model_path_(model_path), video_path_(video_path), thread_num_(thread_num),
          conf_threshold_(conf_threshold), nms_threshold_(nms_threshold), running_(true) {
        // 后处理上下文由每个 rkYolov5s 实例自行初始化
    }

    void stop() { running_.store(false); }
    void pause() { paused_.store(true); }
    void resume() { paused_.store(false); }
    void stepFrame() { step_once_.store(true); paused_.store(false); }
    bool isPaused() const { return paused_.load(); }
    void set_thread_num(int n) { thread_num_ = n; }
    void setWebSocket(WebSocket* ws) { ws_ = ws; }
    void setRoi(const QRect &roi) { roi_rect_ = roi; roi_enabled_ = !roi.isNull(); }
    void clearRoi() { roi_rect_ = QRect(); roi_enabled_ = false; }

signals:
    void frameReady(const QImage& image, double fps);
    void statsUpdated(int framesProcessed, double avgFps, double inferenceTime);
    void finished();
    void error(const QString& msg);
    void detectionResult(int frameId, const QString& className, float confidence,
                         int left, int top, int right, int bottom);

protected:
    void run() override {
        auto pool = std::make_unique<rknnPool<rkYolov5s, cv::Mat, cv::Mat>>(model_path_.c_str(), thread_num_);
        if (pool->init() != 0) {
            emit error("rknnPool init failed!");
            emit finished();
            return;
        }
        pool->set_thresholds(conf_threshold_, nms_threshold_);
        printf("[DetectThread] Thresholds applied: conf=%.2f, nms=%.2f\n", conf_threshold_, nms_threshold_);

        cv::VideoCapture capture;
        if (video_path_.length() == 1)
            capture.open((int)(video_path_[0] - '0'));
        else
            capture.open(video_path_);

        if (!capture.isOpened()) {
            emit error("Cannot open video/camera: " + QString::fromStdString(video_path_));
            emit finished();
            return;
        }

        auto get_time_ms = []() -> long long {
            struct timeval tv;
            gettimeofday(&tv, nullptr);
            return tv.tv_sec * 1000 + tv.tv_usec / 1000;
        };

        long long start_time = get_time_ms();
        int frames = 0;
        long long before_time = start_time;
        double current_fps = 0.0;

        while (running_.load()) {
            // 暂停/单帧步进检查
            while (paused_.load() && running_.load()) {
                if (step_once_.load()) {
                    step_once_.store(false);
                    break;
                }
                QThread::msleep(50);
            }
            if (!running_.load()) break;

            cv::Mat img;
            if (!capture.read(img)) break;

            long long infer_start = get_time_ms();

            // ROI 裁剪：如果有 ROI 区域，只对裁剪区域做检测
            cv::Mat detect_img;
            int roi_x = 0, roi_y = 0;
            if (roi_enabled_ && !roi_rect_.isNull()) {
                int x1 = qBound(0, roi_rect_.x(), img.cols - 1);
                int y1 = qBound(0, roi_rect_.y(), img.rows - 1);
                int x2 = qBound(x1 + 1, roi_rect_.x() + roi_rect_.width(), img.cols);
                int y2 = qBound(y1 + 1, roi_rect_.y() + roi_rect_.height(), img.rows);
                roi_x = x1;
                roi_y = y1;
                detect_img = img(cv::Range(y1, y2), cv::Range(x1, x2)).clone();
            } else {
                detect_img = img;
            }

            if (pool->put(detect_img) != 0) break;
            if (frames >= thread_num_ && pool->get(detect_img) != 0) break;

            // 如果有 ROI，将检测结果坐标偏移回原图，并绘制到原图上
            if (roi_enabled_ && !roi_rect_.isNull() && frames >= thread_num_) {
                detect_result_group_t &result = const_cast<detect_result_group_t&>(pool->getLastDetectResult());
                for (int i = 0; i < result.count; i++) {
                    result.results[i].box.left += roi_x;
                    result.results[i].box.right += roi_x;
                    result.results[i].box.top += roi_y;
                    result.results[i].box.bottom += roi_y;
                    // 在原图上绘制检测框
                    char text[64];
                    snprintf(text, sizeof(text), "%s %.0f%%", result.results[i].name, result.results[i].prop * 100);
                    cv::rectangle(img,
                        cv::Point(result.results[i].box.left, result.results[i].box.top),
                        cv::Point(result.results[i].box.right, result.results[i].box.bottom),
                        cv::Scalar(255, 0, 0), 2);
                    cv::putText(img, text,
                        cv::Point(result.results[i].box.left, result.results[i].box.top - 5),
                        cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1);
                }
                img.copyTo(detect_img);
            }

            // WebSocket 报警检查
            if (ws_ && ws_->isAlarmEnabled() && frames >= thread_num_) {
                ws_->checkAndAlarm(&pool->getLastDetectResult(), frames);
            }

            // 收集检测结果用于导出
            if (frames >= thread_num_) {
                const detect_result_group_t &result = pool->getLastDetectResult();
                for (int i = 0; i < result.count; i++) {
                    const detect_result_t &det = result.results[i];
                    emit detectionResult(frames, QString::fromUtf8(det.name), det.prop,
                                         det.box.left, det.box.top, det.box.right, det.box.bottom);
                }
            }

            long long infer_end = get_time_ms();
            double infer_time = (double)(infer_end - infer_start);

            // 每 30 帧更新 FPS 和推理耗时
            if (frames % 30 == 0 && frames > 0) {
                long long now = get_time_ms();
                current_fps = 30.0 / float(now - before_time) * 1000.0;
                before_time = now;
            }
            // 每帧更新 NPU 使用率（轻量读取 sysfs）
            if (frames % 5 == 0) {
                emit statsUpdated(frames, current_fps, infer_time);
            }

            char fps_text[32];
            snprintf(fps_text, sizeof(fps_text), "FPS: %.2f", current_fps);
            cv::putText(img, fps_text, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);

            cv::Mat rgb_img;
            cv::cvtColor(img, rgb_img, cv::COLOR_BGR2RGB);
            QImage qimg(rgb_img.data, rgb_img.cols, rgb_img.rows, rgb_img.step, QImage::Format_RGB888);
            emit frameReady(qimg.copy(), current_fps);

            frames++;
            QThread::msleep(1);
        }

        // drain remaining
        while (running_.load()) {
            cv::Mat img;
            if (pool->get(img) != 0) break;

            cv::Mat rgb_img;
            cv::cvtColor(img, rgb_img, cv::COLOR_BGR2RGB);
            QImage qimg(rgb_img.data, rgb_img.cols, rgb_img.rows, rgb_img.step, QImage::Format_RGB888);
            emit frameReady(qimg.copy(), current_fps);
        }

        long long end_time = get_time_ms();
        double avg_fps = (end_time > start_time) ? float(frames) / float(end_time - start_time) * 1000.0 : 0.0;
        emit statsUpdated(frames, avg_fps, 0);
        emit error(QString("Detection finished. Frames: %1, Avg FPS: %2").arg(frames).arg(avg_fps, 0, 'f', 2));
        emit finished();
    }

private:
    std::string model_path_;
    std::string video_path_;
    int thread_num_;
    float conf_threshold_;
    float nms_threshold_;
    std::atomic<bool> running_;
    std::atomic<bool> paused_{false};
    std::atomic<bool> step_once_{false};
    WebSocket* ws_ = nullptr;
    QRect roi_rect_;
    bool roi_enabled_ = false;
};

// ============================================================
// MainWindow: RK3588 主窗口 (16:9 自适应布局)
// ============================================================
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onBrowseModel();
    void onLoadModel();
    void onBrowseVideo();
    void onStartDetection();
    void onStopDetection();
    void onFrameReady(const QImage& image, double fps);
    void onStatsUpdated(int frames, double fps, double inferTime);
    void onDetectFinished();
    void onDetectError(const QString& msg);
    void onConfThresholdChanged(int value);
    void onNmsThresholdChanged(int value);
    void onClearLog();
    void onCameraSelected(int index);
    void onRefreshCameras();
    void onPauseToggle();
    void onStepFrame();
    void onScreenshot();
    void onRecordToggle();
    void onExportResults();
    void onRoiToggle();
    void onRoiClear();

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
    void detectCameras();
    QString formatSize(qint64 bytes);
    void saveSettings();
    void restoreSettings();

    // --- 左侧面板 ---
    // 检测显示
    QGraphicsView* display_view_;
    QGraphicsScene* display_scene_;
    QGraphicsPixmapItem* pixmap_item_;
    QLabel* display_overlay_;       // 叠加 FPS/状态信息

    // 控制区
    QLineEdit* model_edit_;
    QLineEdit* video_edit_;
    QPushButton* model_btn_;
    QPushButton* video_btn_;
    QPushButton* load_btn_;
    QPushButton* start_btn_;
    QPushButton* stop_btn_;
    QPushButton* pause_btn_;
    QPushButton* step_btn_;
    QPushButton* screenshot_btn_;
    QPushButton* record_btn_;
    QPushButton* export_btn_;
    QSpinBox* thread_spin_;
    QSlider* conf_slider_;
    QSlider* nms_slider_;
    QLabel* conf_value_label_;
    QLabel* nms_value_label_;
    QPushButton* roi_btn_;
    QPushButton* roi_clear_btn_;

    // --- 右侧面板 ---
    // 状态信息
    QLabel* model_status_label_;
    QLabel* model_status_left_label_;
    QLabel* model_name_label_;
    QLabel* fps_label_;
    QLabel* resolution_label_;
    QLabel* frames_label_;
    QLabel* infer_time_label_;
    QProgressBar* npu_usage_bar_;

    // 摄像头列表
    QListWidget* camera_list_;
    QPushButton* refresh_cam_btn_;
    QComboBox* camera_combo_;

    // 日志
    QTextEdit* log_edit_;
    QPushButton* clear_log_btn_;

    // 状态栏
    QLabel* status_label_;

    // WebSocket 状态
    QLabel* ws_status_label_;      // 显示 WebSocket 地址:端口
    QLabel* ws_clients_label_;     // 显示连接的客户端数

    // 逻辑
    DetectThread* detect_thread_ = nullptr;
    std::unique_ptr<WebSocket> ws_server_;
    std::unique_ptr<QFileSystemWatcher> config_watcher_;
    float conf_threshold_ = 0.25f;
    float nms_threshold_ = 0.45f;
    std::string model_path_;
    std::string video_path_;
    QImage last_frame_;
    bool recording_ = false;
    std::unique_ptr<cv::VideoWriter> video_writer_;

    // 检测结果历史（用于导出）
    struct ExportRecord {
        int frame_id;
        std::string class_name;
        float confidence;
        int left, top, right, bottom;
    };
    std::vector<ExportRecord> export_records_;

    // ROI 检测区域
    bool roi_enabled_ = false;
    bool roi_selecting_ = false;
    QPoint roi_start_;
    QRect roi_rect_;
    QLabel* roi_status_label_;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
};

#endif // RK_MAINWINDOW_HPP
