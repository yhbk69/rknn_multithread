/**
 * @file camera_manager.hpp
 * @brief 摄像头生命周期管理模块
 *
 * 负责管理摄像头工作线程的创建、启动、停止、清理，
 * 以及批量推理开关等跨摄像头设置。
 */

#ifndef CAMERA_MANAGER_HPP
#define CAMERA_MANAGER_HPP

#include <QThread>
#include <QMap>
#include <QList>
#include <functional>
#include "inference_worker.hpp"

class CameraManager {
public:
    struct Entry {
        QThread* thread = nullptr;
        InferenceWorker* worker = nullptr;
    };

    struct Callbacks {
        std::function<void(const QString& cat, const QString& msg)> log;
    };

    CameraManager();
    ~CameraManager() = default;

    void setCallbacks(const Callbacks& cb) { callbacks_ = cb; }

    // --- 摄像头注册/移除 ---

    void add(int cameraId, QThread* thread, InferenceWorker* worker);
    InferenceWorker* worker(int cameraId) const;
    QThread* thread(int cameraId) const { auto it=entries_.find(cameraId); return it!=entries_.end()?it->thread:nullptr; }
    bool contains(int cameraId) const;
    int count() const { return entries_.size(); }
    bool isEmpty() const { return entries_.isEmpty(); }
    QList<int> cameraIds() const;

    // --- 生命周期控制 ---

    void stop(int cameraId);
    void stopAll();

    // --- ID 管理 ---

    int allocateId() { return nextCameraId_++; }
    int nextId() const { return nextCameraId_; }

    // --- 批量设置 ---

    void setBatchInference(bool enabled);

    // private: // made public for mainwindow
    QMap<int, Entry> entries_;
    int nextCameraId_ = 1;
    Callbacks callbacks_;
};

#endif // CAMERA_MANAGER_HPP
