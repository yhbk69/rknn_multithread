/*
 * mainwindow.cpp - RK3588 Qt GUI 主窗口实现
 */

#include "rk/mainwindow.hpp"
#include <QDateTime>
#include <QMessageBox>
#include <QGroupBox>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), detect_thread_(nullptr)
{
    setupUI();
    log("system", "Ready. Load model first.");
}

MainWindow::~MainWindow() {
    if (detect_thread_) {
        detect_thread_->stop();
        detect_thread_->wait();
        delete detect_thread_;
    }
}

void MainWindow::setupUI() {
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout* main_layout = new QVBoxLayout(central);

    // 模型选择区域
    QGroupBox* model_group = new QGroupBox("模型配置", this);
    QHBoxLayout* model_layout = new QHBoxLayout(model_group);

    model_edit_ = new QLineEdit("./model/RK3588/yolov5s-640-640.rknn", this);
    model_btn_ = new QPushButton("浏览...", this);
    load_btn_ = new QPushButton("加载模型", this);
    model_status_label_ = new QLabel("未加载", this);
    model_status_label_->setStyleSheet("color: gray;");

    model_layout->addWidget(new QLabel("模型:"));
    model_layout->addWidget(model_edit_, 1);
    model_layout->addWidget(model_btn_);
    model_layout->addWidget(load_btn_);
    model_layout->addWidget(model_status_label_);

    // 视频选择区域
    QGroupBox* video_group = new QGroupBox("视频配置", this);
    QHBoxLayout* video_layout = new QHBoxLayout(video_group);

    video_edit_ = new QLineEdit(this);
    video_edit_->setPlaceholderText("选择视频文件或摄像头ID (如 0)...");
    video_btn_ = new QPushButton("浏览...", this);

    video_layout->addWidget(new QLabel("视频:"));
    video_layout->addWidget(video_edit_, 1);
    video_layout->addWidget(video_btn_);

    // 显示区域
    display_label_ = new QLabel("请加载模型后打开视频/摄像头", this);
    display_label_->setAlignment(Qt::AlignCenter);
    display_label_->setMinimumSize(640, 480);
    display_label_->setStyleSheet("QLabel { border: 2px dashed #aaa; border-radius: 4px; background-color: #1e1e1e; color: #888; }");

    // 控制按钮区域
    QHBoxLayout* button_layout = new QHBoxLayout();

    start_btn_ = new QPushButton("开始检测", this);
    start_btn_->setEnabled(false);
    stop_btn_ = new QPushButton("停止", this);
    stop_btn_->setEnabled(false);

    thread_spin_ = new QSpinBox(this);
    thread_spin_->setRange(1, 8);
    thread_spin_->setValue(3);

    button_layout->addWidget(start_btn_);
    button_layout->addWidget(stop_btn_);
    button_layout->addStretch();
    button_layout->addWidget(new QLabel("线程数:"));
    button_layout->addWidget(thread_spin_);

    // 阈值控制区域
    QHBoxLayout* threshold_layout = new QHBoxLayout();

    conf_slider_ = new QSlider(Qt::Horizontal, this);
    conf_slider_->setRange(0, 100);
    conf_slider_->setValue(25);
    conf_value_label_ = new QLabel("0.25", this);

    nms_slider_ = new QSlider(Qt::Horizontal, this);
    nms_slider_->setRange(0, 100);
    nms_slider_->setValue(45);
    nms_value_label_ = new QLabel("0.45", this);

    threshold_layout->addWidget(new QLabel("置信度:"));
    threshold_layout->addWidget(conf_slider_, 1);
    threshold_layout->addWidget(conf_value_label_);
    threshold_layout->addStretch();
    threshold_layout->addWidget(new QLabel("NMS:"));
    threshold_layout->addWidget(nms_slider_, 1);
    threshold_layout->addWidget(nms_value_label_);

    // 日志区域
    QHBoxLayout* log_header = new QHBoxLayout();
    log_header->addWidget(new QLabel("运行日志"));
    clear_log_btn_ = new QPushButton("清空", this);
    clear_log_btn_->setMaximumWidth(80);
    log_header->addWidget(clear_log_btn_);

    log_edit_ = new QTextEdit(this);
    log_edit_->setReadOnly(true);
    log_edit_->setMaximumHeight(120);
    log_edit_->setStyleSheet("QTextEdit { font-family: monospace; font-size: 12px; background-color: #1e1e1e; color: #d4d4d4; }");

    // 组装主布局
    main_layout->addWidget(model_group);
    main_layout->addWidget(video_group);
    main_layout->addWidget(display_label_, 1);
    main_layout->addLayout(button_layout);
    main_layout->addLayout(threshold_layout);
    main_layout->addLayout(log_header);
    main_layout->addWidget(log_edit_);

    // 状态栏
    fps_label_ = new QLabel("FPS: --", this);
    time_label_ = new QLabel("Time: --", this);
    fps_label_->setStyleSheet("margin-right: 15px;");
    time_label_->setStyleSheet("margin-right: 15px;");
    statusBar()->addWidget(new QLabel("Ready"), 1);
    statusBar()->addPermanentWidget(fps_label_);
    statusBar()->addPermanentWidget(time_label_);

    // 信号槽连接
    connect(model_btn_, &QPushButton::clicked, this, &MainWindow::onBrowseModel);
    connect(load_btn_, &QPushButton::clicked, this, &MainWindow::onLoadModel);
    connect(video_btn_, &QPushButton::clicked, this, &MainWindow::onBrowseVideo);
    connect(start_btn_, &QPushButton::clicked, this, &MainWindow::onStartDetection);
    connect(stop_btn_, &QPushButton::clicked, this, &MainWindow::onStopDetection);
    connect(conf_slider_, &QSlider::valueChanged, this, &MainWindow::onConfThresholdChanged);
    connect(nms_slider_, &QSlider::valueChanged, this, &MainWindow::onNmsThresholdChanged);
    connect(clear_log_btn_, &QPushButton::clicked, this, &MainWindow::onClearLog);
}

