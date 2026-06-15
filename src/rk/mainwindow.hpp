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
        initLabelPath(model_path.c_str());
    }

    void stop() { running_.store(false); }
    void pause() { paused_.store(true); }
    void resume() { paused_.store(false); }
    void stepFrame() { step_once_.store(true); paused_.store(false); }
    bool isPaused() const { return paused_.load(); }
    void set_thread_num(int n) { thread_num_ = n; }

signals:
    void frameReady(const QImage& image, double fps);
    void statsUpdated(int framesProcessed, double avgFps, double inferenceTime);
    void finished();
    void error(const QString& msg);

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

            if (pool->put(img) != 0) break;
            if (frames >= thread_num_ && pool->get(img) != 0) break;

            long long infer_end = get_time_ms();
            double infer_time = (double)(infer_end - infer_start);

            if (frames % 30 == 0 && frames > 0) {
                long long now = get_time_ms();
                current_fps = 30.0 / float(now - before_time) * 1000.0;
                before_time = now;
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
    QSpinBox* thread_spin_;
    QSlider* conf_slider_;
    QSlider* nms_slider_;
    QLabel* conf_value_label_;
    QLabel* nms_value_label_;

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

    // 逻辑
    DetectThread* detect_thread_ = nullptr;
    float conf_threshold_ = 0.25f;
    float nms_threshold_ = 0.45f;
    std::string model_path_;
    std::string video_path_;
    QImage last_frame_;
    bool recording_ = false;
    std::unique_ptr<cv::VideoWriter> video_writer_;
};

#endif // RK_MAINWINDOW_HPP
