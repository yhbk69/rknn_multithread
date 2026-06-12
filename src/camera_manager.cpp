/**
 * @file camera_manager.cpp
 * @brief 摄像头生命周期管理模块实现
 */

#include "camera_manager.hpp"
#include <QDebug>

 // 构造函数：初始化 CameraManager 实例
CameraManager::CameraManager() {
    // 成员变量 entries_ 会在初始化列表中或声明时默认构造
    // 此处无需额外操作
}

/**
 * @brief 添加一个新的相机实例到管理器中
 * @param cameraId 相机的唯一标识符
 * @param thread 负责该相机采集或处理的线程对象指针
 * @param worker 负责该相机推理任务的工作者对象指针
 */
void CameraManager::add(int cameraId, QThread* thread, InferenceWorker* worker) {
    // 空指针检查：确保线程和工作对象有效，防止后续崩溃
    if (!thread || !worker) return;

    // 将线程和工作对象打包存入映射表，键为相机ID
    // 如果该ID已存在，这会覆盖旧的条目
    entries_[cameraId] = { thread, worker };
}

/**
 * @brief 获取指定相机ID关联的推理工作者
 * @param cameraId 相机ID
 * @return 返回 InferenceWorker 指针，如果未找到则返回 nullptr
 */
InferenceWorker* CameraManager::worker(int cameraId) const {
    // 在映射表中查找对应的相机ID
    auto it = entries_.find(cameraId);
    // 如果找到则返回 worker 指针，否则返回空指针
    return it != entries_.end() ? it->worker : nullptr;
}

/**
 * @brief 检查指定的相机ID是否已存在于管理器中
 * @param cameraId 相机ID
 * @return 存在返回 true，否则返回 false
 */
bool CameraManager::contains(int cameraId) const {
    // 利用 QMap/QHash 的 contains 方法快速查找
    return entries_.contains(cameraId);
}

/**
 * @brief 获取当前所有已注册相机的ID列表
 * @return 包含所有相机ID的列表
 */
QList<int> CameraManager::cameraIds() const {
    // 返回映射表中所有的键（即相机ID）
    return entries_.keys();
}

/**
 * @brief 停止并移除指定的相机实例
 * @param cameraId 需要停止的相机ID
 *
 * @note 包含线程安全停止逻辑：先尝试软停止，超时则强制终止
 */
void CameraManager::stop(int cameraId) {
    auto it = entries_.find(cameraId);
    if (it == entries_.end()) return;

    // 设置停止标志, 不等待线程退出
    // 工作线程会在下一轮循环检测到 running_ = false 后自然退出
    if (it->worker) it->worker->stop();

    // 立即从管理器移除, 调用方更新 UI
    entries_.erase(it);
}

/**
 * @brief 停止并清理所有已注册的相机
 */
void CameraManager::stopAll() {
    // 获取当前所有相机ID的副本列表
    // 注意：这里先复制keys是为了避免在遍历过程中直接修改容器导致迭代器失效
    auto ids = entries_.keys();

    // 遍历并逐个停止
    for (int id : ids) {
        stop(id);
    }
}

/**
 * @brief 批量设置所有相机的推理模式（是否启用批处理）
 * @param enabled true 为启用批处理，false 为关闭
 */
void CameraManager::setBatchInference(bool enabled) {
    // 遍历所有注册的相机条目
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        // 如果 worker 存在，则应用设置
        if (it->worker) it->worker->setBatchInference(enabled);
    }
}