#ifndef PIPELINE_FRAME_READER_HPP
#define PIPELINE_FRAME_READER_HPP

#include <string>
#include <chrono>
#include <thread>
#include <opencv2/opencv.hpp>

class FrameReader {
public:
    FrameReader(const std::string& video_path, int reconnect_delay_ms = 1000)
        : video_path_(video_path), reconnect_delay_ms_(reconnect_delay_ms) {}

    bool open() {
        if (video_path_.length() == 1 && video_path_[0] >= '0' && video_path_[0] <= '9')
            capture_.open((int)(video_path_[0] - '0'));
        else
            capture_.open(video_path_);
        return capture_.isOpened();
    }

    bool read(cv::Mat& frame) {
        return capture_.read(frame);
    }

    bool isOpened() const {
        return capture_.isOpened();
    }

    bool readWithRetry(cv::Mat& frame, int max_retries = 3) {
        for (int i = 0; i < max_retries; i++) {
            if (capture_.read(frame))
                return true;
            if (i < max_retries - 1) {
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

private:
    std::string video_path_;
    cv::VideoCapture capture_;
    int reconnect_delay_ms_;
};

#endif
