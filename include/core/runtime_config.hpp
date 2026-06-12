/**
 * @file runtime_config.hpp
 * @brief 运行时可变配置 — 线程安全单例
 *
 * 所有运行时可调参数的默认值仅在此处定义，SettingsDialog 从此处读取。
 *
 * 新增参数时:
 *   1. 添加成员变量 + getter/setter
 *   2. 在 toJson()/applyJson() 中添加序列化
 *   3. 在 Config 命名空间添加同名 constexpr
 *   4. 在 SettingsDialog 中添加 UI 控件
 */

#ifndef RUNTIME_CONFIG_HPP
#define RUNTIME_CONFIG_HPP

#include <QString>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <mutex>
#include <string>
#include <vector>

// 记忆的摄像头条目
struct CameraEntry {
    QString source;       // 设备ID(e.g. "0") 或 RTSP URL
    QString alias;        // 用户自定义别名
    float confThreshold = 0.25f;
    float nmsThreshold  = 0.45f;

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["source"]         = source;
        obj["alias"]          = alias;
        obj["conf_threshold"] = confThreshold;
        obj["nms_threshold"]  = nmsThreshold;
        return obj;
    }

    static CameraEntry fromJson(const QJsonObject& obj) {
        CameraEntry e;
        e.source         = obj["source"].toString();
        e.alias          = obj["alias"].toString();
        e.confThreshold  = static_cast<float>(obj["conf_threshold"].toDouble(0.25));
        e.nmsThreshold   = static_cast<float>(obj["nms_threshold"].toDouble(0.45));
        return e;
    }

    QString displayName() const { return alias.isEmpty() ? source : alias; }
};

class RuntimeConfig {
public:
    static RuntimeConfig& instance() {
        static RuntimeConfig inst;
        return inst;
    }

    // ---- 阈值 ----
    float confThreshold() const       { std::lock_guard<std::mutex> lk(mu_); return confThreshold_; }
    void  setConfThreshold(float v)    { std::lock_guard<std::mutex> lk(mu_); confThreshold_ = v; }

    float iouThreshold() const        { std::lock_guard<std::mutex> lk(mu_); return iouThreshold_; }
    void  setIouThreshold(float v)     { std::lock_guard<std::mutex> lk(mu_); iouThreshold_ = v; }

    // ---- 网络端口 ----
    int websocketPort() const         { std::lock_guard<std::mutex> lk(mu_); return websocketPort_; }
    void setWebsocketPort(int v)      { std::lock_guard<std::mutex> lk(mu_); websocketPort_ = v; }

    int alertWsPort() const           { std::lock_guard<std::mutex> lk(mu_); return alertWsPort_; }
    void setAlertWsPort(int v)        { std::lock_guard<std::mutex> lk(mu_); alertWsPort_ = v; }

    int streamPort() const            { std::lock_guard<std::mutex> lk(mu_); return streamPort_; }
    void setStreamPort(int v)         { std::lock_guard<std::mutex> lk(mu_); streamPort_ = v; }

    int httpPort() const              { std::lock_guard<std::mutex> lk(mu_); return httpPort_; }
    void setHttpPort(int v)           { std::lock_guard<std::mutex> lk(mu_); httpPort_ = v; }

    // ---- 报警参数 ----
    int ackTimeoutMs() const          { std::lock_guard<std::mutex> lk(mu_); return ackTimeoutMs_; }
    void setAckTimeoutMs(int v)       { std::lock_guard<std::mutex> lk(mu_); ackTimeoutMs_ = v; }

    int ringBufferFrames() const      { std::lock_guard<std::mutex> lk(mu_); return ringBufferFrames_; }
    void setRingBufferFrames(int v)   { std::lock_guard<std::mutex> lk(mu_); ringBufferFrames_ = v; }

    int alertAfterFrames() const      { std::lock_guard<std::mutex> lk(mu_); return alertAfterFrames_; }
    void setAlertAfterFrames(int v)   { std::lock_guard<std::mutex> lk(mu_); alertAfterFrames_ = v; }

    int alertCooldownMs() const       { std::lock_guard<std::mutex> lk(mu_); return alertCooldownMs_; }
    void setAlertCooldownMs(int v)    { std::lock_guard<std::mutex> lk(mu_); alertCooldownMs_ = v; }

