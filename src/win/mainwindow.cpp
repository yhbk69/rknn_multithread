#include "mainwindow.hpp"
#include "ui_mainwindow.h"
#include "gui_logger.hpp"
#include "detection_utils.hpp"
#include "video_source.hpp"
#include "camera_manager.hpp"
#include "model_manager.hpp"
#include "core/config.hpp"
#include "preprocessor.hpp"
#include "mjpeg_streamer.hpp"
#include <opencv2/opencv.hpp>
#include <QFileDialog>
#include <QMessageBox>
#include <QToolBar>
#include <QMenu>
#include <QCloseEvent>
#include <fstream>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), confThreshold_(0.25f), nmsThreshold_(0.45f)
{
    ui = new Ui::MainWindow;
    ui->setupUi(this);
    ui->modelPathEdit->setText("model/best.onnx");

    statusMessageLabel_ = new QLabel("Ready", this);
    fpsLabel_  = new QLabel("FPS: --", this);
    timeLabel_ = new QLabel("Time: --", this);
    fpsLabel_->setStyleSheet("margin-right: 15px;");
    timeLabel_->setStyleSheet("margin-right: 15px;");
    statusBar()->addWidget(statusMessageLabel_, 1);
    statusBar()->addPermanentWidget(fpsLabel_);
    statusBar()->addPermanentWidget(timeLabel_);

    cameraListWidget_ = new QListWidget(this);
    cameraListWidget_->setMaximumHeight(150);
    cameraListWidget_->setStyleSheet(
        "QListWidget{background:#1e1e1e;border:1px solid #444;}"
        "QListWidget::item{padding:4px 8px;}"
        "QListWidget::item:selected{background:#2a3f5f;}");
    auto* rl = qobject_cast<QVBoxLayout*>(ui->rightPanel->layout());
    if (rl) rl->insertWidget(rl->indexOf(ui->resultTitleLabel)+1, cameraListWidget_);
    connect(cameraListWidget_, &QListWidget::itemClicked, this, &MainWindow::onCameraListClicked);

    // File menu
    QMenu* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(ui->actionOpenImage);
    fileMenu->addAction(ui->actionOpenVideo);
    fileMenu->addAction(ui->actionOpenCamera);
    fileMenu->addSeparator();
    fileMenu->addAction(ui->actionLoadModel);
    fileMenu->addSeparator();
    fileMenu->addAction(ui->actionExit);

    // Toolbar
    QToolBar* tb = addToolBar("Tools");
    tb->addAction(ui->actionOpenImage);
    tb->addAction(ui->actionOpenVideo);
    tb->addAction(ui->actionOpenCamera);

    auto* bl = qobject_cast<QHBoxLayout*>(ui->buttonLayout);
    if (bl) {
        startDetectBtn_ = new QPushButton("Start Detection", this);
        startDetectBtn_->setStyleSheet(
            "QPushButton{background:#5cb85c;color:white;font-weight:bold;padding:6px 16px;}"
            "QPushButton:disabled{background:#555;color:#888;}");
        startDetectBtn_->setEnabled(false);
        bl->addWidget(startDetectBtn_);
        connect(startDetectBtn_, &QPushButton::clicked, this, &MainWindow::onStartDetection);
    }

    modelManager_ = std::make_unique<ModelManager>();
    cameraManager_ = std::make_unique<CameraManager>();

    // 加载标签文件
    loadLabels("model/ppe_11_labels.txt");

    // MJPEG streamer
    mjpegStreamer_ = std::make_unique<MjpegStreamer>();
    mjpegStreamer_->start(9093, "127.0.0.1");
    log("system", "MJPEG: http://localhost:9093/stream");

    connect(ui->browseModelBtn, &QPushButton::clicked, this, &MainWindow::onBrowseModel);
    connect(ui->loadModelBtn, &QPushButton::clicked, this, &MainWindow::onLoadModel);
    connect(ui->reloadModelBtn, &QPushButton::clicked, this, &MainWindow::onReloadModel);
    connect(ui->stopBtn, &QPushButton::clicked, this, &MainWindow::onStopProcessing);
    connect(ui->confSlider, &QSlider::valueChanged, this, &MainWindow::onConfThresholdChanged);
    connect(ui->nmsSlider, &QSlider::valueChanged, this, &MainWindow::onNmsThresholdChanged);
    connect(ui->actionOpenImage, &QAction::triggered, this, &MainWindow::onOpenImage);
    connect(ui->actionOpenVideo, &QAction::triggered, this, &MainWindow::onOpenVideo);
    connect(ui->actionOpenCamera, &QAction::triggered, this, [this](bool){ onOpenCamera(true); });
    connect(ui->actionExit, &QAction::triggered, this, &QWidget::close);
    connect(ui->actionLoadModel, &QAction::triggered, this, &MainWindow::onLoadModel);

    updateThresholdLabels();
    log("system", "Ready. Load model first.");
}

