/**
 * @file main_win.cpp
 * @brief Windows 平台入口
 */

#include <QApplication>
#include "ui/mainwindow.hpp"
#include "core/runtime_config.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // 注册 Qt 元类型（跨线程信号槽）
    qRegisterMetaType<std::vector<Detection>>("std::vector<Detection>");

    // 初始化配置
    auto& cfg = RuntimeConfig::instance();
    auto result = cfg.init();
    if (result == RuntimeConfig::InitResult::Loaded) {
        qDebug() << "[Config] config.json loaded";
    } else if (result == RuntimeConfig::InitResult::Created) {
        qDebug() << "[Config] config.json created with defaults";
    }

    MainWindow window;
    window.show();

    return app.exec();
}
