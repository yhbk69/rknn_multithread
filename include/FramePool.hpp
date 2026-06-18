/*
 * FramePool.hpp - 帧内存池
 *
 * 预分配固定大小的 cv::Mat 缓冲区，避免每帧动态内存分配。
 * 适用于高频帧处理场景，如实时视频检测。
 *
 * 特点：
 *   - 预分配缓冲区，减少堆分配
 *   - 线程安全，支持多线程并发 acquire/release
 *   - 自动扩容，当池耗尽时动态分配新缓冲区
 *   - 支持缓冲区回收和重用
 *
 * 使用场景：
 *   - 替代 CascadePipeline::put() 中的 cv::Mat::clone()
 *   - 替代 rknnPool 中的帧拷贝
 */

#ifndef FRAMEPOOL_H
#define FRAMEPOOL_H

#include <opencv2/core.hpp>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

class FramePool {
public:
    /*
     * 构造函数
     * @param pool_size 初始池大小（预分配的缓冲区数量）
     */
    explicit FramePool(size_t pool_size = 16)
        : pool_size_(pool_size)
        , total_allocated_(0)
    {
        // 预分配缓冲区（延迟到首次 acquire 时分配）
        // 这样可以根据实际需要的尺寸分配
    }

    ~FramePool() = default;

    // 禁用拷贝
    FramePool(const FramePool&) = delete;
    FramePool& operator=(const FramePool&) = delete;

    /*
     * acquire - 获取一个缓冲区
     * @param rows 行数
     * @param cols 列数
     * @param type OpenCV 数据类型 (如 CV_8UC3)
     * @return cv::Mat 缓冲区（按值返回，内部使用共享指针管理）
     *
     * 如果池中有可用缓冲区且尺寸匹配，则重用；
     * 否则从池中获取一个或分配新的。
     */
    cv::Mat acquire(int rows, int cols, int type) {
        std::lock_guard<std::mutex> lock(mtx_);

        // 尝试从空闲列表获取匹配尺寸的缓冲区
        while (!free_list_.empty()) {
            cv::Mat mat = free_list_.front();
            free_list_.pop();

            // 检查尺寸是否匹配
            if (mat.rows == rows && mat.cols == cols && mat.type() == type) {
                return mat;
            }
            // 尺寸不匹配，丢弃这个缓冲区
        }

        // 池中没有匹配的缓冲区，分配新的
        cv::Mat mat(rows, cols, type);
        total_allocated_++;
        return mat;
    }

    /*
     * release - 释放缓冲区回池
     * @param mat 要释放的缓冲区
     *
     * 将缓冲区放回空闲列表，供后续重用。
     */
    void release(cv::Mat mat) {
        if (mat.empty()) return;

        std::lock_guard<std::mutex> lock(mtx_);

        // 限制池大小，防止内存无限增长
        if (free_list_.size() < pool_size_) {
            free_list_.push(mat);
        }
        // 如果池已满，直接丢弃（让 GC 回收）
    }

    /*
     * clear - 清空缓冲池
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mtx_);
        while (!free_list_.empty()) {
            free_list_.pop();
        }
        total_allocated_ = 0;
    }

    /*
     * stats - 获取池统计信息
     */
    struct Stats {
        size_t pool_size;        // 池大小限制
        size_t free_count;       // 当前空闲缓冲区数量
        size_t total_allocated;  // 总分配次数
    };

    Stats stats() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return {
            pool_size_,
            free_list_.size(),
            total_allocated_
        };
    }

    /*
     * set_pool_size - 设置池大小限制
     */
    void set_pool_size(size_t size) {
        std::lock_guard<std::mutex> lock(mtx_);
        pool_size_ = size;

        // 如果当前空闲列表超过新限制，丢弃多余的
        while (free_list_.size() > pool_size_) {
            free_list_.pop();
        }
    }

private:
    mutable std::mutex mtx_;
    std::queue<cv::Mat> free_list_;
    size_t pool_size_;
    size_t total_allocated_;
};

/*
 * ScopedFrame - RAII 风格的帧管理
 *
 * 自动管理帧的获取和释放，确保资源正确回收。
 */
class ScopedFrame {
public:
    ScopedFrame(FramePool& pool, int rows, int cols, int type)
        : pool_(pool)
        , mat_(pool.acquire(rows, cols, type))
    {}

    ~ScopedFrame() {
        pool_.release(mat_);
    }

    // 禁用拷贝
    ScopedFrame(const ScopedFrame&) = delete;
    ScopedFrame& operator=(const ScopedFrame&) = delete;

    // 允许移动
    ScopedFrame(ScopedFrame&& other) noexcept
        : pool_(other.pool_)
        , mat_(std::move(other.mat_))
    {
        other.mat_ = cv::Mat();
    }

    cv::Mat& get() { return mat_; }
    const cv::Mat& get() const { return mat_; }

    // 隐式转换到 cv::Mat
    operator cv::Mat&() { return mat_; }
    operator const cv::Mat&() const { return mat_; }

private:
    FramePool& pool_;
    cv::Mat mat_;
};

#endif // FRAMEPOOL_H
