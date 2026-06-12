#include "mainwindow.hpp"
#include "ui_mainwindow.h"
#include "gui_logger.hpp"
#include "detection_utils.hpp"
#include "video_source.hpp"
#include "camera_manager.hpp"
#include "model_manager.hpp"
#include <QFileDialog>
#include <QMessageBox>
#include <QScreen>
#include <QPixmap>
#include <QDir>
#include <QCloseEvent>
#include <QAction>
#include <QInputDialog>
#include <filesystem>
namespace fs = std::filesystem;

// ============ Constructor ============
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), confThreshold_(0.25f), nmsThreshold_(0.45f)
{
    ui = new Ui::MainWindow;
    ui->setupUi(this);

    statusMessageLabel_ = new QLabel("Ready", this);
    fpsLabel_  = new QLabel("FPS: --", this);
    timeLabel_ = new QLabel("Time: --", this);
    fpsLabel_->setStyleSheet("margin-right: 15px;");
    timeLabel_->setStyleSheet("margin-right: 15px;");
    statusBar()->addWidget(statusMessageLabel_, 1);
    statusBar()->addPermanentWidget(fpsLabel_);
    statusBar()->addPermanentWidget(timeLabel_);

    // Camera list
    cameraListWidget_ = new QListWidget(this);
    cameraListWidget_->setMaximumHeight(150);
    cameraListWidget_->setStyleSheet(
        "QListWidget { background-color: #1e1e1e; border: 1px solid #444; }"
        "QListWidget::item { padding: 4px 8px; }"
        "QListWidget::item:selected { background-color: #2a3f5f; }");
    auto* rl = qobject_cast<QVBoxLayout*>(ui->rightPanel->layout());
    if (rl) rl->insertWidget(rl->indexOf(ui->resultTitleLabel) + 1, cameraListWidget_);
    connect(cameraListWidget_, &QListWidget::itemClicked, this, &MainWindow::onCameraListClicked);

    // Start Detection button
    auto* bl = qobject_cast<QHBoxLayout*>(ui->buttonLayout);
    if (bl) {
        startDetectBtn_ = new QPushButton("Start Detection", this);
        startDetectBtn_->setStyleSheet("QPushButton{background:#5cb85c;color:white;font-weight:bold;padding:6px 16px;}"
            "QPushButton:disabled{background:#555;color:#888;}");
        startDetectBtn_->setEnabled(false);
        bl->addWidget(startDetectBtn_);
        connect(startDetectBtn_, &QPushButton::clicked, this, &MainWindow::onStartDetection);
    }

    // Model manager
    modelManager_ = std::make_unique<ModelManager>();
    cameraManager_ = std::make_unique<CameraManager>();

    // UI connections
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
    log(GuiLogger::CAT_SYSTEM, "Application started");
}

MainWindow::~MainWindow() {
    stopAllCameras();
    delete ui;
}

