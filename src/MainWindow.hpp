/*
 * MainWindow.hpp - Qt GUI 主窗口
 * 提供模型和视频选择，实时显示检测画面
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QThread>
#include <QImage>
#include <QTimer>

#include <sys/time.h>

#include <memory>
#include <atomic>
#include <mutex>

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "rkYolov5s.hpp"
#include "rknnPool.hpp"
#include "config_loader.hpp"
#include "postprocess.h"

/*
 * 检测工作线程：在后台线程中运行 RKNN 推理
 * 通过 Qt 信号槽将处理后的帧发送到主线程显示
 */
class DetectThread : public QThread
{
    Q_OBJECT
public:
    DetectThread(const std::string &model_path, const std::string &video_path, int thread_num = 3)
        : model_path_(model_path), video_path_(video_path), thread_num_(thread_num), running_(true)
    {
        // 初始化标签路径
        initLabelPath(model_path.c_str());
    }

    void stop()
    {
        running_.store(false);
    }

    void set_thread_num(int n) { thread_num_ = n; }

signals:
    void frameReady(const QImage &image, double fps);
    void finished();
    void error(const QString &msg);

protected:
    void run() override
    {
        // 初始化线程池
        auto pool = std::make_unique<rknnPool<rkYolov5s, cv::Mat, cv::Mat>>(model_path_.c_str(), thread_num_);
        if (pool->init() != 0)
        {
            emit error("rknnPool init fail!");
            emit finished();
            return;
        }

        cv::VideoCapture capture;
        if (video_path_.length() == 1)
            capture.open((int)(video_path_[0] - '0'));
        else
            capture.open(video_path_);

        if (!capture.isOpened())
        {
            emit error("Cannot open video!");
            emit finished();
            return;
        }

        struct timeval time;
        gettimeofday(&time, nullptr);
        auto start_time = time.tv_sec * 1000 + time.tv_usec / 1000;

        int frames = 0;
        auto before_time = start_time;
        double current_fps = 0.0;

        while (running_.load())
        {
            cv::Mat img;
            if (!capture.read(img))
                break;

            if (pool->put(img) != 0)
                break;

            if (frames >= thread_num_ && pool->get(img) != 0)
                break;

            if (frames % 30 == 0 && frames > 0)
            {
                gettimeofday(&time, nullptr);
                auto current_time = time.tv_sec * 1000 + time.tv_usec / 1000;
                current_fps = 30.0 / float(current_time - before_time) * 1000.0;
                before_time = current_time;
            }

            // 绘制 FPS 文字
            char fps_text[32];
            snprintf(fps_text, sizeof(fps_text), "FPS: %.2f", current_fps);
            cv::putText(img, fps_text, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);

            // 转换 OpenCV Mat 到 QImage
            cv::Mat rgb_img;
            cv::cvtColor(img, rgb_img, cv::COLOR_BGR2RGB);
            QImage qimg(rgb_img.data, rgb_img.cols, rgb_img.rows, rgb_img.step, QImage::Format_RGB888);
            emit frameReady(qimg.copy(), current_fps);

            frames++;

            // 控制帧率，避免过快
            QThread::msleep(1);
        }

        // 清空剩余结果
        while (running_.load())
        {
            cv::Mat img;
            if (pool->get(img) != 0)
                break;

            cv::Mat rgb_img;
            cv::cvtColor(img, rgb_img, cv::COLOR_BGR2RGB);
            QImage qimg(rgb_img.data, rgb_img.cols, rgb_img.rows, rgb_img.step, QImage::Format_RGB888);
            emit frameReady(qimg.copy(), current_fps);
        }

        gettimeofday(&time, nullptr);
        auto end_time = time.tv_sec * 1000 + time.tv_usec / 1000;
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

/*
 * MainWindow - Qt 主窗口
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void selectModel();
    void selectVideo();
    void startDetection();
    void stopDetection();
    void onFrameReady(const QImage &image, double fps);
    void onDetectFinished();
    void onDetectError(const QString &msg);

private:
    // UI 组件
    QWidget *central_widget_;
    QVBoxLayout *main_layout_;
    QHBoxLayout *input_layout_;
    QHBoxLayout *button_layout_;

    QLineEdit *model_edit_;
    QLineEdit *video_edit_;
    QPushButton *model_btn_;
    QPushButton *video_btn_;
    QPushButton *start_btn_;
    QPushButton *stop_btn_;
    QLabel *display_label_;

    // 检测线程
    DetectThread *detect_thread_;
};

#endif // MAINWINDOW_H
