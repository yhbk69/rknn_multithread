/**
 * @file mjpeg_streamer.cpp
 * @brief MJPEG 推流服务模块实现
 */

#include "mjpeg_streamer.hpp"
#include <QObject>
#include <QBuffer>
#include <QTcpSocket>
#include <QTcpServer>


MjpegStreamer::MjpegStreamer()
    : connCtx_(new QObject()),
      lastFrameTime_(std::chrono::steady_clock::now())
{
}

MjpegStreamer::~MjpegStreamer() {
    stop();
    delete connCtx_;
}

void MjpegStreamer::start(quint16 port, const QString& hostIp) {
    server_ = new QTcpServer();
    if (!server_->listen(QHostAddress::Any, port)) {
        if (logCallback_) {
            logCallback_(QString::fromUtf8("MJPEG"),
                QString::fromUtf8("服务启动失败, 端口: %1").arg(port));
        }
        delete server_;
        server_ = nullptr;
        return;
    }

    QObject::connect(server_, &QTcpServer::newConnection, connCtx_, [this]() {
        onNewConnection();
    });

    if (logCallback_) {
        logCallback_(QString::fromUtf8("MJPEG"),
            QString::fromUtf8("服务已启动: http://%1:%2/stream").arg(hostIp).arg(port));
    }
}

void MjpegStreamer::pushFrame(const QByteArray& jpegData) {
    if (jpegData.isEmpty()) return;

    QMutexLocker locker(&clientsMutex_);
    if (clients_.isEmpty()) return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameTime_).count();
    if (elapsed < 33) return;
    lastFrameTime_ = now;

    QByteArray chunk = QByteArray("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: ")
        + QByteArray::number(jpegData.size()) + QByteArray("\r\n\r\n")
        + jpegData + QByteArray("\r\n");

    auto clients = clients_;
    for (auto* client : clients) {
        if (client && client->state() == QAbstractSocket::ConnectedState) {
            client->write(chunk);
        }
    }
}

void MjpegStreamer::pushImage(const QImage& image, int quality) {
    QByteArray jpegData;
    QBuffer buf(&jpegData);
    buf.open(QIODevice::WriteOnly);
    image.save(&buf, "JPEG", quality);
    pushFrame(jpegData);
}

void MjpegStreamer::stop() {
    if (!server_) return;

    {
        QMutexLocker locker(&clientsMutex_);
        for (auto* client : clients_) {
            if (client) {
                client->disconnectFromHost();
                client->deleteLater();
            }
        }
        clients_.clear();
    }

    server_->close();
    delete server_;
    server_ = nullptr;
}

int MjpegStreamer::clientCount() const {
    return clients_.size();
}

void MjpegStreamer::onFrame(const FrameData& data) {
    pushImage(data.image);
}

void MjpegStreamer::setLogCallback(std::function<void(const QString&, const QString&)> callback) {
    logCallback_ = std::move(callback);
}

void MjpegStreamer::onNewConnection() {
    if (!server_) return;

    auto* socket = server_->nextPendingConnection();
    if (!socket) return;

    QByteArray header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
        "Connection: keep-alive\r\n"
        "Cache-Control: no-cache\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";
    socket->write(header);
    socket->flush();

    {
        QMutexLocker locker(&clientsMutex_);
        clients_.append(socket);
    }

    if (logCallback_) {
        logCallback_(QString::fromUtf8("MJPEG"), QString::fromUtf8("新客户端已连接"));
    }

    QObject::connect(socket, &QTcpSocket::disconnected, connCtx_, [this, socket]() {
        onClientDisconnected(socket);
    });
}

void MjpegStreamer::onClientDisconnected(QTcpSocket* socket) {
    if (!socket) return;

    {
        QMutexLocker locker(&clientsMutex_);
        clients_.removeAll(socket);
    }

    if (logCallback_) {
        logCallback_(QString::fromUtf8("MJPEG"), QString::fromUtf8("客户端已断开"));
    }

    socket->deleteLater();
}