MainWindow::~MainWindow() { if(mjpegStreamer_) mjpegStreamer_->stop(); stopAllCameras(); delete ui; }
void MainWindow::log(const QString& c, const QString& m) { GuiLogger::log(ui->logTextEdit, c, m); }
void MainWindow::updateThresholdLabels() {
    ui->confValueLabel->setText(QString::number(confThreshold_,'f',2));
    ui->nmsValueLabel->setText(QString::number(nmsThreshold_,'f',2));
}
void MainWindow::onBrowseModel() {
    QString p = QFileDialog::getOpenFileName(this, "Select ONNX", "", "ONNX (*.onnx);;All (*.*)");
    if (!p.isEmpty()) ui->modelPathEdit->setText(p);
}
void MainWindow::onLoadModel() {
    try {
        bool ok = modelManager_->load(ui->modelPathEdit->text().toStdString());
        if (ok) {
            if (startDetectBtn_) startDetectBtn_->setEnabled(true);
            log("model", "Loaded: " + ui->modelPathEdit->text());
            statusMessageLabel_->setText("Model loaded");
        } else {
            log("error", "Failed to load: " + ui->modelPathEdit->text());
            QMessageBox::warning(this, "Error", "Failed to load model.");
        }
    } catch (const std::exception& e) { log("error", QString("Error: ")+e.what()); }
}
void MainWindow::onReloadModel() { if(modelManager_) modelManager_->reload(ui->modelPathEdit->text().toStdString()); }
void MainWindow::onOpenCamera(bool) {
    stopAllCameras();
    if (!modelManager_ || !modelManager_->currentEngine()) { log("error", "Load model first!"); return; }
    startCameraWorker(0, "Camera 0", "0");
}
void MainWindow::onOpenVideo() {
    stopAllCameras();
    if (!modelManager_ || !modelManager_->currentEngine()) { log("error", "Load model first!"); return; }
    QString p = QFileDialog::getOpenFileName(this, "Open Video", "", "Videos (*.mp4 *.avi);;All (*.*)");
    if (p.isEmpty()) return;
    startVideoWorker(p);
}
void MainWindow::onOpenImage() {
    if (!modelManager_ || !modelManager_->currentEngine()) { log("error", "Load model first!"); return; }
    QString p = QFileDialog::getOpenFileName(this, "Open Image", "", "Images (*.jpg *.png)");
    if (p.isEmpty()) return;
    processSingleImage(p.toStdString());
}
void MainWindow::onStopProcessing() { stopAllCameras(); }
void MainWindow::onConfThresholdChanged(int v) { confThreshold_=v/100.0f; updateThresholdLabels(); }
void MainWindow::onNmsThresholdChanged(int v) { nmsThreshold_=v/100.0f; updateThresholdLabels(); }

