/*
 * mainwindow.cpp - RK3588 Qt GUI 主窗口实现
 * 16:9 自适应布局，左右分栏
 */

#include "rk/mainwindow.hpp"
#include <QDateTime>
#include <QMessageBox>
#include <QGroupBox>
#include <QFrame>
#include <QScrollArea>
#include <QDir>
#include <QFileInfo>
#include <QStyle>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), detect_thread_(nullptr)
{
    setupUI();
    setupStyle();

    // 默认窗口大小 1280x720 (16:9)
    resize(1280, 720);

    // 居中显示
    if (QScreen* screen = QApplication::primaryScreen()) {
        QRect screenGeometry = screen->availableGeometry();
        move((screenGeometry.width() - width()) / 2,
             (screenGeometry.height() - height()) / 2);
    }

    detectCameras();
    log("system", "应用就绪，请加载模型。");
}

MainWindow::~MainWindow() {
    if (detect_thread_) {
        detect_thread_->stop();
        detect_thread_->wait();
        delete detect_thread_;
    }
}

// ============================================================
// 样式表
// ============================================================
void MainWindow::setupStyle() {
    qApp->setStyleSheet(R"(
        /* 全局 - 黑色背景，浅色字体 */
        QMainWindow, QWidget {
            font-family: "Segoe UI", "Noto Sans CJK SC", "WenQuanYi Micro Hei", sans-serif;
            font-size: 13px;
            color: #e8e8e8;
            background-color: #0a0a0a;
        }

        /* 分组框 - 深灰色功能区 */
        QGroupBox {
            font-weight: bold;
            border: 1px solid #3a3a3a;
            border-radius: 6px;
            margin-top: 14px;
            padding: 14px 10px 10px 10px;
            background-color: #1a1a1a;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 8px;
            color: #5dade2;
        }

        /* 输入框 */
        QLineEdit {
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            padding: 6px 10px;
            background-color: #141414;
            color: #f5f5f5;
            selection-background-color: #1a5276;
            font-size: 13px;
        }
        QLineEdit:focus {
            border: 1px solid #3498db;
            background-color: #1a1a1a;
        }
        QLineEdit:disabled {
            background-color: #1a1a1a;
            color: #555555;
        }

        /* 按钮 */
        QPushButton {
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            padding: 7px 16px;
            background-color: #252525;
            color: #f0f0f0;
            min-height: 26px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #333333;
            border: 1px solid #4a4a4a;
            color: #ffffff;
        }
        QPushButton:pressed {
            background-color: #1a1a1a;
        }
        QPushButton:disabled {
            background-color: #1a1a1a;
            color: #444444;
        }

        /* 开始按钮 - 蓝色调 */
        QPushButton#startBtn {
            background-color: #1a5276;
            border-color: #2980b9;
            color: #ffffff;
            font-weight: bold;
        }
        QPushButton#startBtn:hover {
            background-color: #2980b9;
        }
        QPushButton#startBtn:disabled {
            background-color: #1a1a1a;
            color: #444444;
        }

        /* 停止按钮 - 暖红色调 */
        QPushButton#stopBtn {
            background-color: #922b21;
            border-color: #c0392b;
            color: #ffffff;
            font-weight: bold;
        }
        QPushButton#stopBtn:hover {
            background-color: #c0392b;
        }
        QPushButton#stopBtn:disabled {
            background-color: #1a1a1a;
            color: #444444;
        }

        /* 滑块 */
        QSlider::groove:horizontal {
            border: none;
            height: 6px;
            background: #333333;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #3498db;
            border: none;
            width: 16px;
            height: 16px;
            margin: -5px 0;
            border-radius: 8px;
        }
        QSlider::handle:horizontal:hover {
            background: #5dade2;
        }
        QSlider::sub-page:horizontal {
            background: #3498db;
            border-radius: 3px;
        }

        /* SpinBox */
        QSpinBox {
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            padding: 5px 8px;
            background-color: #141414;
            color: #f5f5f5;
            min-height: 26px;
        }

        /* 日志区 */
        QTextEdit {
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            background-color: #0f0f0f;
            color: #e0e0e0;
            font-family: "Cascadia Code", "JetBrains Mono", "Consolas", monospace;
            font-size: 12px;
            padding: 6px;
        }

        /* 摄像头列表 */
        QListWidget {
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            background-color: #141414;
            color: #e8e8e8;
            padding: 4px;
            outline: none;
        }
        QListWidget::item {
            padding: 5px 8px;
            border-radius: 3px;
            margin: 1px 2px;
        }
        QListWidget::item:selected {
            background-color: #1a5276;
            color: #ffffff;
        }
        QListWidget::item:hover {
            background-color: #2a2a2a;
        }

        /* ComboBox */
        QComboBox {
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            padding: 5px 10px;
            background-color: #141414;
            color: #e8e8e8;
            min-height: 26px;
        }
        QComboBox:hover {
            border: 1px solid #4a4a4a;
        }
        QComboBox::drop-down {
            border: none;
            width: 22px;
        }
        QComboBox QAbstractItemView {
            background-color: #141414;
            color: #e8e8e8;
            selection-background-color: #1a5276;
            border: 1px solid #3a3a3a;
        }

        /* ProgressBar */
        QProgressBar {
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            background-color: #141414;
            text-align: center;
            color: #e8e8e8;
            min-height: 20px;
            font-size: 12px;
        }
        QProgressBar::chunk {
            background-color: #1a5276;
            border-radius: 3px;
        }

        /* 状态栏 */
        QStatusBar {
            background-color: #1a1a1a;
            color: #e8e8e8;
            font-size: 12px;
            border-top: 1px solid #3a3a3a;
        }

        /* Splitter */
        QSplitter::handle {
            background-color: #2a2a2a;
        }
        QSplitter::handle:horizontal {
            width: 3px;
        }
        QSplitter::handle:vertical {
            height: 3px;
        }
        QSplitter::handle:hover {
            background-color: #3498db;
        }

        /* Label 样式 */
        QLabel {
            color: #e0e0e0;
        }
        QLabel#statusValue {
            color: #2ecc71;
            font-weight: bold;
            font-size: 13px;
        }
        QLabel#statusHeader {
            color: #95a5a6;
            font-size: 12px;
        }
        QLabel#overlayLabel {
            background-color: rgba(0, 0, 0, 200);
            color: #2ecc71;
            font-family: "Cascadia Code", "Consolas", monospace;
            font-size: 14px;
            padding: 5px 10px;
            border-radius: 4px;
            font-weight: bold;
        }

        /* ScrollBar */
        QScrollBar:vertical {
            background-color: #141414;
            width: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background-color: #444444;
            min-height: 30px;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #555555;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar:horizontal {
            background-color: #141414;
            height: 10px;
        }
        QScrollBar::handle:horizontal {
            background-color: #444444;
            min-width: 30px;
            border-radius: 5px;
        }
        QScrollBar::handle:horizontal:hover {
            background-color: #555555;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
    )");
}

