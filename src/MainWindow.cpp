/*
 * MainWindow.cpp - Qt GUI 主窗口实现
 */

#include "MainWindow.hpp"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), detect_thread_(nullptr)
{
    setWindowTitle("RKNN YOLOv5 Detection");
    resize(800, 600);

    // 创建中心组件和布局
    central_widget_ = new QWidget(this);
    setCentralWidget(central_widget_);
    main_layout_ = new QVBoxLayout(central_widget_);

    // 输入区域布局
    input_layout_ = new QHBoxLayout();

    model_btn_ = new QPushButton("Select Model", this);
    model_edit_ = new QLineEdit(this);
    model_edit_->setPlaceholderText("Model path (.rknn)");
    model_edit_->setReadOnly(true);

    video_btn_ = new QPushButton("Select Video", this);
    video_edit_ = new QLineEdit(this);
    video_edit_->setPlaceholderText("Video path or camera ID");
    video_edit_->setReadOnly(true);

    input_layout_->addWidget(new QLabel("Model:"));
    input_layout_->addWidget(model_edit_, 1);
    input_layout_->addWidget(model_btn_);
    input_layout_->addWidget(new QLabel("Video:"));
    input_layout_->addWidget(video_edit_, 1);
    input_layout_->addWidget(video_btn_);

    // 按钮布局
    button_layout_ = new QHBoxLayout();
    start_btn_ = new QPushButton("Start", this);
    stop_btn_ = new QPushButton("Stop", this);
    stop_btn_->setEnabled(false);
    button_layout_->addStretch();
    button_layout_->addWidget(start_btn_);
    button_layout_->addWidget(stop_btn_);

    // 显示区域
    display_label_ = new QLabel(this);
    display_label_->setAlignment(Qt::AlignCenter);
    display_label_->setStyleSheet("background-color: black;");
    display_label_->setMinimumSize(640, 480);
    display_label_->setText("No video");

    // 组装布局
    main_layout_->addLayout(input_layout_);
    main_layout_->addLayout(button_layout_);
    main_layout_->addWidget(display_label_, 1);

    // 连接信号槽
    connect(model_btn_, &QPushButton::clicked, this, &MainWindow::selectModel);
    connect(video_btn_, &QPushButton::clicked, this, &MainWindow::selectVideo);
    connect(start_btn_, &QPushButton::clicked, this, &MainWindow::startDetection);
    connect(stop_btn_, &QPushButton::clicked, this, &MainWindow::stopDetection);
}

MainWindow::~MainWindow()
{
    stopDetection();
}

void MainWindow::selectModel()
{
    QString file_path = QFileDialog::getOpenFileName(this, "Select Model", "", "RKNN Model (*.rknn)");
    if (!file_path.isEmpty())
    {
        model_edit_->setText(file_path);
    }
}

void MainWindow::selectVideo()
{
    QString file_path = QFileDialog::getOpenFileName(this, "Select Video", "", "Video Files (*.mp4 *.avi *.mkv *.mov)");
    if (!file_path.isEmpty())
    {
        video_edit_->setText(file_path);
    }
}

void MainWindow::startDetection()
{
    QString model_path = model_edit_->text();
    QString video_path = video_edit_->text();

    if (model_path.isEmpty())
    {
        QMessageBox::warning(this, "Warning", "Please select a model file!");
        return;
    }

    if (video_path.isEmpty())
    {
        QMessageBox::warning(this, "Warning", "Please select a video file!");
        return;
    }

    // 创建并启动检测线程
    detect_thread_ = new DetectThread(model_path.toStdString(), video_path.toStdString());
    connect(detect_thread_, &DetectThread::frameReady, this, &MainWindow::onFrameReady);
    connect(detect_thread_, &DetectThread::finished, this, &MainWindow::onDetectFinished);
    connect(detect_thread_, &DetectThread::error, this, &MainWindow::onDetectError);

    detect_thread_->start();

    start_btn_->setEnabled(false);
    stop_btn_->setEnabled(true);
}

void MainWindow::stopDetection()
{
    if (detect_thread_ && detect_thread_->isRunning())
    {
        detect_thread_->stop();
        detect_thread_->wait();
        delete detect_thread_;
        detect_thread_ = nullptr;
    }

    start_btn_->setEnabled(true);
    stop_btn_->setEnabled(false);
}

void MainWindow::onFrameReady(const QImage &image, double fps)
{
    // 缩放图像以适应显示区域
    QPixmap pixmap = QPixmap::fromImage(image);
    QPixmap scaled = pixmap.scaled(display_label_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    display_label_->setPixmap(scaled);
}

void MainWindow::onDetectFinished()
{
    stopDetection();
}

void MainWindow::onDetectError(const QString &msg)
{
    QMessageBox::information(this, "Info", msg);
}
