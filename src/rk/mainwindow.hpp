/*
 * mainwindow.hpp - RK3588 Qt GUI 主窗口
 * 四路视频 2x2 网格布局，支持单路放大/缩小
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
#include <QGridLayout>
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
#include <QTimer>
#include <QNetworkInterface>
#include <QAbstractSocket>
#include <QFileSystemWatcher>
#include <atomic>
#include <cstdio>
#include <memory>
#include <mutex>
#include <vector>
#include <array>
#include <map>
#include <string>
#include <chrono>

#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>

#include "config_loader.hpp"
#include "websocket.hpp"
#include "pipeline/FrameDetections.hpp"
#include "pipeline/FrameQueue.hpp"
#include "pipeline/DetectThread.hpp"
#include "pipeline/VideoCell.hpp"

class VideoRecorder;

static constexpr int MAX_CHANNELS = 4;

// ============================================================
// MainWindow: RK3588 主窗口 (四路 2x2 网格)
// ============================================================
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onBrowseModel();
    void onLoadModel();
    void onBrowseVideo(int ch);
    void onStartDetection();
    void onStopDetection();
    /* 帧队列轮询槽（QTimer 驱动，GUI 线程执行 BGR→RGB→QImage） */
    void onPollFrames();
    void onStatsUpdated(int ch, int frames, double fps, double inferTime);
    void onDetectFinished(int ch);
    void onDetectError(int ch, const QString& msg);
    void onConfThresholdChanged(int value);
    void onNmsThresholdChanged(int value);
    void onClearLog();
    void onPauseToggle();
    void onStepFrame();
    void onScreenshot();
    void onExportResults();
    /* P1-3: 接收批量检测结果（每帧一次） */
    void onDetectionBatch(int ch, int frameId, const QVector<FrameDetections::Det>& dets);

    // 缩放
    void onZoomIn(int ch);
    void onZoomOut();

    // WebSocket 相关
    void onWebSocketStarted(quint16 port);
    void onWebSocketClientConnected(QWebSocket *client);
    void onWebSocketClientDisconnected(QWebSocket *client);
    void onWebSocketAlarm(const QString &alarmId, const QString &alarmType,
                          int frameId, long long timestampMs);

    // 统计面板更新
    void onStatsPanelUpdated(int ch, long long totalAlarms, const QString& classStatsJson);

    // 配置热更新
    void onConfigFileChanged(const QString &path);
    void onRecordToggle();  // 录制按钮槽
    void onRoiToggle();     // 围栏模式槽

private:
    void log(const QString& category, const QString& message);
    void logWithColor(const QString& category, const QString& message, const QColor& color);
    QString currentTimestamp();
    void updateThresholdLabels();
    void enableControls(bool enabled);
    void setupUI();
    void setupStyle();
    QString formatSize(qint64 bytes);
    void saveSettings();
    void restoreSettings();
    void updateZoomState();
    void syncAlarmScreenshotDirToConfig(const std::string &dir);
    void initStatsGrid();

    // --- 视频显示网格 ---
    VideoCell video_cells_[MAX_CHANNELS];
    QGridLayout* grid_layout_ = nullptr;
    int expanded_ch_ = -1;  // -1 = 2x2 网格, 0-3 = 放大的通道

    // --- 控制区 ---
    QLineEdit* model_edit_;
    QLineEdit* video_edits_[MAX_CHANNELS];
    QLineEdit* video_alias_edits_[MAX_CHANNELS];
    QPushButton* model_btn_;
    QPushButton* video_btns_[MAX_CHANNELS];
    QPushButton* video_del_btns_[MAX_CHANNELS];
    QPushButton* load_btn_;
    QPushButton* start_btn_;
    QPushButton* stop_btn_;
    QPushButton* pause_btn_;
    QPushButton* step_btn_;
    QPushButton* screenshot_btn_;
    QPushButton* export_btn_;
    QPushButton* record_btn_;       // 录制按钮
    QPushButton* roi_btn_;          // 围栏按钮
    QSpinBox* thread_spin_;
    QSlider* conf_slider_;
    QSlider* nms_slider_;
    QLabel* conf_value_label_;
    QLabel* nms_value_label_;
    QLabel* model_status_left_label_;  // 控制区模型状态
    QLineEdit* alarm_screenshot_edit_;
    QPushButton* alarm_screenshot_btn_;

    // --- 右侧面板 ---
    QLabel* model_status_label_;
    QLabel* model_name_label_;
    QLabel* fps_labels_[MAX_CHANNELS];
    QLabel* frames_labels_[MAX_CHANNELS];
    QLabel* infer_time_labels_[MAX_CHANNELS];
    QProgressBar* npu_usage_bar_;

    // 日志
    QTextEdit* log_edit_;
    QPushButton* clear_log_btn_;

    // 状态栏
    QLabel* status_label_;

    // WebSocket 状态
    QLabel* ws_status_label_;
    QLabel* ws_clients_label_;

    // 检测统计网格
    std::map<std::string, QLabel*> stats_class_labels_;  // 类别 -> 标签映射
    QLabel* stats_alarm_label_ = nullptr;                 // 报警总数标签
    QGridLayout* stats_grid_ = nullptr;                   // 3行网格布局
    std::vector<std::string> all_class_names_;            // 所有类别名（从标签文件读取）

    // 逻辑
    DetectThread* detect_threads_[MAX_CHANNELS] = {};
    std::shared_ptr<WebSocket> ws_server_;
    std::unique_ptr<QFileSystemWatcher> config_watcher_;
    std::unique_ptr<QTimer> frame_poll_timer_;  /* 帧队列轮询定时器 */
    FrameQueue frame_queues_[MAX_CHANNELS];     /* 每通道一个共享帧队列 */
    float conf_threshold_ = 0.25f;
    float nms_threshold_ = 0.45f;
    std::string model_path_;
    QImage last_frames_[MAX_CHANNELS];
    long long last_stats_update_[MAX_CHANNELS] = {};  // 各通道上次统计更新时间（ms）
    std::shared_ptr<VideoRecorder> recorders_[MAX_CHANNELS];  // 各通道录制器
    bool roi_mode_ = false;  // 围栏绘制模式

    // 检测结果历史（用于导出）
    struct ExportRecord {
        int channel;
        int frame_id;
        std::string class_name;
        float confidence;
        int left, top, right, bottom;
    };
    std::vector<ExportRecord> export_records_;
};

#endif // RK_MAINWINDOW_HPP
