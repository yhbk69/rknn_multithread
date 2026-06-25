/*
 * mainwindow.cpp - RK3588 Qt GUI 主窗口
 * 构造/析构 + UI 布局 + 样式表
 */

#include "rk/mainwindow.hpp"
#include <QDateTime>
#include <QGroupBox>
#include <QFrame>
#include <QFileInfo>
#include <QNetworkInterface>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include "Logger.hpp"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), expanded_ch_(-1)
{
    /* 注册自定义类型，用于跨线程信号槽传递 */
    qRegisterMetaType<FrameDetections::Det>("FrameDetections::Det");
    qRegisterMetaType<QVector<FrameDetections::Det>>("QVector<FrameDetections::Det>");

    AppConfig init_cfg = load_config();
    conf_threshold_ = init_cfg.box_threshold;
    nms_threshold_ = init_cfg.nms_threshold;

    setupUI();
    setupStyle();

    resize(1600, 900);

    if (QScreen* screen = QApplication::primaryScreen()) {
        QRect screenGeometry = screen->availableGeometry();
        move((screenGeometry.width() - width()) / 2,
             (screenGeometry.height() - height()) / 2);
    }

    restoreSettings();
    log("system", "应用就绪，请加载模型。");

    ws_server_ = std::make_shared<WebSocket>(this);

    connect(ws_server_.get(), &WebSocket::serverStarted,
            this, &MainWindow::onWebSocketStarted);
    connect(ws_server_.get(), &WebSocket::clientConnected,
            this, &MainWindow::onWebSocketClientConnected);
    connect(ws_server_.get(), &WebSocket::clientDisconnected,
            this, &MainWindow::onWebSocketClientDisconnected);
    connect(ws_server_.get(), &WebSocket::alarmTriggered,
            this, &MainWindow::onWebSocketAlarm);

    WebSocketConfig ws_cfg;
    ws_cfg.server_host = QString::fromStdString(init_cfg.ws_host);
    ws_cfg.server_port = init_cfg.ws_port;
    ws_cfg.enable_alarm = true;
    for (const auto &name : init_cfg.alarm_class_names)
        ws_cfg.alarm_class_names.insert(name);
    ws_cfg.alarm_screenshot_dir = init_cfg.alarm_screenshot_dir;
    if (ws_server_->init(ws_cfg) == 0) {
        log("websocket", QString("服务器已启动: ws://%1:%2")
            .arg(ws_cfg.server_host).arg(ws_cfg.server_port));
    } else {
        log("websocket", "服务器启动失败！");
    }

    config_watcher_ = std::make_unique<QFileSystemWatcher>(this);
    config_watcher_->addPath(QFileInfo("config.json").absoluteFilePath());
    connect(config_watcher_.get(), &QFileSystemWatcher::fileChanged,
            this, &MainWindow::onConfigFileChanged);
    log("system", "已启用配置热更新，修改 config.json 自动生效。");

    /* 帧队列轮询定时器：每 16ms（~60Hz）从共享队列取帧并更新显示 */
    frame_poll_timer_ = std::make_unique<QTimer>(this);
    connect(frame_poll_timer_.get(), &QTimer::timeout, this, &MainWindow::onPollFrames);
    frame_poll_timer_->start(16);
}

MainWindow::~MainWindow() {
    saveSettings();
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (detect_threads_[i]) {
            detect_threads_[i]->stop();
            detect_threads_[i]->wait();
            delete detect_threads_[i];
            detect_threads_[i] = nullptr;
        }
    }
    if (ws_server_) {
        ws_server_->shutdown();
        ws_server_.reset();
    }
}

