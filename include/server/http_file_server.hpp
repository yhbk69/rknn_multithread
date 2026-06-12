/**
 * @file http_file_server.hpp
 * @brief HTTP 静态文件服务模块
 *
 * 为告警视频/截图提供下载服务。
 * 支持常见的媒体文件类型(MJPEG 流除外,见 MjpegStreamer)。
 *
 * 设计要点:
 * - 线程安全: 客户端连接处理在 lambda 中自动管理
 * - 路径安全: 防止路径穿越攻击,只允许访问指定目录下的文件
 * - 静态资源: 根据扩展名自动识别 MIME 类型
 * - 不继承 QObject: 避免 MOC 复杂化,用 lambda 回调代替信号槽
 */

#ifndef HTTP_FILE_SERVER_HPP
#define HTTP_FILE_SERVER_HPP

#include <QString>
#include <QByteArray>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMutex>
#include <QList>
#include <QMap>
#include <functional>

class HttpFileServer {
public:
    HttpFileServer();
    ~HttpFileServer();

    /**
     * @brief 初始化并启动 HTTP 文件服务器
     * @param port 监听端口(默认从 Config::HTTP_PORT 读取)
     * @param rootDir 要serve的根目录(默认从 Config::OUTPUT_DIR 读取)
     */
    void start(quint16 port = 0, const QString& rootDir = QString());

    /**
     * @brief 便捷初始化方法,使用 Config 默认值
     */
    void init();

    /**
     * @brief 停止服务
     */
    void stop();

    /**
     * @brief 服务是否正在运行
     */
    bool isRunning() const;

    /**
     * @brief 获取当前监听端口
     */
    quint16 port() const;

    /**
     * @brief 获取当前服务根目录
     */
    QString rootDir() const;

    /**
     * @brief 设置日志回调
     * @param callback(category, message)
     */
    void setLogCallback(std::function<void(const QString&, const QString&)> callback);

private:
    void onNewConnection();

    QObject* connCtx_ = nullptr;       // 信号槽上下文(自动管理连接生命周期)
    QTcpServer* server_ = nullptr;
    QString rootDir_;
    quint16 port_ = 0;
    mutable QMutex mutex_;
    std::function<void(const QString&, const QString&)> logCallback_;

    // 每个连接的HTTP请求头缓冲区 (处理TCP分片)
    QMap<QTcpSocket*, QByteArray> requestBuffers_;

    // MIME 类型映射
    static QString getMimeType(const QString& fileName);
};

#endif // HTTP_FILE_SERVER_HPP