/*
 * Logger.hpp - 彩色日志系统
 *
 * 支持多种日志级别和颜色输出：
 *   - DEBUG: 灰色
 *   - INFO:  绿色
 *   - WARN:  黄色
 *   - ERROR: 红色
 *   - ALARM: 红色加粗（报警专用）
 *
 * 使用示例:
 *   LOG_INFO("[Module] Message: %s", value);
 *   LOG_ERROR("[Module] Error: %d", ret);
 *   LOG_ALARM("[WebSocket] ALARM: %s", type);
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <cstdio>
#include <cstdarg>

// ANSI 颜色代码
namespace LogColor {
    constexpr const char* RESET   = "\033[0m";
    constexpr const char* RED     = "\033[31m";
    constexpr const char* GREEN   = "\033[32m";
    constexpr const char* YELLOW  = "\033[33m";
    constexpr const char* BLUE    = "\033[34m";
    constexpr const char* MAGENTA = "\033[35m";
    constexpr const char* CYAN    = "\033[36m";
    constexpr const char* WHITE   = "\033[37m";
    constexpr const char* GRAY    = "\033[90m";

    // 加粗样式
    constexpr const char* BOLD_RED   = "\033[1;31m";
    constexpr const char* BOLD_GREEN = "\033[1;32m";
    constexpr const char* BOLD_YELLOW = "\033[1;33m";
}

// 日志级别
enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR,
    ALARM
};

// 全局日志级别控制
inline LogLevel& getGlobalLogLevel() {
    static LogLevel level = LogLevel::INFO;
    return level;
}

inline void setLogLevel(LogLevel level) {
    getGlobalLogLevel() = level;
}

// 内部日志函数
inline void logMessage(LogLevel level, const char* tag, const char* fmt, ...) {
    // 检查日志级别
    if (level < getGlobalLogLevel()) return;

    const char* color;
    const char* levelStr;

    switch (level) {
        case LogLevel::DEBUG:
            color = LogColor::GRAY;
            levelStr = "DEBUG";
            break;
        case LogLevel::INFO:
            color = LogColor::GREEN;
            levelStr = "INFO ";
            break;
        case LogLevel::WARN:
            color = LogColor::YELLOW;
            levelStr = "WARN ";
            break;
        case LogLevel::ERROR:
            color = LogColor::RED;
            levelStr = "ERROR";
            break;
        case LogLevel::ALARM:
            color = LogColor::BOLD_RED;
            levelStr = "ALARM";
            break;
        default:
            color = LogColor::WHITE;
            levelStr = "?????";
            break;
    }

    // 输出带颜色的日志
    fprintf(stderr, "%s[%s]%s %s", color, levelStr, LogColor::RESET, tag);

    // 格式化消息
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}

// 便捷宏
#define LOG_DEBUG(tag, fmt, ...) logMessage(LogLevel::DEBUG, tag, fmt, ##__VA_ARGS__)
#define LOG_INFO(tag, fmt, ...)  logMessage(LogLevel::INFO, tag, fmt, ##__VA_ARGS__)
#define LOG_WARN(tag, fmt, ...)  logMessage(LogLevel::WARN, tag, fmt, ##__VA_ARGS__)
#define LOG_ERROR(tag, fmt, ...) logMessage(LogLevel::ERROR, tag, fmt, ##__VA_ARGS__)
#define LOG_ALARM(tag, fmt, ...) logMessage(LogLevel::ALARM, tag, fmt, ##__VA_ARGS__)

#endif // LOGGER_H
