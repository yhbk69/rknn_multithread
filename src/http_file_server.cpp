/**
 * @file http_file_server.cpp
 * @brief HTTP 静态文件服务模块实现
 */

#include "http_file_server.hpp"
#include "core/config.hpp"
#include <QObject>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QHostAddress>
#include <QPointer>
#include <map>

HttpFileServer::HttpFileServer()
    : connCtx_(new QObject())  // 用于信号槽上下文,析构时自动清理
{
}

HttpFileServer::~HttpFileServer() {
    // 先关闭并清理 server_, 但不触发 deleteLater (直接删除)
    if (server_) {
        server_->close();
        // 注意: 不调用 deleteLater, 因为我们要直接销毁整个对象
        server_->disconnect();
        delete server_;
        server_ = nullptr;
    }
    delete connCtx_;
    connCtx_ = nullptr;
}

void HttpFileServer::init() {
    // 使用 Config 中的默认配置
    quint16 defaultPort = static_cast<quint16>(Config::HTTP_PORT);
    QString defaultDir = QString::fromStdString(std::string("output"));
    start(defaultPort, defaultDir);
}

void HttpFileServer::start(quint16 port, const QString& rootDir) {
    // 先停止旧服务(不带锁,避免死锁)
    if (server_) {
        server_->close();
        server_->disconnect();
        delete server_;
        server_ = nullptr;
    }

    QMutexLocker locker(&mutex_);

    // 如果参数为默认值,使用 Config 中的值
    quint16 actualPort = (port == 0) ? static_cast<quint16>(Config::HTTP_PORT) : port;
    QString actualDir = rootDir.isEmpty() ? QString::fromStdString(std::string("output")) : rootDir;

    rootDir_ = actualDir;
    port_ = actualPort;

    server_ = new QTcpServer(connCtx_);
    if (!server_->listen(QHostAddress::Any, port_)) {
        if (logCallback_) {
            logCallback_("HTTP", QString("文件服务启动失败, 端口: %1").arg(port_));
        }
        return;
    }

    QObject::connect(server_, &QTcpServer::newConnection, connCtx_, [this]() {
        onNewConnection();
    });

    if (logCallback_) {
        logCallback_("HTTP", QString("文件服务已启动: http://0.0.0.0:%1").arg(port_));
    }
}

void HttpFileServer::stop() {
    QMutexLocker locker(&mutex_);

    if (server_) {
        server_->close();
        server_->disconnect();
        delete server_;
        server_ = nullptr;
    }
}

bool HttpFileServer::isRunning() const {
    QMutexLocker locker(&mutex_);
    return server_ != nullptr && server_->isListening();
}

quint16 HttpFileServer::port() const {
    QMutexLocker locker(&mutex_);
    return port_;
}

QString HttpFileServer::rootDir() const {
    QMutexLocker locker(&mutex_);
    return rootDir_;
}

void HttpFileServer::setLogCallback(std::function<void(const QString&, const QString&)> callback) {
    logCallback_ = std::move(callback);
}

