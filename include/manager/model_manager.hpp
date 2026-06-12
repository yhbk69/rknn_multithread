/**
 * @file model_manager.hpp
 * @brief 模型加载管理模块
 *
 * 负责 TensorRT 引擎的创建、加载、热切换，
 * 通过回调通知调用方结果，自身不依赖 UI。
 */

#ifndef MODEL_MANAGER_HPP
#define MODEL_MANAGER_HPP

#include <memory>
#include <string>
#include <functional>
#include "inference_engine.hpp"

class ModelManager {
public:
    struct Callbacks {
        std::function<void(const QString& cat, const QString& msg)> log;
        std::function<void(const QString& msg)> onError;
    };

    ModelManager();
    ~ModelManager() = default;

    void setCallbacks(const Callbacks& cb) { callbacks_ = cb; }

    /**
     * @brief 加载模型
     * @param path 模型文件路径 (各后端对应 .engine / .rknn / .onnx 等)
     * @return 成功返回 true
     */
    bool load(const std::string& path);

    /**
     * @brief 热切换模型
     * @param path 新的模型文件路径
     * @return 成功返回 true
     */
    bool reload(const std::string& path);

    bool isLoaded() const { return engine_ != nullptr; }
    IEngine* engine() const { return engine_.get(); }
    IEngine* currentEngine() const { return engine_.get(); }  // alias for mainwindow

private:
    std::unique_ptr<IEngine> engine_;
    Callbacks callbacks_;
};

#endif // MODEL_MANAGER_HPP