// ============================================================
// UI 布局
// ============================================================
void MainWindow::setupUI() {
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    // =============================================
    // 主分割器：左右分栏 (2:1 比例)
    // =============================================
    QSplitter* main_splitter = new QSplitter(Qt::Horizontal, this);
    main_splitter->setHandleWidth(4);

    // =============================================
    // 左侧面板：检测窗口 + 控制区
    // =============================================
    QWidget* left_panel = new QWidget();
    QVBoxLayout* left_layout = new QVBoxLayout(left_panel);
    left_layout->setContentsMargins(0, 0, 0, 0);
    left_layout->setSpacing(6);

    // -- 检测显示区 (占据主要空间) --
    display_label_ = new QLabel("请先加载模型并选择视频源", left_panel);
    display_label_->setAlignment(Qt::AlignCenter);
    display_label_->setMinimumSize(320, 240);
    display_label_->setStyleSheet(
        "QLabel { border: 2px dashed #3a3a3a; border-radius: 8px; "
        "background-color: #0a0a0a; color: #555555; font-size: 15px; }");

    // FPS 叠加层
    display_overlay_ = new QLabel("帧率: --", display_label_);
    display_overlay_->setObjectName("overlayLabel");
    display_overlay_->move(10, 10);
    display_overlay_->adjustSize();

    left_layout->addWidget(display_label_, 3);  // 检测窗口占 3 份

    // -- 控制区 (底部) --
    QFrame* control_frame = new QFrame(left_panel);
    control_frame->setFrameStyle(QFrame::StyledPanel);
    QVBoxLayout* ctrl_layout = new QVBoxLayout(control_frame);
    ctrl_layout->setContentsMargins(8, 8, 8, 8);
    ctrl_layout->setSpacing(6);

    // 模型行
    QHBoxLayout* model_row = new QHBoxLayout();
    model_row->setSpacing(4);
    model_edit_ = new QLineEdit("./model/RK3588/yolov5s-640-640.rknn", left_panel);
    model_btn_ = new QPushButton("浏览", left_panel);
    model_btn_->setMaximumWidth(70);
    load_btn_ = new QPushButton("加载模型", left_panel);
    load_btn_->setObjectName("startBtn");
    load_btn_->setMaximumWidth(90);
    model_status_label_ = new QLabel("未加载", left_panel);
    model_status_label_->setStyleSheet("color: #8a9bb0; font-size: 12px;");

    model_row->addWidget(new QLabel("模型:", left_panel));
    model_row->addWidget(model_edit_, 1);
    model_row->addWidget(model_btn_);
    model_row->addWidget(load_btn_);
    model_row->addWidget(model_status_label_);
    ctrl_layout->addLayout(model_row);

    // 视频行
    QHBoxLayout* video_row = new QHBoxLayout();
    video_row->setSpacing(4);
    video_edit_ = new QLineEdit(left_panel);
    video_edit_->setPlaceholderText("视频文件路径或摄像头ID（如 0）");
    video_btn_ = new QPushButton("浏览", left_panel);
    video_btn_->setMaximumWidth(70);

    video_row->addWidget(new QLabel("来源:", left_panel));
    video_row->addWidget(video_edit_, 1);
    video_row->addWidget(video_btn_);
    ctrl_layout->addLayout(video_row);

    // 按钮行
    QHBoxLayout* btn_row = new QHBoxLayout();
    btn_row->setSpacing(6);
    start_btn_ = new QPushButton("开始检测", left_panel);
    start_btn_->setObjectName("startBtn");
    start_btn_->setEnabled(false);
    stop_btn_ = new QPushButton("停止", left_panel);
    stop_btn_->setObjectName("stopBtn");
    stop_btn_->setEnabled(false);

    thread_spin_ = new QSpinBox(left_panel);
    thread_spin_->setRange(1, 8);
    thread_spin_->setValue(3);

    btn_row->addWidget(start_btn_);
    btn_row->addWidget(stop_btn_);
    btn_row->addStretch();
    btn_row->addWidget(new QLabel("线程数:", left_panel));
    btn_row->addWidget(thread_spin_);
    ctrl_layout->addLayout(btn_row);

    // 阈值行
    QHBoxLayout* thr_row = new QHBoxLayout();
    thr_row->setSpacing(6);
    conf_slider_ = new QSlider(Qt::Horizontal, left_panel);
    conf_slider_->setRange(0, 100);
    conf_slider_->setValue(25);
    conf_value_label_ = new QLabel("0.25", left_panel);
    conf_value_label_->setMinimumWidth(36);

    nms_slider_ = new QSlider(Qt::Horizontal, left_panel);
    nms_slider_->setRange(0, 100);
    nms_slider_->setValue(45);
    nms_value_label_ = new QLabel("0.45", left_panel);
    nms_value_label_->setMinimumWidth(36);

    thr_row->addWidget(new QLabel("置信度:", left_panel));
    thr_row->addWidget(conf_slider_, 1);
    thr_row->addWidget(conf_value_label_);
    thr_row->addSpacing(12);
    thr_row->addWidget(new QLabel("NMS:", left_panel));
    thr_row->addWidget(nms_slider_, 1);
    thr_row->addWidget(nms_value_label_);
    ctrl_layout->addLayout(thr_row);

    left_layout->addWidget(control_frame, 0);  // 控制区不伸缩

    main_splitter->addWidget(left_panel);

    // =============================================
    // 右侧面板：状态 + 摄像头列表 + 日志
    // =============================================
    QWidget* right_panel = new QWidget();
    QVBoxLayout* right_layout = new QVBoxLayout(right_panel);
    right_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->setSpacing(6);

    // -- 右侧上半部分：状态信息 + 摄像头列表 --
    QSplitter* right_top_splitter = new QSplitter(Qt::Vertical, right_panel);
    right_top_splitter->setHandleWidth(3);

    // 状态信息组
    QGroupBox* status_group = new QGroupBox("状态信息", right_panel);
    QGridLayout* status_grid = new QGridLayout(status_group);
    status_grid->setSpacing(6);
    status_grid->setContentsMargins(10, 16, 10, 10);

    auto makeStatusLabel = [](const QString& text) -> QLabel* {
        QLabel* lbl = new QLabel(text);
        lbl->setObjectName("statusHeader");
        return lbl;
    };
    auto makeStatusValue = [](const QString& text) -> QLabel* {
        QLabel* lbl = new QLabel(text);
        lbl->setObjectName("statusValue");
        return lbl;
    };

    model_name_label_ = makeStatusValue("--");
    model_status_label_ = makeStatusValue("未加载");
    fps_label_ = makeStatusValue("--");
    resolution_label_ = makeStatusValue("--");
    frames_label_ = makeStatusValue("0");
    infer_time_label_ = makeStatusValue("--");
    npu_usage_bar_ = new QProgressBar(right_panel);
    npu_usage_bar_->setRange(0, 100);
    npu_usage_bar_->setValue(0);
    npu_usage_bar_->setFormat("NPU: %p%");

    status_grid->addWidget(makeStatusLabel("模型:"), 0, 0);
    status_grid->addWidget(model_name_label_, 0, 1);
    status_grid->addWidget(makeStatusLabel("状态:"), 0, 2);
    status_grid->addWidget(model_status_label_, 0, 3);

    status_grid->addWidget(makeStatusLabel("帧率:"), 1, 0);
    status_grid->addWidget(fps_label_, 1, 1);
    status_grid->addWidget(makeStatusLabel("分辨率:"), 1, 2);
    status_grid->addWidget(resolution_label_, 1, 3);

    status_grid->addWidget(makeStatusLabel("帧数:"), 2, 0);
    status_grid->addWidget(frames_label_, 2, 1);
    status_grid->addWidget(makeStatusLabel("推理耗时:"), 2, 2);
    status_grid->addWidget(infer_time_label_, 2, 3);

    status_grid->addWidget(npu_usage_bar_, 3, 0, 1, 4);

    right_top_splitter->addWidget(status_group);

    // 摄像头列表组
    QGroupBox* camera_group = new QGroupBox("摄像头", right_panel);
    QVBoxLayout* cam_layout = new QVBoxLayout(camera_group);
    cam_layout->setContentsMargins(10, 16, 10, 10);

    QHBoxLayout* cam_header = new QHBoxLayout();
    camera_combo_ = new QComboBox(right_panel);
    refresh_cam_btn_ = new QPushButton("刷新", right_panel);
    refresh_cam_btn_->setMaximumWidth(70);
    cam_header->addWidget(camera_combo_, 1);
    cam_header->addWidget(refresh_cam_btn_);
    cam_layout->addLayout(cam_header);

    camera_list_ = new QListWidget(right_panel);
    camera_list_->setMinimumHeight(60);
    cam_layout->addWidget(camera_list_, 1);

    right_top_splitter->addWidget(camera_group);
    right_top_splitter->setStretchFactor(0, 2);  // 状态信息占 2 份
    right_top_splitter->setStretchFactor(1, 1);  // 摄像头列表占 1 份

    right_layout->addWidget(right_top_splitter, 1);

    // -- 右侧下半部分：日志 --
    QGroupBox* log_group = new QGroupBox("运行日志", right_panel);
    QVBoxLayout* log_layout = new QVBoxLayout(log_group);
    log_layout->setContentsMargins(10, 16, 10, 10);

    QHBoxLayout* log_header = new QHBoxLayout();
    log_header->addStretch();
    clear_log_btn_ = new QPushButton("清空", right_panel);
    clear_log_btn_->setMaximumWidth(60);
    log_header->addWidget(clear_log_btn_);
    log_layout->addLayout(log_header);

    log_edit_ = new QTextEdit(right_panel);
    log_edit_->setReadOnly(true);
    log_layout->addWidget(log_edit_, 1);

    right_layout->addWidget(log_group, 1);  // 日志区占 1 份

    main_splitter->addWidget(right_panel);

    // 设置左右分割比例 (2:1)
    main_splitter->setStretchFactor(0, 2);
    main_splitter->setStretchFactor(1, 1);
    main_splitter->setSizes({850, 430});

    // 主布局
    QVBoxLayout* main_layout = new QVBoxLayout(central);
    main_layout->setContentsMargins(4, 4, 4, 4);
    main_layout->addWidget(main_splitter);

    // =============================================
    // 状态栏
    // =============================================
    status_label_ = new QLabel("就绪", this);
    statusBar()->addWidget(status_label_, 1);
    statusBar()->addPermanentWidget(new QLabel("RK3588 YOLO 目标检测", this));

    // =============================================
    // 信号槽连接
    // =============================================
    connect(model_btn_, &QPushButton::clicked, this, &MainWindow::onBrowseModel);
    connect(load_btn_, &QPushButton::clicked, this, &MainWindow::onLoadModel);
    connect(video_btn_, &QPushButton::clicked, this, &MainWindow::onBrowseVideo);
    connect(start_btn_, &QPushButton::clicked, this, &MainWindow::onStartDetection);
    connect(stop_btn_, &QPushButton::clicked, this, &MainWindow::onStopDetection);
    connect(conf_slider_, &QSlider::valueChanged, this, &MainWindow::onConfThresholdChanged);
    connect(nms_slider_, &QSlider::valueChanged, this, &MainWindow::onNmsThresholdChanged);
    connect(clear_log_btn_, &QPushButton::clicked, this, &MainWindow::onClearLog);
    connect(refresh_cam_btn_, &QPushButton::clicked, this, &MainWindow::onRefreshCameras);
    connect(camera_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onCameraSelected);
}

