#ifndef FRAME_QUEUE_HPP
#define FRAME_QUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>
#include <opencv2/core.hpp>

struct FrameQueue {
    struct Entry {
        int channel = 0;
        cv::Mat bgr_frame;
        double fps = 0;
    };
    std::queue<Entry> q;
    mutable std::mutex mtx;
    std::condition_variable cv;

    void push(int ch, const cv::Mat& frame, double fps) {
        {
            std::lock_guard<std::mutex> lk(mtx);
            if (q.size() >= 8) q.pop();
            q.push({ch, frame.clone(), fps});
        }
        cv.notify_one();
    }

    bool pop(Entry& e) {
        std::unique_lock<std::mutex> lk(mtx);
        if (q.empty()) return false;
        e = std::move(q.front());
        q.pop();
        return true;
    }
};

#endif // FRAME_QUEUE_HPP
