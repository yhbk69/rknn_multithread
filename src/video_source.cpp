/**
 * @file video_source.cpp
 * @brief 视频输入源实现
 */

#include "io/video_source.hpp"

// ---- CameraVideoSource ----

CameraVideoSource::CameraVideoSource(int cameraId, const QString& source) {
    if (!source.isEmpty() && source.startsWith("rtsp://")) {
        name_ = source;
        cap_.open(source.toStdString());
    } else {
        bool ok = false;
        int devId = source.isEmpty() ? cameraId : source.toInt(&ok);
        name_ = QString("camera_%1").arg(ok ? devId : cameraId);
#ifdef _WIN32
        cap_.open(ok ? devId : cameraId, cv::CAP_DSHOW);
#else
        cap_.open(ok ? devId : cameraId);
#endif
    }
    if (cap_.isOpened()) {
        cap_.set(cv::CAP_PROP_BUFFERSIZE, 1);
    }
}

CameraVideoSource::~CameraVideoSource() { close(); }

void CameraVideoSource::close() {
    std::lock_guard<std::mutex> lock(capMutex_);
    if (cap_.isOpened()) cap_.release();
}

bool CameraVideoSource::validate(const QString& source, int cameraId) {
    cv::VideoCapture cap;
    if (!source.isEmpty() && source.startsWith("rtsp://")) {
        cap.open(source.toStdString());
    } else {
        bool ok = false;
        int devId = source.isEmpty() ? cameraId : source.toInt(&ok);
#ifdef _WIN32
        cap.open(ok ? devId : cameraId, cv::CAP_DSHOW);
#else
        cap.open(ok ? devId : cameraId);
#endif
    }
    bool valid = cap.isOpened();
    if (valid) cap.release();
    return valid;
}

bool CameraVideoSource::readFrame(cv::Mat& frame) {
    std::lock_guard<std::mutex> lock(capMutex_);
    if (!cap_.isOpened()) return false;
    return cap_.read(frame);
}

// ---- FileVideoSource ----

FileVideoSource::FileVideoSource(const QString& filePath)
    : name_(filePath) {
    cap_.open(filePath.toStdString());
}

FileVideoSource::~FileVideoSource() {
    std::lock_guard<std::mutex> lock(capMutex_);
    if (cap_.isOpened()) cap_.release();
}

bool FileVideoSource::readFrame(cv::Mat& frame) {
    std::lock_guard<std::mutex> lock(capMutex_);
    if (!cap_.isOpened()) return false;
    return cap_.read(frame);
}