// ============================================================
// 摄像头检测
// ============================================================
void MainWindow::detectCameras() {
    camera_combo_->clear();
    camera_list_->clear();

    for (int i = 0; i < 10; i++) {
        cv::VideoCapture cap(i);
        if (cap.isOpened()) {
            int w = (int)cap.get(cv::CAP_PROP_FRAME_WIDTH);
            int h = (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT);
            QString info = QString("摄像头 %1 (%2x%3)").arg(i).arg(w).arg(h);
            camera_combo_->addItem(info, i);
            camera_list_->addItem(info);
            cap.release();
        }
    }

    if (camera_combo_->count() == 0) {
        camera_combo_->addItem("-- 未检测到摄像头 --", -1);
        camera_list_->addItem("未检测到摄像头");
    } else {
        camera_combo_->insertItem(0, "-- 选择摄像头 --", -1);
        camera_combo_->setCurrentIndex(0);
    }

    log("system", QString("检测到 %1 个摄像头。").arg(camera_combo_->count()));
}

// ============================================================
// 辅助方法
// ============================================================
void MainWindow::log(const QString& category, const QString& message) {
    QString ts = currentTimestamp();
    QString line = QString("<span style='color:#6a7a8a'>[%1]</span> "
                           "<span style='color:#7ab8f5'>[%2]</span> %3")
                       .arg(ts, category, message);
    log_edit_->append(line);

    // 自动滚动到底部
    QTextCursor cursor = log_edit_->textCursor();
    cursor.movePosition(QTextCursor::End);
    log_edit_->setTextCursor(cursor);
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

QString MainWindow::formatSize(qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 2);
}

