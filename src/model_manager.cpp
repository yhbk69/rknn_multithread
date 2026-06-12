/**
 * @file model_manager.cpp
 * @brief 模型加载管理模块实现
 */

#include "model_manager.hpp"
#include "onnx_engine.hpp"
// TODO: RKNN 引擎头文件, 当前 rknn_inference_engine.hpp 存在编译问题
// (rknn_context 前向声明+0初始化, Preprocessor 为命名空间而非类)
// #include "rknn_inference_engine.hpp"
#include <QString>
#include <QFileInfo>

ModelManager::ModelManager()
    : engine_(nullptr)
{
}

bool ModelManager::load(const std::string& path) {
    try {
        std::unique_ptr<IEngine> eng;

        QString lower = QFileInfo(QString::fromStdString(path)).suffix().toLower();
        if (lower == "engine") {
            eng = std::make_unique<OnnxEngine>();
        } else if (lower == "rknn") {
            // eng = std::make_unique<OnnxEngine>();
            if (callbacks_.onError) {
                callbacks_.onError("RKNN 引擎暂未启用, 回退到 TensorRT");
            }
            eng = std::make_unique<OnnxEngine>();
        } else {
            if (callbacks_.log) {
                callbacks_.log("模型", QString("未知模型格式(%1), 默认使用 TensorRT 引擎").arg(lower));
            }
            eng = std::make_unique<OnnxEngine>();
        }

        eng->load(path);
        engine_ = std::move(eng);
        if (callbacks_.log) {
            callbacks_.log("模型", QString("模型加载成功: %1").arg(QString::fromStdString(path)));
        }
        return true;
    } catch (const std::exception& e) {
        engine_.reset();
        if (callbacks_.onError) {
            callbacks_.onError(QString::fromUtf8(e.what()));
        }
        return false;
    }
}

bool ModelManager::reload(const std::string& path) {
    if (!engine_) return false;
    try {
        engine_->reload(path);
        if (callbacks_.log) {
            callbacks_.log("模型", QString("模型热切换成功: %1").arg(QString::fromStdString(path)));
        }
        return true;
    } catch (const std::exception& e) {
        if (callbacks_.onError) {
            callbacks_.onError(QString::fromUtf8(e.what()));
        }
        return false;
    }
}