// ============================================================
// UI 布局
// ============================================================
void MainWindow::setupUI() {
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    AppConfig cfg = load_config();

    QSplitter* main_splitter = new QSplitter(Qt::Horizontal, this);
    main_splitter->setHandleWidth(4);

    // 左侧面板
    QWidget* left_panel = new QWidget();
    QVBoxLayout* left_layout = new QVBoxLayout(left_panel);
    left_layout->setContentsMargins(0, 0, 0, 0);
    left_layout->setSpacing(4);

    // 2x2 视频网格
    QWidget* grid_widget = new QWidget(left_panel);
    grid_layout_ = new QGridLayout(grid_widget);
    grid_layout_->setContentsMargins(2, 2, 2, 2);
    grid_layout_->setSpacing(4);

    const char* channel_names[] = {"通道1", "通道2", "通道3", "通道4"};

    for (int i = 0; i < MAX_CHANNELS; i++) {
        VideoCell& cell = video_cells_[i];

        cell.container = new QWidget(grid_widget);
        QVBoxLayout* cell_layout = new QVBoxLayout(cell.container);
        cell_layout->setContentsMargins(0, 0, 0, 0);
        cell_layout->setSpacing(0);

        QWidget* top_bar = new QWidget(cell.container);
        QHBoxLayout* top_layout = new QHBoxLayout(top_bar);
        top_layout->setContentsMargins(6, 2, 6, 2);
        top_layout->setSpacing(4);

        cell.channel_label = new QLabel(channel_names[i], top_bar);
        cell.channel_label->setStyleSheet("color: #5dade2; font-weight: bold; font-size: 14px;");
        cell.overlay = new QLabel("FPS: --", top_bar);
        cell.overlay->setStyleSheet("color: #2ecc71; font-size: 13px; font-weight: bold;");

        cell.zoom_in_btn = new QPushButton("放大", top_bar);
        cell.zoom_in_btn->setFixedSize(40, 20);
        cell.zoom_in_btn->setStyleSheet(
            "QPushButton { background-color: #1a5276; color: white; border: none; border-radius: 3px; font-size: 10px; }"
            "QPushButton:hover { background-color: #2980b9; }");
        cell.zoom_out_btn = new QPushButton("缩小", top_bar);
        cell.zoom_out_btn->setFixedSize(40, 20);
        cell.zoom_out_btn->setStyleSheet(
            "QPushButton { background-color: #922b21; color: white; border: none; border-radius: 3px; font-size: 10px; }"
            "QPushButton:hover { background-color: #c0392b; }");
        cell.zoom_out_btn->hide();

        top_layout->addWidget(cell.channel_label);
        top_layout->addWidget(cell.overlay);
        top_layout->addStretch();
        top_layout->addWidget(cell.zoom_in_btn);
        top_layout->addWidget(cell.zoom_out_btn);

        cell.view = new QGraphicsView(cell.container);
        cell.view->setRenderHint(QPainter::Antialiasing);
        cell.view->setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
        cell.view->setStyleSheet(
            "QGraphicsView { border: 1px solid #3a3a3a; border-radius: 4px; background-color: #0a0a0a; }");
        cell.scene = new QGraphicsScene(cell.view);
        cell.view->setScene(cell.scene);
        cell.pixmap_item = cell.scene->addPixmap(QPixmap());
        cell.scene->setBackgroundBrush(QBrush(QColor("#0a0a0a")));

        cell_layout->addWidget(top_bar);
        cell_layout->addWidget(cell.view, 1);

        grid_layout_->addWidget(cell.container, i / 2, i % 2);

        int ch = i;
        connect(cell.zoom_in_btn, &QPushButton::clicked, this, [this, ch]() { onZoomIn(ch); });
        connect(cell.zoom_out_btn, &QPushButton::clicked, this, &MainWindow::onZoomOut);
    }

    left_layout->addWidget(grid_widget, 3);

    // 控制区
    QFrame* control_frame = new QFrame(left_panel);
    control_frame->setFrameStyle(QFrame::StyledPanel);
    QVBoxLayout* ctrl_layout = new QVBoxLayout(control_frame);
    ctrl_layout->setContentsMargins(8, 8, 8, 8);
    ctrl_layout->setSpacing(4);

    // 模型行
    QHBoxLayout* model_row = new QHBoxLayout();
    model_row->setSpacing(4);
    model_edit_ = new QLineEdit(QString::fromStdString(cfg.model_path), left_panel);
    model_btn_ = new QPushButton("浏览", left_panel);
    model_btn_->setMaximumWidth(70);
    load_btn_ = new QPushButton("加载模型", left_panel);
    load_btn_->setObjectName("startBtn");
    load_btn_->setMaximumWidth(90);
    model_status_left_label_ = new QLabel("未加载", left_panel);
    model_status_left_label_->setStyleSheet("color: #8a9bb0; font-size: 12px;");

    model_row->addWidget(new QLabel("模型:", left_panel));
    model_row->addWidget(model_edit_, 1);
    model_row->addWidget(model_btn_);
    model_row->addWidget(load_btn_);
    model_row->addWidget(model_status_left_label_);
    ctrl_layout->addLayout(model_row);

    // 4路视频源行
    for (int i = 0; i < MAX_CHANNELS; i++) {
        QHBoxLayout* video_row = new QHBoxLayout();
        video_row->setSpacing(4);
        video_edits_[i] = new QLineEdit(left_panel);
        video_edits_[i]->setPlaceholderText(QString("通道%1: 视频路径或摄像头ID").arg(i + 1));
        video_edits_[i]->setFixedHeight(32);
        video_alias_edits_[i] = new QLineEdit(left_panel);
        video_alias_edits_[i]->setPlaceholderText("别名");
        video_alias_edits_[i]->setMaximumWidth(80);
        video_alias_edits_[i]->setFixedHeight(32);
        video_btns_[i] = new QPushButton("浏览", left_panel);
        video_btns_[i]->setFixedWidth(70);
        video_btns_[i]->setFixedHeight(32);
        video_btns_[i]->setStyleSheet(
            "QPushButton { padding: 4px 8px; min-height: 0px; }"
            "QPushButton:hover { background-color: #333333; border: 1px solid #4a4a4a; color: #ffffff; }"
            "QPushButton:pressed { background-color: #1a1a1a; }");
        video_del_btns_[i] = new QPushButton("X", left_panel);
        video_del_btns_[i]->setFixedSize(32, 32);
        video_del_btns_[i]->setToolTip("清空此路视频源");
        video_del_btns_[i]->setStyleSheet(
            "QPushButton { background-color: #922b21; color: #ffffff; border: none; border-radius: 4px; "
            "padding: 0px; min-height: 0px; font-size: 16px; font-weight: bold; }"
            "QPushButton:hover { background-color: #c0392b; }");

        QLabel* road_label = new QLabel(QString("路%1:").arg(i + 1), left_panel);
        road_label->setStyleSheet("color: #e8e8e8; font-size: 13px; font-weight: bold;");
        video_row->addWidget(road_label);
        video_row->addWidget(video_edits_[i], 1);
        video_row->addWidget(video_alias_edits_[i]);
        video_row->addWidget(video_btns_[i]);
        video_row->addWidget(video_del_btns_[i]);
        ctrl_layout->addLayout(video_row);

        int ch = i;
        connect(video_btns_[i], &QPushButton::clicked, this, [this, ch]() { onBrowseVideo(ch); });
        connect(video_del_btns_[i], &QPushButton::clicked, this, [this, ch]() {
            if (detect_threads_[ch] && !detect_threads_[ch]->isPaused()) {
                QMessageBox::warning(this, "提示", "检测运行中不可删除视频源");
                return;
            }
            if (detect_threads_[ch] && detect_threads_[ch]->isPaused()) {
                detect_threads_[ch]->stop();
                detect_threads_[ch]->wait();
                delete detect_threads_[ch];
                detect_threads_[ch] = nullptr;
                bool all_done = true;
                for (int j = 0; j < MAX_CHANNELS; j++)
                    if (detect_threads_[j]) { all_done = false; break; }
                if (all_done) {
                    enableControls(false);
                    model_status_label_->setText("已完成");
                    model_status_label_->setStyleSheet("color: #8a9bb0; font-weight: bold;");
                    model_status_left_label_->setText("已选择");
                    model_status_left_label_->setStyleSheet("color: #4ec9b0; font-weight: bold;");
                    status_label_->setText("所有通道检测完成。");
                }
                log("system", QString("通道%1 已停止（暂停状态下删除视频源）").arg(ch + 1));
            }
            video_edits_[ch]->clear();
            video_alias_edits_[ch]->clear();
            video_cells_[ch].channel_label->setText(QString("通道%1").arg(ch + 1));
            video_cells_[ch].pixmap_item->setPixmap(QPixmap());
            video_cells_[ch].overlay->setText("FPS: --");
            video_cells_[ch].last_frame = QImage();
            fps_labels_[ch]->setText("--");
            frames_labels_[ch]->setText("0");
            infer_time_labels_[ch]->setText("--");
            last_frames_[ch] = QImage();
        });
        connect(video_alias_edits_[i], &QLineEdit::textChanged, this, [this, ch](const QString& text) {
            if (text.isEmpty())
                video_cells_[ch].channel_label->setText(QString("通道%1").arg(ch + 1));
            else
                video_cells_[ch].channel_label->setText(QString("通道%1(%2)").arg(ch + 1).arg(text));
        });
    }

    // 按钮行
    QHBoxLayout* btn_row = new QHBoxLayout();
    btn_row->setSpacing(6);
    start_btn_ = new QPushButton("开始检测", left_panel);
    start_btn_->setObjectName("startBtn");
    start_btn_->setEnabled(false);
    stop_btn_ = new QPushButton("停止", left_panel);
    stop_btn_->setObjectName("stopBtn");
    stop_btn_->setEnabled(false);
    pause_btn_ = new QPushButton("暂停", left_panel);
    pause_btn_->setEnabled(false);
    step_btn_ = new QPushButton("单帧", left_panel);
    step_btn_->setEnabled(false);
    screenshot_btn_ = new QPushButton("截图", left_panel);
    screenshot_btn_->setEnabled(false);
    export_btn_ = new QPushButton("导出结果", left_panel);
    export_btn_->setEnabled(false);
    record_btn_ = new QPushButton("录制", left_panel);
    record_btn_->setEnabled(false);
    record_btn_->setCheckable(true);

    thread_spin_ = new QSpinBox(left_panel);
    thread_spin_->setRange(1, 8);
    thread_spin_->setValue(cfg.thread_num);

    btn_row->addWidget(start_btn_);
    btn_row->addWidget(stop_btn_);
    btn_row->addWidget(pause_btn_);
    btn_row->addWidget(step_btn_);
    btn_row->addWidget(screenshot_btn_);
    btn_row->addWidget(export_btn_);
    btn_row->addWidget(record_btn_);
    btn_row->addStretch();
    btn_row->addWidget(new QLabel("线程数:", left_panel));
    btn_row->addWidget(thread_spin_);
    ctrl_layout->addLayout(btn_row);

    // 阈值行
    QHBoxLayout* thr_row = new QHBoxLayout();
    thr_row->setSpacing(6);
    conf_slider_ = new QSlider(Qt::Horizontal, left_panel);
    conf_slider_->setRange(0, 100);
    conf_slider_->setValue((int)(cfg.box_threshold * 100));
    conf_value_label_ = new QLabel(QString::number(cfg.box_threshold, 'f', 2), left_panel);
    conf_value_label_->setMinimumWidth(36);

    nms_slider_ = new QSlider(Qt::Horizontal, left_panel);
    nms_slider_->setRange(0, 100);
    nms_slider_->setValue((int)(cfg.nms_threshold * 100));
    nms_value_label_ = new QLabel(QString::number(cfg.nms_threshold, 'f', 2), left_panel);
    nms_value_label_->setMinimumWidth(36);

    thr_row->addWidget(new QLabel("置信度:", left_panel));
    thr_row->addWidget(conf_slider_, 1);
    thr_row->addWidget(conf_value_label_);
    thr_row->addSpacing(12);
    thr_row->addWidget(new QLabel("NMS:", left_panel));
    thr_row->addWidget(nms_slider_, 1);
    thr_row->addWidget(nms_value_label_);
    ctrl_layout->addLayout(thr_row);

    // 报警截图目录行
    QHBoxLayout* screenshot_dir_row = new QHBoxLayout();
    screenshot_dir_row->setSpacing(4);
    alarm_screenshot_edit_ = new QLineEdit(QString::fromStdString(cfg.alarm_screenshot_dir), left_panel);
    alarm_screenshot_edit_->setPlaceholderText("报警截图保存目录（空=不保存）");
    alarm_screenshot_btn_ = new QPushButton("浏览", left_panel);
    alarm_screenshot_btn_->setMaximumWidth(70);
    screenshot_dir_row->addWidget(new QLabel("报警截图:", left_panel));
    screenshot_dir_row->addWidget(alarm_screenshot_edit_, 1);
    screenshot_dir_row->addWidget(alarm_screenshot_btn_);
    ctrl_layout->addLayout(screenshot_dir_row);

    connect(alarm_screenshot_btn_, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "选择报警截图保存目录",
                                                        alarm_screenshot_edit_->text());
        if (!dir.isEmpty()) {
            alarm_screenshot_edit_->setText(dir);
        }
    });
    connect(alarm_screenshot_edit_, &QLineEdit::textChanged, this, [this](const QString &text) {
        std::string dir = text.toStdString();
        syncAlarmScreenshotDirToConfig(dir);
        if (ws_server_) {
            WebSocketConfig wscfg;
            wscfg.alarm_screenshot_dir = dir;
            ws_server_->init(wscfg);
        }
    });

    left_layout->addWidget(control_frame, 0);

    main_splitter->addWidget(left_panel);

    // 右侧面板
    QWidget* right_panel = new QWidget();
    QVBoxLayout* right_layout = new QVBoxLayout(right_panel);
    right_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->setSpacing(6);

    QGroupBox* status_group = new QGroupBox("通道状态", right_panel);
    QGridLayout* status_grid = new QGridLayout(status_group);
    status_grid->setSpacing(4);
    status_grid->setContentsMargins(10, 16, 10, 10);

    status_grid->addWidget(new QLabel(""), 0, 0);
    status_grid->addWidget(new QLabel("FPS"), 0, 1, Qt::AlignCenter);
    status_grid->addWidget(new QLabel("帧数"), 0, 2, Qt::AlignCenter);

    for (int i = 0; i < MAX_CHANNELS; i++) {
        QLabel* ch_label = new QLabel(QString("通道%1").arg(i + 1));
        ch_label->setStyleSheet("color: #5dade2; font-weight: bold;");
        fps_labels_[i] = new QLabel("--");
        fps_labels_[i]->setObjectName("statusValue");
        fps_labels_[i]->setAlignment(Qt::AlignCenter);
        frames_labels_[i] = new QLabel("0");
        frames_labels_[i]->setObjectName("statusValue");
        frames_labels_[i]->setAlignment(Qt::AlignCenter);
        infer_time_labels_[i] = new QLabel("--");
        infer_time_labels_[i]->setObjectName("statusValue");
        infer_time_labels_[i]->setAlignment(Qt::AlignCenter);

        status_grid->addWidget(ch_label, i + 1, 0);
        status_grid->addWidget(fps_labels_[i], i + 1, 1);
        status_grid->addWidget(frames_labels_[i], i + 1, 2);
    }

    QLabel* npu_label = new QLabel("NPU 使用率:");
    npu_label->setObjectName("statusHeader");
    npu_usage_bar_ = new QProgressBar(right_panel);
    npu_usage_bar_->setRange(0, 100);
    npu_usage_bar_->setValue(0);
    npu_usage_bar_->setMinimumHeight(20);

    QHBoxLayout* npu_row = new QHBoxLayout();
    npu_row->addWidget(npu_label);
    npu_row->addWidget(npu_usage_bar_, 1);
    status_grid->addLayout(npu_row, MAX_CHANNELS + 1, 0, 1, 3);

    model_name_label_ = new QLabel("--");
    model_name_label_->setObjectName("statusValue");
    model_status_label_ = new QLabel("未加载");
    model_status_label_->setObjectName("statusValue");

    status_grid->addWidget(new QLabel("模型:"), MAX_CHANNELS + 2, 0);
    status_grid->addWidget(model_name_label_, MAX_CHANNELS + 2, 1, 1, 2);
    status_grid->addWidget(new QLabel("状态:"), MAX_CHANNELS + 3, 0);
    status_grid->addWidget(model_status_label_, MAX_CHANNELS + 3, 1, 1, 2);

    right_layout->addWidget(status_group, 0);

    // 检测统计网格（可滚动，固定高度区域）
    QGroupBox* stats_group = new QGroupBox("检测结果", right_panel);
    stats_group->setMaximumHeight(210);
    QVBoxLayout* stats_vbox = new QVBoxLayout(stats_group);
    stats_vbox->setContentsMargins(10, 16, 10, 6);
    stats_vbox->setSpacing(0);

    QScrollArea* stats_scroll = new QScrollArea(stats_group);
    stats_scroll->setWidgetResizable(true);
    stats_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    stats_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    stats_scroll->setFrameShape(QFrame::NoFrame);
    stats_scroll->setStyleSheet("QScrollArea { background-color: transparent; border: none; }");

    QWidget* stats_container = new QWidget();
    stats_container->setStyleSheet("background-color: transparent;");
    stats_grid_ = new QGridLayout(stats_container);
    stats_grid_->setContentsMargins(0, 0, 0, 0);
    stats_grid_->setSpacing(4);

    stats_scroll->setWidget(stats_container);
    stats_vbox->addWidget(stats_scroll);

    right_layout->addWidget(stats_group, 0);

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

    right_layout->addWidget(log_group, 1);

    main_splitter->addWidget(right_panel);

    main_splitter->setStretchFactor(0, 2);
    main_splitter->setStretchFactor(1, 1);
    main_splitter->setSizes({1066, 534});

    QVBoxLayout* main_layout = new QVBoxLayout(central);
    main_layout->setContentsMargins(4, 4, 4, 4);
    main_layout->addWidget(main_splitter);

    status_label_ = new QLabel("就绪", this);
    statusBar()->addWidget(status_label_, 1);

    auto addSep = [this]() {
        QLabel* sep = new QLabel("|", this);
        sep->setStyleSheet("color: #555; font-weight: bold; margin: 0 4px;");
        statusBar()->addPermanentWidget(sep);
    };

    addSep();
    ws_clients_label_ = new QLabel("WS 客户端: 0", this);
    ws_clients_label_->setStyleSheet("color: #8a9bb0;");
    statusBar()->addPermanentWidget(ws_clients_label_);
    addSep();
    ws_status_label_ = new QLabel("WS 未启动", this);
    ws_status_label_->setStyleSheet("color: #8a9bb0;");
    statusBar()->addPermanentWidget(ws_status_label_);
    addSep();
    statusBar()->addPermanentWidget(new QLabel("RK3588 YOLO 四路检测", this));

    // 信号槽连接
    connect(model_btn_, &QPushButton::clicked, this, &MainWindow::onBrowseModel);
    connect(load_btn_, &QPushButton::clicked, this, &MainWindow::onLoadModel);
    connect(start_btn_, &QPushButton::clicked, this, &MainWindow::onStartDetection);
    connect(stop_btn_, &QPushButton::clicked, this, &MainWindow::onStopDetection);
    connect(pause_btn_, &QPushButton::clicked, this, &MainWindow::onPauseToggle);
    connect(step_btn_, &QPushButton::clicked, this, &MainWindow::onStepFrame);
    connect(screenshot_btn_, &QPushButton::clicked, this, &MainWindow::onScreenshot);
    connect(export_btn_, &QPushButton::clicked, this, &MainWindow::onExportResults);
    connect(record_btn_, &QPushButton::clicked, this, &MainWindow::onRecordToggle);
    connect(conf_slider_, &QSlider::valueChanged, this, &MainWindow::onConfThresholdChanged);
    connect(nms_slider_, &QSlider::valueChanged, this, &MainWindow::onNmsThresholdChanged);
    connect(clear_log_btn_, &QPushButton::clicked, this, &MainWindow::onClearLog);

    // 从标签文件加载所有类别名，初始化检测统计网格
    initStatsGrid();
}