// ============================================================
// 槽函数
// ============================================================
void MainWindow::onBrowseModel() {
    QString p = QFileDialog::getOpenFileName(this, "选择 RKNN 模型", "", "RKNN 模型 (*.rknn);;所有文件 (*.*)");
    if (!p.isEmpty()) model_edit_->setText(p);
}

void MainWindow::onLoadModel() {
    model_path_ = model_edit_->text().toStdString();
    if (model_path_.empty()) {
        QMessageBox::warning(this, "错误", "请选择模型文件。");
        return;
    }

    FILE* f = fopen(model_path_.c_str(), "rb");
    if (!f) {
        QMessageBox::warning(this, "错误", "模型文件未找到：" + QString::fromStdString(model_path_));
        return;
    }
    fseek(f, 0, SEEK_END);
    qint64 size = ftell(f);
    fclose(f);

    QFileInfo fi(QString::fromStdString(model_path_));

    model_status_label_->setText("已加载");
    model_status_label_->setStyleSheet("color: #4ec9b0; font-weight: bold;");
    model_name_label_->setText(fi.fileName());
    start_btn_->setEnabled(true);
    status_label_->setText("模型已加载：" + fi.fileName());
    log("model", QString("已加载：%1 (%2)").arg(fi.fileName(), formatSize(size)));
}

