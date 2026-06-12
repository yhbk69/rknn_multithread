/**
 * @file logger.hpp
 * @brief 统一日志门面
 *
 * 同时分发到 GuiLogger (UI显示) 和 FileLogger (文件持久化)。
 * 使用方式:
 *   Logger::instance().setGuiCallback(gui_logger_callback);
 *   Logger::instance().initFileLogging("logs/");
 *   LOG_INFO("model") << "Model loaded successfully";
 */

#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <functional>
#include <sstream>
#include <mutex>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <ctime>

// 日志回调: (category, message, color_html)
using LogCallback = std::function<void(const std::string&, const std::string&, const std::string&)>;

enum class LogLevel { Debug, Info, Warning, Error };

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    /// 设置 GUI 日志回调 (在 MainWindow 初始化时调用)
    void setGuiCallback(LogCallback cb) {
        std::lock_guard<std::mutex> lk(mu_);
        guiCallback_ = std::move(cb);
    }

    /// 启用文件日志 (按日期滚动)
    void initFileLogging(const std::string& logDir) {
        std::lock_guard<std::mutex> lk(mu_);
        logDir_ = logDir;
        fileEnabled_ = true;
        openLogFile();
    }

    /// 关闭文件日志
    void closeFileLogging() {
        std::lock_guard<std::mutex> lk(mu_);
        if (logFile_.is_open()) logFile_.close();
        fileEnabled_ = false;
    }

    /// 通用日志接口 (category 用于颜色分类)
    void log(const std::string& category, const std::string& message,
             LogLevel level = LogLevel::Info) {
        std::lock_guard<std::mutex> lk(mu_);

        std::string color = getColorForCategory(category, level);
        std::string timestamp = getTimestamp();
        std::string fullMsg = "[" + timestamp + "] [" + category + "] " + message;

        // GUI 回调
        if (guiCallback_) {
            guiCallback_(category, message, color);
        }

        // 文件日志
        if (fileEnabled_) {
            if (!logFile_.is_open()) openLogFile();
            if (logFile_.is_open()) {
                logFile_ << fullMsg << std::endl;
            }
        }
    }

    // 便捷方法
    void info(const std::string& cat, const std::string& msg)  { log(cat, msg, LogLevel::Info); }
    void warn(const std::string& cat, const std::string& msg)  { log(cat, msg, LogLevel::Warning); }
    void error(const std::string& cat, const std::string& msg) { log(cat, msg, LogLevel::Error); }
    void debug(const std::string& cat, const std::string& msg) { log(cat, msg, LogLevel::Debug); }

private:
    Logger() = default;

    std::string getTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) % 1000;
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&t), "%H:%M:%S")
            << "." << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    std::string getColorForCategory(const std::string& cat, LogLevel level) {
        if (level == LogLevel::Error)  return "#e74c3c";  // 红色
        if (level == LogLevel::Warning) return "#e67e22"; // 橙色
        // 按类别
        if (cat.find("alert") != std::string::npos || cat.find("报警") != std::string::npos)
            return "#e74c3c";
        if (cat.find("model") != std::string::npos || cat.find("推理") != std::string::npos)
            return "#27ae60";
        if (cat.find("record") != std::string::npos || cat.find("录像") != std::string::npos)
            return "#e67e22";
        if (cat.find("ws") != std::string::npos || cat.find("mjpeg") != std::string::npos)
            return "#3498db";
        if (cat.find("config") != std::string::npos || cat.find("配置") != std::string::npos)
            return "#9b59b6";
        if (cat.find("system") != std::string::npos || cat.find("系统") != std::string::npos)
            return "#34495e";
        return "#bdc3c7"; // 默认灰色
    }

    void openLogFile() {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream fname;
        fname << logDir_ << "/log_"
              << std::put_time(std::localtime(&t), "%Y%m%d") << ".txt";
        logFile_.open(fname.str(), std::ios::app);
    }

    std::mutex mu_;
    LogCallback guiCallback_;
    bool fileEnabled_ = false;
    std::string logDir_;
    std::ofstream logFile_;
};

// 流式日志宏
#define LOG_INFO(cat)  Logger::instance().log((cat), "", LogLevel::Info); LogStream(Logger::instance(), (cat), LogLevel::Info)
#define LOG_WARN(cat)  Logger::instance().log((cat), "", LogLevel::Warning); LogStream(Logger::instance(), (cat), LogLevel::Warning)
#define LOG_ERROR(cat) Logger::instance().log((cat), "", LogLevel::Error); LogStream(Logger::instance(), (cat), LogLevel::Error)
#define LOG_DEBUG(cat) Logger::instance().log((cat), "", LogLevel::Debug); LogStream(Logger::instance(), (cat), LogLevel::Debug)

class LogStream {
public:
    LogStream(Logger& logger, const std::string& cat, LogLevel lvl)
        : logger_(logger), cat_(cat), lvl_(lvl) {}
    ~LogStream() {
        // 析构时实际写入
        logger_.log(cat_, ss_.str(), lvl_);
    }
    template<typename T>
    LogStream& operator<<(const T& val) { ss_ << val; return *this; }
private:
    Logger& logger_;
    std::string cat_;
    LogLevel lvl_;
    std::ostringstream ss_;
};

#endif // LOGGER_HPP
