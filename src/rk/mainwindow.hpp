/*
 * mainwindow.hpp - RK3588 Qt GUI 主窗口
 * 仿照 Windows 版本，适配 RKNN 推理引擎
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
#include <atomic>
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
// 检测工作线程：在后台线程中运行 RKNN 推理
// ============================================================
class DetectThread : public QThread {
    Q_OBJECT
public:
    DetectThread(const std::string& model_path, const std::string& video_path, int thread_num = 3)
        : model_path_(model_path), video_path_(video_path), thread_num_(thread_num), running_(true) {
        initLabelPath(model_path.c_str());
    }

    void stop() { running_.store(false); }
    void set_thread_num(int n) { thread_num_ = n; }

signals:
    void frameReady(const QImage& image, double fps);
    void finished();
    void error(const QString& msg);

protected:
    void run() override {
        auto pool = std::make_unique<rknnPool<rkYolov5s, cv::Mat, cv::Mat>>(model_path_.c_str(), thread_num_);
        if (pool->init() != 0) {
            emit error("rknnPool init fail!");
            emit finished();
            return;
        }

        cv::VideoCapture capture;
        if (video_path_.length() == 1)
            capture.open((int)(video_path_[0] - '0'));
        else
            capture.open(video_path_);

        if (!capture.isOpened()) {
            emit error("Cannot open video!");
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
            cv::Mat img;
            if (!capture.read(img)) break;

            if (pool->put(img) != 0) break;

            if (frames >= thread_num_ && pool->get(img) != 0) break;

            if (frames % 30 == 0 && frames > 0) {
                long long now = get_time_ms();
                current_fps = 30.0 / float(now - before_time) * 1000.0;
                before_time = now;
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

        // 清空剩余结果
        while (running_.load()) {
            cv::Mat img;
            if (pool->get(img) != 0) break;

            cv::Mat rgb_img;
            cv::cvtColor(img, rgb_img, cv::COLOR_BGR2RGB);
            QImage qimg(rgb_img.data, rgb_img.cols, rgb_img.rows, rgb_img.step, QImage::Format_RGB888);
            emit frameReady(qimg.copy(), current_fps);
        }

        long long end_time = get_time_ms();
        double avg_fps = float(frames) / float(end_time - start_time) * 1000.0;
        emit error(QString("Detection finished. Average FPS: %1").arg(avg_fps, 0, 'f', 2));
        emit finished();
    }

private:
    std::string model_path_;
    std::string video_path_;
    int thread_num_;
    std::atomic<bool> running_;
};

// ============================================================
// MainWindow: RK3588 主窗口
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
    void onDetectFinished();
    void onDetectError(const QString& msg);
    void onConfThresholdChanged(int value);
    void onNmsThresholdChanged(int value);
    void onClearLog();

private:
    void log(const QString& category, const QString& message);
    QString currentTimestamp();
    void updateThresholdLabels();
    void enableControls(bool enabled);
    void setupUI();

    // UI 组件
    QLineEdit* model_edit_;
    QLineEdit* video_edit_;
    QPushButton* model_btn_;
    QPushButton* video_btn_;
    QPushButton* load_btn_;
    QPushButton* start_btn_;
    QPushButton* stop_btn_;
    QLabel* display_label_;
    QLabel* model_status_label_;
    QLabel* fps_label_;
    QLabel* time_label_;
    QSlider* conf_slider_;
    QSlider* nms_slider_;
    QLabel* conf_value_label_;
    QLabel* nms_value_label_;
    QSpinBox* thread_spin_;
    QTextEdit* log_edit_;
    QPushButton* clear_log_btn_;

    DetectThread* detect_thread_ = nullptr;
    float conf_threshold_ = 0.25f;
    float nms_threshold_ = 0.45f;
    std::string model_path_;
    std::string video_path_;
};

#endif // RK_MAINWINDOW_HPP