void MainWindow::onBrowseVideo() {
    QString p = QFileDialog::getOpenFileName(this, "打开视频", "", "视频文件 (*.mp4 *.avi *.mkv *.mov);;所有文件 (*.*)");
    if (!p.isEmpty()) video_edit_->setText(p);
}

void MainWindow::onRefreshCameras() {
    detectCameras();
}

void MainWindow::onCameraSelected(int index) {
    if (index >= 0) {
        int cam_id = camera_combo_->itemData(index).toInt();
        if (cam_id >= 0) {
            video_edit_->setText(QString::number(cam_id));
        }
    }
}

void MainWindow::onStartDetection() {
    if (model_path_.empty()) {
        QMessageBox::warning(this, "错误", "请先加载模型。");
        return;
    }

    video_path_ = video_edit_->text().toStdString();
    if (video_path_.empty()) {
        QMessageBox::warning(this, "错误", "请选择视频文件或输入摄像头ID。");
        return;
    }

    int thread_num = thread_spin_->value();

    detect_thread_ = new DetectThread(model_path_, video_path_, thread_num);
    connect(detect_thread_, &DetectThread::frameReady, this, &MainWindow::onFrameReady);
    connect(detect_thread_, &DetectThread::statsUpdated, this, &MainWindow::onStatsUpdated);
    connect(detect_thread_, &DetectThread::finished, this, &MainWindow::onDetectFinished);
    connect(detect_thread_, &DetectThread::error, this, &MainWindow::onDetectError);

    enableControls(true);
    model_status_label_->setText("运行中");
    model_status_label_->setStyleSheet("color: #ffb74d; font-weight: bold;");
    frames_label_->setText("0");
    infer_time_label_->setText("--");

    detect_thread_->start();
    log("system", QString("检测开始。来源：%1，线程数：%2")
                       .arg(QString::fromStdString(video_path_)).arg(thread_num));
    status_label_->setText("检测中...");
}