    // ---- 模型路径 ----
    QString modelPath() const         { std::lock_guard<std::mutex> lk(mu_); return modelPath_; }
    void setModelPath(const QString& v) { std::lock_guard<std::mutex> lk(mu_); modelPath_ = v; }

    QString labelFile() const         { std::lock_guard<std::mutex> lk(mu_); return labelFile_; }
    void setLabelFile(const QString& v) { std::lock_guard<std::mutex> lk(mu_); labelFile_ = v; }

    // ---- 路径 ----
    QString outputDir() const         { std::lock_guard<std::mutex> lk(mu_); return outputDir_; }
    void setOutputDir(const QString& v) { std::lock_guard<std::mutex> lk(mu_); outputDir_ = v; }

    QString recordDir() const         { std::lock_guard<std::mutex> lk(mu_); return recordDir_; }
    void setRecordDir(const QString& v) { std::lock_guard<std::mutex> lk(mu_); recordDir_ = v; }

    int recordKeepDays() const        { std::lock_guard<std::mutex> lk(mu_); return recordKeepDays_; }
    void setRecordKeepDays(int v)     { std::lock_guard<std::mutex> lk(mu_); recordKeepDays_ = v; }

    // ---- 摄像头列表 ----
    std::vector<CameraEntry> cameras() const {
        std::lock_guard<std::mutex> lk(mu_); return cameras_;
    }
    void setCameras(const std::vector<CameraEntry>& v) {
        std::lock_guard<std::mutex> lk(mu_); cameras_ = v;
    }
    void addCamera(const CameraEntry& cam) {
        std::lock_guard<std::mutex> lk(mu_); cameras_.push_back(cam);
    }

    // ---- 类别配置 ----
    std::vector<QString> classNames() const {
        std::lock_guard<std::mutex> lk(mu_); return classNames_;
    }
    void setClassNames(const std::vector<QString>& v) {
        std::lock_guard<std::mutex> lk(mu_); classNames_ = v;
    }

    // ---- Qt 平台 ----
    QString qtPlatform() const        { std::lock_guard<std::mutex> lk(mu_); return qtPlatform_; }
    void setQtPlatform(const QString& v) { std::lock_guard<std::mutex> lk(mu_); qtPlatform_ = v; }

    // ---- 平台检测 ----
#ifdef _WIN32
    bool isWindows() const { return true; }
    bool isRK3588()  const { return false; }
#else
    bool isWindows() const { return false; }
    bool isRK3588()  const { return true; }
#endif

    // ---- 文件 I/O ----
    enum class InitResult { Loaded, Created, Failed };

    InitResult init() {
        configFilePath_ = discoverConfigPath();
        if (QFile::exists(configFilePath_)) {
            return loadFromFile(configFilePath_) ? InitResult::Loaded : InitResult::Failed;
        }
        return saveToFile(configFilePath_) ? InitResult::Created : InitResult::Failed;
    }

    bool loadFromFile(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "[RuntimeConfig] Cannot open:" << path;
            return false;
        }
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
        file.close();
        if (err.error != QJsonParseError::NoError) {
            qWarning() << "[RuntimeConfig] Parse error:" << err.errorString();
            return false;
        }
        return applyJson(doc.object());
    }

    bool saveToFile(const QString& path) const {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
        file.close();
        return true;
    }

    QJsonObject toJson() const {
        std::lock_guard<std::mutex> lk(mu_);
        QJsonObject root;
        root["config_version"]     = 2;
        root["conf_threshold"]     = confThreshold_;
        root["iou_threshold"]      = iouThreshold_;
        root["websocket_port"]     = websocketPort_;
        root["alert_ws_port"]      = alertWsPort_;
        root["stream_port"]        = streamPort_;
        root["http_port"]          = httpPort_;
        root["ack_timeout_ms"]     = ackTimeoutMs_;
        root["ring_buffer_frames"] = ringBufferFrames_;
        root["alert_after_frames"] = alertAfterFrames_;
        root["alert_cooldown_ms"]  = alertCooldownMs_;
        root["model_path"]         = modelPath_;
        root["label_file"]         = labelFile_;
        root["output_dir"]         = outputDir_;
        root["record_dir"]         = recordDir_;
        root["record_keep_days"]   = recordKeepDays_;
        root["qt_platform"]        = qtPlatform_;

        QJsonArray camArr;
        for (const auto& c : cameras_)
            camArr.append(c.toJson());
        root["cameras"] = camArr;

        QJsonArray clsArr;
        for (const auto& c : classNames_)
            clsArr.append(c);
        root["class_names"] = clsArr;

        return root;
    }

    QString configFilePath() const { return configFilePath_; }