void MainWindow::log(const QString& category, const QString& message) {
    QString ts = currentTimestamp();
    QString line = QString("[%1] [%2] %3").arg(ts).arg(category).arg(message);
    log_edit_->append(line);
}

QString MainWindow::currentTimestamp() {
    return QDateTime::currentDateTime().toString("hh:mm:ss");
}

void MainWindow::updateThresholdLabels() {
    conf_value_label_->setText(QString::number(conf_threshold_, 'f', 2));
    nms_value_label_->setText(QString::number(nms_threshold_, 'f', 2));
}

void MainWindow::enableControls(bool enabled) {
    start_btn_->setEnabled(!enabled);
    stop_btn_->setEnabled(enabled);
    model_edit_->setEnabled(!enabled);
    video_edit_->setEnabled(!enabled);
    model_btn_->setEnabled(!enabled);
    video_btn_->setEnabled(!enabled);
    load_btn_->setEnabled(!enabled);
    thread_spin_->setEnabled(!enabled);
}

void MainWindow::onBrowseModel() {
    QString p = QFileDialog::getOpenFileName(this, "Select RKNN Model", "", "RKNN (*.rknn);;All (*.*)");
    if (!p.isEmpty()) model_edit_->setText(p);
}

void MainWindow::onLoadModel() {
    model_path_ = model_edit_->text().toStdString();
    if (model_path_.empty()) {
        QMessageBox::warning(this, "Error", "Please select a model file.");
        return;
    }

    FILE* f = fopen(model_path_.c_str(), "rb");
    if (!f) {
        QMessageBox::warning(this, "Error", "Model file not found: " + QString::fromStdString(model_path_));
        return;
    }
    fclose(f);

    model_status_label_->setText("已加载");
    model_status_label_->setStyleSheet("color: green; font-weight: bold;");
    start_btn_->setEnabled(true);
    log("model", "Model loaded: " + QString::fromStdString(model_path_));
}

void MainWindow::onBrowseVideo() {
    QString p = QFileDialog::getOpenFileName(this, "Open Video", "", "Videos (*.mp4 *.avi *.mkv *.mov);;All (*.*)");
    if (!p.isEmpty()) video_edit_->setText(p);
}

void MainWindow::onStartDetection() {
    if (model_path_.empty()) {
        QMessageBox::warning(this, "Error", "Please load a model first.");
        return;
    }

    video_path_ = video_edit_->text().toStdString();
    if (video_path_.empty()) {
        QMessageBox::warning(this, "Error", "Please select a video file or enter camera ID.");
        return;
    }

    int thread_num = thread_spin_->value();

    detect_thread_ = new DetectThread(model_path_, video_path_, thread_num);
    connect(detect_thread_, &DetectThread::frameReady, this, &MainWindow::onFrameReady);
    connect(detect_thread_, &DetectThread::finished, this, &MainWindow::onDetectFinished);
    connect(detect_thread_, &DetectThread::error, this, &MainWindow::onDetectError);

    enableControls(true);
    detect_thread_->start();
    log("system", "Detection started. Threads: " + QString::number(thread_num));
}

void MainWindow::onStopDetection() {
    if (detect_thread_) {
        detect_thread_->stop();
        log("system", "Stopping detection...");
    }
}

void MainWindow::onFrameReady(const QImage& image, double fps) {
    QPixmap pix = QPixmap::fromImage(image);
    display_label_->setPixmap(pix.scaled(display_label_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    fps_label_->setText(QString("FPS: %1").arg(fps, 0, 'f', 1));
}

void MainWindow::onDetectFinished() {
    enableControls(false);
    if (detect_thread_) {
        delete detect_thread_;
        detect_thread_ = nullptr;
    }
    log("system", "Detection finished.");
}

void MainWindow::onDetectError(const QString& msg) {
    log("info", msg);
}

void MainWindow::onConfThresholdChanged(int value) {
    conf_threshold_ = value / 100.0f;
    updateThresholdLabels();
}

void MainWindow::onNmsThresholdChanged(int value) {
    nms_threshold_ = value / 100.0f;
    updateThresholdLabels();
}

void MainWindow::onClearLog() {
    log_edit_->clear();
}