void MainWindow::onStopDetection() {
    if (detect_thread_) {
        detect_thread_->stop();
        log("system", "正在停止检测...");
        status_label_->setText("停止中...");
    }
}

void MainWindow::onFrameReady(const QImage& image, double fps) {
    QPixmap pix = QPixmap::fromImage(image);
    display_label_->setPixmap(pix.scaled(display_label_->size(),
                                          Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));
    // 更新叠加层位置
    display_overlay_->setText(QString("帧率: %1").arg(fps, 0, 'f', 1));
    display_overlay_->adjustSize();
    display_overlay_->move(10, 10);

    // 更新分辨率
    resolution_label_->setText(QString("%1x%2").arg(image.width()).arg(image.height()));
}

void MainWindow::onStatsUpdated(int frames, double fps, double inferTime) {
    fps_label_->setText(QString("%1").arg(fps, 0, 'f', 1));
    frames_label_->setText(QString::number(frames));
    if (inferTime > 0) {
        infer_time_label_->setText(QString("%1 ms").arg(inferTime, 0, 'f', 1));
    }

    // 模拟 NPU 使用率 (实际应从 /sys 读取)
    int usage = qBound(0, (int)(fps * 3), 100);
    npu_usage_bar_->setValue(usage);
}

void MainWindow::onDetectFinished() {
    enableControls(false);
    if (detect_thread_) {
        delete detect_thread_;
        detect_thread_ = nullptr;
    }
    model_status_label_->setText("已完成");
    model_status_label_->setStyleSheet("color: #8a9bb0; font-weight: bold;");
    status_label_->setText("检测完成。");
    log("system", "检测完成。");
}

void MainWindow::onDetectError(const QString& msg) {
    log("info", msg);
    if (msg.contains("finished")) {
        // 正常结束信息，不弹窗
    }
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
