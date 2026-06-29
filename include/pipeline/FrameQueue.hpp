/*
 * FrameQueue.hpp - 帧队列（SPSC 优化）
 *
 * 单生产者/单消费者队列，用于 DetectThread → GUI 线程的帧传递。
 * 优化：使用 cv::Mat 移动语义避免深拷贝。
 *
 * 特点：
 *   - 容量限制 8 帧，超容丢弃最旧帧
 *   - 线程安全（mutex + condition_variable）
 *   - move 语义减少内存拷贝
 */

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

    // 生产者：移动帧数据，避免 clone()
    void push(int ch, cv::Mat frame, double fps) {
        {
            std::lock_guard<std::mutex> lk(mtx);
            if (q.size() >= 8) q.pop();  // 丢弃最旧帧
            q.push({ch, std::move(frame), fps});
        }
    }

    // 消费者：移动取出
    bool pop(Entry& e) {
        std::lock_guard<std::mutex> lk(mtx);
        if (q.empty()) return false;
        e = std::move(q.front());
        q.pop();
        return true;
    }
};

#endif // FRAME_QUEUE_HPP