// ============================================================
// 从标签文件加载类别名，填充3行检测统计网格
// ============================================================
void MainWindow::initStatsGrid() {
    if (!stats_grid_) return;

    // 读取标签文件
    AppConfig cfg = load_config();

    // 尝试多个可能的标签文件路径
    QStringList candidates;
    // 1. 从 config 解析的 label_path
    if (!cfg.label_path.empty())
        candidates << QString::fromStdString(cfg.label_path);
    // 2. 相对于当前工作目录
    candidates << QString::fromStdString(cfg.label_file);
    // 3. model/ 下
    candidates << QString("model/%1").arg(QString::fromStdString(cfg.label_file));
    // 4. install 目录下
    candidates << QString("../model/%1").arg(QString::fromStdString(cfg.label_file));

    QFile file;
    for (const QString& path : candidates) {
        qDebug() << "[StatsGrid] Trying:" << path << "exists:" << QFile::exists(path);
        file.setFileName(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "[StatsGrid] Opened:" << path;
            break;
        }
        file.setFileName("");
    }

    if (!file.isOpen()) {
        qDebug() << "[StatsGrid] FAILED: no label file found";
        return;
    }

    all_class_names_.clear();
    QTextStream ts(&file);
    while (!ts.atEnd()) {
        QString line = ts.readLine().trimmed();
        if (!line.isEmpty())
            all_class_names_.push_back(line.toStdString());
    }
    file.close();

    if (all_class_names_.empty()) return;

    // 每行4列，不限行数，上下滚动
    int total = (int)all_class_names_.size();
    int cols = 4;

    // 清除旧内容
    QLayoutItem* item;
    while ((item = stats_grid_->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    // 创建所有类别标签（灰色底色，检测到时高亮）
    for (int i = 0; i < total; i++) {
        QLabel* lbl = new QLabel();
        lbl->setText(QString::fromStdString(all_class_names_[i]) + ": 0");
        lbl->setStyleSheet(
            "background-color: #2c2c3e; color: #666; border-radius: 3px; "
            "padding: 2px 4px; font-size: 11px;");
        stats_class_labels_[all_class_names_[i]] = lbl;
        stats_grid_->addWidget(lbl, i / cols, i % cols);
    }

    LOG_INFO("[MainWindow]", "Stats grid: %d classes loaded", total);
}

// setupStyle() 实现已移至 mainwindow_style.cpp