void MainWindow::onFrameProcessed(int cid, QImage img, std::vector<Detection> dets, double ms) {
    if (!ui->displayLabel) return;
    QPixmap pix = QPixmap::fromImage(img);
    ui->displayLabel->setPixmap(pix.scaled(ui->displayLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    fpsLabel_->setText(QString("FPS: %1").arg(1000.0/std::max(ms,1.0),0,'f',1));
    timeLabel_->setText(QString("Time: %1 ms").arg(ms,0,'f',1));
    if (activeDisplayCamera_ != cid) activeDisplayCamera_ = cid;
    // Push to MJPEG
    if (mjpegStreamer_) mjpegStreamer_->pushImage(img, 50);
}
void MainWindow::onWorkerFinished(int cid) { log("system", QString("Camera %1 stopped").arg(cid)); }
void MainWindow::onWorkerError(int cid, const QString& m) { log("error", QString("Camera %1: ").arg(cid)+m); }

void MainWindow::startCameraWorker(int cid, const QString& name, const QString& src) {
    IEngine* eng = modelManager_->currentEngine();
    auto* thread = new QThread(this);
    auto* worker = new InferenceWorker(eng, cid, name);
    cameraManager_->add(cid, thread, worker);
    connect(worker, &InferenceWorker::frameProcessed, this, &MainWindow::onFrameProcessed);
    connect(worker, &InferenceWorker::finished, this, &MainWindow::onWorkerFinished);
    connect(worker, &InferenceWorker::errorOccurred, this, &MainWindow::onWorkerError);
    worker->moveToThread(thread);
    float cf=confThreshold_, nm=nmsThreshold_;
    int iw=Config::INPUT_WIDTH, ih=Config::INPUT_HEIGHT;
    connect(thread, &QThread::started, this, [worker,cid,src,cf,nm,iw,ih](){
        auto source = std::make_unique<CameraVideoSource>(cid, src);
        worker->process(std::move(source), cf, nm, iw, ih);
    });
    thread->start();
    cameraSources_[cid]=src; cameraAliases_[cid]=name;
    activeDisplayCamera_=cid; refreshCameraList();
    log("system", QString("Camera %1 started").arg(cid));
}

void MainWindow::startVideoWorker(const QString& fp) {
    IEngine* eng = modelManager_->currentEngine();
    int vid = 999;
    auto* thread = new QThread(this);
    auto* worker = new InferenceWorker(eng, vid, "video");
    cameraManager_->add(vid, thread, worker);
    connect(worker, &InferenceWorker::frameProcessed, this, &MainWindow::onFrameProcessed);
    connect(worker, &InferenceWorker::finished, this, &MainWindow::onWorkerFinished);
    connect(worker, &InferenceWorker::errorOccurred, this, &MainWindow::onWorkerError);
    worker->moveToThread(thread);
    float cf=confThreshold_, nm=nmsThreshold_;
    int iw=Config::INPUT_WIDTH, ih=Config::INPUT_HEIGHT;
    connect(thread, &QThread::started, this, [worker,fp,cf,nm,iw,ih](){
        auto source = std::make_unique<FileVideoSource>(fp);
        worker->process(std::move(source), cf, nm, iw, ih);
    });
    thread->start();
    activeDisplayCamera_ = vid;
    log("system", "Video started: " + fp);
}

void MainWindow::stopCamera(int cid) { cameraManager_->stop(cid); cameraSources_.erase(cid); cameraAliases_.erase(cid); }
void MainWindow::stopAllCameras() { if(cameraManager_) cameraManager_->stopAll(); }
void MainWindow::refreshCameraList() {
    cameraListWidget_->clear();
    for (const auto& [id, alias] : cameraAliases_) {
        auto* it = new QListWidgetItem(QString("[%1] %2").arg(id).arg(alias));
        it->setData(Qt::UserRole, id);
        cameraListWidget_->addItem(it);
    }
}
void MainWindow::onCameraListClicked(QListWidgetItem* it) { activeDisplayCamera_ = it->data(Qt::UserRole).toInt(); }
void MainWindow::closeEvent(QCloseEvent* e) { if(mjpegStreamer_) mjpegStreamer_->stop(); stopAllCameras(); e->accept(); }

void MainWindow::processSingleImage(const std::string& path) {
    cv::Mat img = cv::imread(path);
    if (img.empty()) { log("error", "Cannot read: " + QString::fromStdString(path)); return; }
    auto* eng = modelManager_->currentEngine();
    cv::Mat proc = Preprocessor::letterbox(img, Config::INPUT_WIDTH, Config::INPUT_HEIGHT);
    auto tensor = Preprocessor::imageToTensor(proc);
    std::vector<Detection> dets;
    eng->infer(tensor, dets, img.cols, img.rows, confThreshold_, nmsThreshold_);
    for (const auto& d : dets) {
        cv::rectangle(img, cv::Point((int)d.x,(int)d.y), cv::Point((int)(d.x+d.w),(int)(d.y+d.h)), cv::Scalar(0,255,0), 2);
    }
    cv::cvtColor(img, img, cv::COLOR_BGR2RGB);
    QImage qimg(img.data, img.cols, img.rows, img.step, QImage::Format_RGB888);
    ui->displayLabel->setPixmap(QPixmap::fromImage(qimg).scaled(ui->displayLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    log("detect", QString("Image: %1 objects").arg(dets.size()));
}

// Stubs
void MainWindow::loadLabels(const std::string& path) {
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) classNames_.push_back(line);
    }
    log("system", QString("Loaded %1 labels from %2").arg(classNames_.size()).arg(QString::fromStdString(path)));
}
void MainWindow::onAddCamera() {} void MainWindow::onRemoveCamera(int) {} void MainWindow::onOpenFolder() {}
void MainWindow::onSettings() {} void MainWindow::onBatchInferenceToggled(bool) {} void MainWindow::onStartDetection() {}
void MainWindow::onAlertSaved(int,const QString&,const QString&,const QString&) {}
void MainWindow::onStartRecording() {} void MainWindow::onStopRecording() {}
void MainWindow::onViewRecordings() {} void MainWindow::onClearOldRecordings() {}
void MainWindow::loadRuntimeConfig() {} void MainWindow::saveRuntimeConfig() {}
void MainWindow::autoStartCameras() {} void MainWindow::savePerCameraThresholds() {}
void MainWindow::setupConnections() {} void MainWindow::updateModelButtons(bool) {} void MainWindow::enableControls(bool) {}
bool MainWindow::isDetecting() const { return false; }
void MainWindow::updateDisplay(const QImage&) {} void MainWindow::updateDetectionList(const std::vector<Detection>&, double) {}
QString MainWindow::currentTimestamp() { return QDateTime::currentDateTime().toString("hh:mm:ss"); }
