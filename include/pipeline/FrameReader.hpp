/*
 * FrameReader.hpp - 视频/摄像头/RTSP 流读取器
 *
 * 支持：
 *   - 本地视频文件
 *   - 摄像头设备 (/dev/videoN 或数字 ID)
 *   - RTSP 网络流（自动优化缓冲和超时）
 *
 * RTSP 优化：
 *   - 设置最小缓冲区避免延迟累积
 *   - 启用丢帧策略只读取最新帧
 *   - 断线自动重连
 */

#ifndef PIPELINE_FRAME_READER_HPP
#define PIPELINE_FRAME_READER_HPP

#include <string>
#include <chrono>
#include <thread>
#include <opencv2/opencv.hpp>
#include "Logger.hpp"

class FrameReader {
public:
    FrameReader(const std::string& video_path, int reconnect_delay_ms = 1000)
        : video_path_(video_path), reconnect_delay_ms_(reconnect_delay_ms) {}

    bool open() {
        // 判断是摄像头 ID 还是文件/URL
        char *end;
        long id = strtol(video_path_.c_str(), &end, 10);
        if (end != video_path_.c_str() && *end == '\0' && id >= 0) {
            capture_.open((int)id);
        } else {
            capture_.open(video_path_);
        }

        if (!capture_.isOpened()) return false;

        // RTSP 专用优化：最小缓冲
        is_rtsp_ = (video_path_.find("rtsp://") == 0);
        if (is_rtsp_) {
            capture_.set(cv::CAP_PROP_BUFFERSIZE, 1);
            LOG_INFO("[FrameReader]", "RTSP stream opened: %s", video_path_.c_str());
        }

        return true;
    }

    bool read(cv::Mat& frame) {
        return capture_.read(frame);
    }

    bool isOpened() const {
        return capture_.isOpened();
    }

    // RTSP 丢帧策略：只读取最新帧，跳过积压帧
    bool readLatest(cv::Mat& frame) {
        if (!is_rtsp_) return read(frame);

        cv::Mat latest;
        bool got_frame = false;
        // grab() 跳过帧，retrieve() 解码
        while (capture_.grab()) {
            capture_.retrieve(latest);
            got_frame = true;
        }
        if (got_frame) {
            frame = std::move(latest);
            return true;
        }
        // 没有积压帧时正常读取
        return capture_.read(frame);
    }

    bool readWithRetry(cv::Mat& frame, int max_retries = 3) {
        for (int i = 0; i < max_retries; i++) {
            if (readLatest(frame))
                return true;
            if (i < max_retries - 1) {
                LOG_WARN("[FrameReader]", "Read failed, reconnecting (%d/%d)...", i + 1, max_retries);
                std::this_thread::sleep_for(std::chrono::milliseconds(reconnect_delay_ms_));
                open();
            }
        }
        return false;
    }

    void close() {
        if (capture_.isOpened())
            capture_.release();
    }

    bool isRtsp() const { return is_rtsp_; }

private:
    std::string video_path_;
    cv::VideoCapture capture_;
    int reconnect_delay_ms_;
    bool is_rtsp_ = false;
};

#endif
