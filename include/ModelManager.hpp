/*
 * ModelManager.hpp - 模型热切换管理器
 *
 * 功能：
 *   - 管理多个预加载的模型
 *   - 运行时切换当前活跃模型
 *   - 线程安全的模型切换
 *   - WebSocket 协议支持
 *
 * 协议：
 *   请求: {"type":"switch_model","model":"yolo11n"}
 *   响应: {"type":"model_switched","model":"yolo11n","success":true}
 *   请求: {"type":"get_models"}
 *   响应: {"type":"models_list","data":["yolov5s","yolo11n"]}
 */

#ifndef MODEL_MANAGER_HPP
#define MODEL_MANAGER_HPP

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>

#include "core/IEngine.hpp"
#include "config_loader.hpp"

class ModelManager {
public:
    using ModelFactory = std::function<std::shared_ptr<IEngine>(const std::string& path)>;

    ModelManager() = default;
    ~ModelManager() = default;

    /* 注册模型工厂（用于创建新模型实例） */
    void registerFactory(const std::string& type, ModelFactory factory) {
        std::lock_guard<std::mutex> lock(mtx_);
        factories_[type] = factory;
    }

    /* 预加载模型到内存 */
    int preloadModel(const std::string& name, const std::string& type,
                     const std::string& path) {
        std::lock_guard<std::mutex> lock(mtx_);

        auto it = factories_.find(type);
        if (it == factories_.end()) {
            LOG_ERROR("[ModelManager]", "Unknown model type: %s", type.c_str());
            return -1;
        }

        auto model = it->second(path);
        if (!model) {
            LOG_ERROR("[ModelManager]", "Failed to create model: %s", name.c_str());
            return -1;
        }

        if (model->init() != 0) {
            LOG_ERROR("[ModelManager]", "Failed to init model: %s", name.c_str());
            return -1;
        }

        ModelInfo info;
        info.name = name;
        info.type = type;
        info.path = path;
        info.engine = model;

        models_[name] = info;
        LOG_INFO("[ModelManager]", "Preloaded model: %s (%s)", name.c_str(), type.c_str());
        return 0;
    }

    /* 获取模型 */
    std::shared_ptr<IEngine> getModel(const std::string& name) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = models_.find(name);
        if (it == models_.end()) return nullptr;
        return it->second.engine;
    }

    /* 获取当前活跃模型名称 */
    std::string getActiveModelName() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return active_model_name_;
    }

    /* 获取当前活跃模型 */
    std::shared_ptr<IEngine> getActiveModel() {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = models_.find(active_model_name_);
        if (it == models_.end()) return nullptr;
        return it->second.engine;
    }

    /* 切换活跃模型 */
    bool switchModel(const std::string& name) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = models_.find(name);
        if (it == models_.end()) {
            LOG_ERROR("[ModelManager]", "Model not found: %s", name.c_str());
            return false;
        }

        std::string old_name = active_model_name_;
        active_model_name_ = name;
        LOG_INFO("[ModelManager]", "Switched model: %s -> %s",
                old_name.c_str(), name.c_str());
        return true;
    }

    /* 获取所有已加载模型列表 */
    std::vector<std::string> getModelNames() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<std::string> names;
        for (const auto& [name, _] : models_) {
            names.push_back(name);
        }
        return names;
    }

    /* 获取模型信息 */
    struct ModelInfo {
        std::string name;
        std::string type;
        std::string path;
        std::shared_ptr<IEngine> engine;
    };

    std::map<std::string, ModelInfo> getModelInfo() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return models_;
    }

    /* 生成 JSON 列表 */
    nlohmann::json toJson() const {
        std::lock_guard<std::mutex> lock(mtx_);
        nlohmann::json j;
        j["active_model"] = active_model_name_;
        j["models"] = nlohmann::json::array();
        for (const auto& [name, info] : models_) {
            j["models"].push_back({
                {"name", name},
                {"type", info.type},
                {"path", info.path}
            });
        }
        return j;
    }

private:
    mutable std::mutex mtx_;
    std::map<std::string, ModelFactory> factories_;
    std::map<std::string, ModelInfo> models_;
    std::string active_model_name_;
};

#endif // MODEL_MANAGER_HPP