void HttpFileServer::onNewConnection() {
    auto* socket = server_->nextPendingConnection();
    if (!socket) return;

    QString peerIp = socket->peerAddress().toString();
    quint16 peerPort = socket->peerPort();

    if (logCallback_) {
        logCallback_("HTTP", QString("新连接 [%1:%2]").arg(peerIp).arg(peerPort));
    }

    // 使用 QPointer 确保对象销毁时自动清理
    QPointer<QTcpSocket> socketPtr(socket);

    // 读取请求数据
    QObject::connect(socket, &QTcpSocket::readyRead, socketPtr, [this, socketPtr]() {
        auto* socket = socketPtr.data();
        if (!socket) return;

        // 累积请求数据到缓冲区 (处理TCP分片)
        requestBuffers_[socket].append(socket->readAll());
        if (!requestBuffers_[socket].contains("\r\n\r\n")) {
            return; // 请求头不完整, 等待更多数据
        }

        // 取走完整请求数据并从缓冲区移除
        QByteArray requestData = requestBuffers_.take(socket);
        if (logCallback_) {
            logCallback_("HTTP", QString("收到请求: %1").arg(QString::fromUtf8(requestData.left(200))));
        }

        QString request = QString::fromUtf8(requestData);
        QStringList lines = request.split("\r\n");
        if (lines.isEmpty()) {
            socket->close();
            return;
        }

        // 解析 GET /filename.ext HTTP/1.1
        QStringList parts = lines[0].split(' ');
        if (parts.size() < 2) {
            socket->write("HTTP/1.1 400 Bad Request\r\n\r\n");
            socket->close();
            return;
        }
        if (parts[0] != "GET") {
            socket->write("HTTP/1.1 405 Method Not Allowed\r\n\r\n");
            socket->close();
            return;
        }

        // 防止路径穿越: 解析请求路径并验证在 rootDir_ 内
        QString requestPath = parts[1];
        QString filePath = QDir::cleanPath(rootDir_ + "/" + requestPath);
        QString cleanRoot = QDir::cleanPath(rootDir_);
        if (!filePath.startsWith(cleanRoot + "/") && filePath != cleanRoot) {
            socket->write("HTTP/1.1 404 Not Found\r\n\r\n");
            socket->close();
            return;
        }
        QString fileName = QFileInfo(filePath).fileName();
        if (logCallback_) {
            logCallback_("HTTP", QString("请求文件: %1 (rootDir=%2)").arg(fileName).arg(rootDir_));
        }
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            socket->write("HTTP/1.1 404 Not Found\r\n\r\n");
            socket->close();
            return;
        }

        QByteArray data = file.readAll();
        file.close();

        // 根据扩展名设置 Content-Type (传入完整文件名)
        QString mime = getMimeType(fileName);

        if (logCallback_) {
            logCallback_("HTTP", QString("发送文件: %1 (%2 bytes, MIME=%3)").arg(fileName).arg(data.size()).arg(mime));
        }

        // 构造 HTTP 响应
        QByteArray header = QString(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %1\r\n"
            "Content-Length: %2\r\n"
            "Connection: close\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "\r\n").arg(mime).arg(data.size()).toUtf8();

        socket->write(header + data);
        socket->disconnectFromHost();
    });

    // 客户端断开时自动清理
    QObject::connect(socket, &QTcpSocket::disconnected, socketPtr, [this, socketPtr]() {
        if (socketPtr) {
            requestBuffers_.remove(socketPtr.data());
            socketPtr->deleteLater();
        }
    });
}

QString HttpFileServer::getMimeType(const QString& fileName) {
    QString ext = QFileInfo(fileName).suffix().toLower();
    
    static const std::map<QString, QString> mimeTypes = {
        {"mp4",  "video/mp4"},
        {"webm", "video/webm"},
        {"ogg",  "video/ogg"},
        {"avi",  "video/x-msvideo"},
        {"mov",  "video/quicktime"},
        {"mkv",  "video/x-matroska"},
        {"jpg",  "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"png",  "image/png"},
        {"gif",  "image/gif"},
        {"webp", "image/webp"},
        {"bmp",  "image/bmp"},
        {"svg",  "image/svg+xml"},
        {"ico",  "image/x-icon"},
        {"txt",  "text/plain"},
        {"html", "text/html"},
        {"htm",  "text/html"},
        {"css",  "text/css"},
        {"js",   "application/javascript"},
        {"json", "application/json"},
        {"xml",  "application/xml"},
        {"pdf",  "application/pdf"},
        {"zip",  "application/zip"},
        {"map",  "application/json"},  // source map
    };

    auto it = mimeTypes.find(ext);
    if (it != mimeTypes.end()) {
        return it->second;
    }
    return "application/octet-stream";
}