private:
    RuntimeConfig() = default;

    // 自动发现 config.json: 优先可执行文件同目录 → 当前目录 → 默认路径
    static QString discoverConfigPath() {
        return QString("config.json"); // 简化：默认当前目录
    }

    bool applyJson(const QJsonObject& root) {
        std::lock_guard<std::mutex> lk(mu_);
        if (root.contains("conf_threshold"))     confThreshold_     = static_cast<float>(root["conf_threshold"].toDouble(confThreshold_));
        if (root.contains("iou_threshold"))      iouThreshold_      = static_cast<float>(root["iou_threshold"].toDouble(iouThreshold_));
        if (root.contains("websocket_port"))     websocketPort_     = root["websocket_port"].toInt(websocketPort_);
        if (root.contains("alert_ws_port"))      alertWsPort_       = root["alert_ws_port"].toInt(alertWsPort_);
        if (root.contains("stream_port"))        streamPort_        = root["stream_port"].toInt(streamPort_);
        if (root.contains("http_port"))          httpPort_          = root["http_port"].toInt(httpPort_);
        if (root.contains("ack_timeout_ms"))     ackTimeoutMs_      = root["ack_timeout_ms"].toInt(ackTimeoutMs_);
        if (root.contains("ring_buffer_frames")) ringBufferFrames_  = root["ring_buffer_frames"].toInt(ringBufferFrames_);
        if (root.contains("alert_after_frames")) alertAfterFrames_  = root["alert_after_frames"].toInt(alertAfterFrames_);
        if (root.contains("alert_cooldown_ms"))  alertCooldownMs_   = root["alert_cooldown_ms"].toInt(alertCooldownMs_);
        if (root.contains("model_path"))         modelPath_         = root["model_path"].toString(modelPath_);
        if (root.contains("label_file"))         labelFile_         = root["label_file"].toString(labelFile_);
        if (root.contains("output_dir"))         outputDir_         = root["output_dir"].toString(outputDir_);
        if (root.contains("record_dir"))         recordDir_         = root["record_dir"].toString(recordDir_);
        if (root.contains("record_keep_days"))   recordKeepDays_    = root["record_keep_days"].toInt(recordKeepDays_);
        if (root.contains("qt_platform"))        qtPlatform_        = root["qt_platform"].toString(qtPlatform_);

        if (root.contains("cameras")) {
            cameras_.clear();
            QJsonArray arr = root["cameras"].toArray();
            for (const auto& v : arr) {
                if (v.isString()) {
                    CameraEntry e;  e.source = v.toString();
                    cameras_.push_back(std::move(e));
                } else {
                    cameras_.push_back(CameraEntry::fromJson(v.toObject()));
                }
            }
        }

        if (root.contains("class_names")) {
            classNames_.clear();
            QJsonArray arr = root["class_names"].toArray();
            for (const auto& v : arr)
                classNames_.push_back(v.toString());
        }

        return true;
    }

    mutable std::mutex mu_;

    float confThreshold_     = 0.25f;
    float iouThreshold_      = 0.45f;
    int   websocketPort_     = 9090;
    int   alertWsPort_       = 9091;
    int   streamPort_        = 9092;
    int   httpPort_          = 9093;
    int   ackTimeoutMs_      = 5000;
    int   ringBufferFrames_  = 90;
    int   alertAfterFrames_  = 60;
    int   alertCooldownMs_   = 10000;
    QString modelPath_       = "model/yolo11n.onnx";
    QString labelFile_       = "model/coco_80_labels_list.txt";
    QString outputDir_       = "output";
    QString recordDir_       = "recordings";
    int    recordKeepDays_   = 7;
    QString qtPlatform_      = "windows";
    std::vector<CameraEntry> cameras_;
    std::vector<QString>    classNames_;
    QString configFilePath_  = "config.json";
};

#endif // RUNTIME_CONFIG_HPP
