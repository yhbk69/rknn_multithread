/*
 * DetectionStats.hpp - 检测统计仪表盘
 *
 * 实时统计：
 *   - FPS (当前/平均)
 *   - 推理延迟 (ms)
 *   - 检测数量 (每帧/累计)
 *   - 类别分布
 *   - NPU 利用率
 *
 * WebSocket 协议:
 *   请求: {"type":"get_stats"}
 *   响应: {"type":"stats","data":{...}}
 */

#ifndef DETECTION_STATS_HPP
#define DETECTION_STATS_HPP

#include <chrono>
#include <map>
#include <string>
#include <mutex>
#include <atomic>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class DetectionStats {
public:
    using Clock = std::chrono::steady_clock;

    void start() {
        std::lock_guard<std::mutex> lock(mtx_);
        start_time_ = Clock::now();
        last_fps_time_ = start_time_;
        last_push_time_ = start_time_;
        current_fps_ = 0.0;
        total_frames_ = 0;
        total_detections_ = 0;
        class_counts_.clear();
        recent_latencies_.clear();
    }

    /* 记录一帧处理完成 */
    void recordFrame(double inference_ms, int detection_count,
                     const std::vector<std::string>& class_names = {}) {
        std::lock_guard<std::mutex> lock(mtx_);

        total_frames_++;
        total_detections_ += detection_count;

        // 记录延迟（保留最近 100 帧）
        recent_latencies_.push_back(inference_ms);
        if (recent_latencies_.size() > 100)
            recent_latencies_.erase(recent_latencies_.begin());

        // 统计类别
        for (const auto& name : class_names) {
            class_counts_[name]++;
        }

        // 更新 FPS
        auto now = Clock::now();
        auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_fps_time_).count();
        if (dt >= 1000) {  // 每秒更新一次
            current_fps_ = (double)frame_count_interval_ / dt * 1000.0;
            frame_count_interval_ = 0;
            last_fps_time_ = now;
        }
        frame_count_interval_++;
    }

    /* 获取当前 FPS */
    double getCurrentFps() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return current_fps_;
    }

    /* 获取平均 FPS */
    double getAvgFps() const {
        std::lock_guard<std::mutex> lock(mtx_);
        auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - start_time_).count();
        return (dt > 0) ? (double)total_frames_ / dt * 1000.0 : 0.0;
    }

    /* 获取平均延迟 */
    double getAvgLatency() const {
        std::lock_guard<std::mutex> lock(mtx_);
        if (recent_latencies_.empty()) return 0.0;
        double sum = 0;
        for (double v : recent_latencies_) sum += v;
        return sum / recent_latencies_.size();
    }

    /* 获取 P95 延迟 */
    double getP95Latency() const {
        std::lock_guard<std::mutex> lock(mtx_);
        if (recent_latencies_.empty()) return 0.0;
        auto sorted = recent_latencies_;
        std::sort(sorted.begin(), sorted.end());
        size_t idx = (size_t)(sorted.size() * 0.95);
        return sorted[idx];
    }

    /* 获取总帧数 */
    long long getTotalFrames() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return total_frames_;
    }

    /* 获取总检测数 */
    long long getTotalDetections() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return total_detections_;
    }

    /* 获取类别统计 */
    std::map<std::string, long long> getClassCounts() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return class_counts_;
    }

    /* 生成 JSON 统计数据 */
    json toJson() const {
        std::lock_guard<std::mutex> lock(mtx_);

        json j;
        j["fps_current"] = current_fps_;
        j["fps_avg"] = getAvgFpsUnlocked();
        j["latency_avg_ms"] = getAvgLatencyUnlocked();
        j["latency_p95_ms"] = getP95LatencyUnlocked();
        j["total_frames"] = total_frames_;
        j["total_detections"] = total_detections_;
        j["detections_per_frame"] = (total_frames_ > 0) ?
            (double)total_detections_ / total_frames_ : 0.0;

        // 类别统计（前 10 名）
        json classes;
        std::vector<std::pair<std::string, long long>> sorted(class_counts_.begin(), class_counts_.end());
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        for (size_t i = 0; i < std::min(sorted.size(), (size_t)10); i++) {
            classes[sorted[i].first] = sorted[i].second;
        }
        j["class_counts"] = classes;

        // 运行时间
        auto dt = std::chrono::duration_cast<std::chrono::seconds>(
            Clock::now() - start_time_).count();
        j["uptime_seconds"] = dt;

        return j;
    }

    /* 检查是否应该推送（每 N 秒一次） */
    bool shouldPush(int interval_ms = 1000) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto now = Clock::now();
        auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_push_time_).count();
        if (dt >= interval_ms) {
            last_push_time_ = now;
            return true;
        }
        return false;
    }

    /* 重置统计 */
    void reset() {
        std::lock_guard<std::mutex> lock(mtx_);
        total_frames_ = 0;
        total_detections_ = 0;
        current_fps_ = 0.0;
        recent_latencies_.clear();
        class_counts_.clear();
        start_time_ = Clock::now();
        last_fps_time_ = start_time_;
    }

private:
    double getAvgFpsUnlocked() const {
        auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - start_time_).count();
        return (dt > 0) ? (double)total_frames_ / dt * 1000.0 : 0.0;
    }

    double getAvgLatencyUnlocked() const {
        if (recent_latencies_.empty()) return 0.0;
        double sum = 0;
        for (double v : recent_latencies_) sum += v;
        return sum / recent_latencies_.size();
    }

    double getP95LatencyUnlocked() const {
        if (recent_latencies_.empty()) return 0.0;
        auto sorted = recent_latencies_;
        std::sort(sorted.begin(), sorted.end());
        size_t idx = (size_t)(sorted.size() * 0.95);
        return sorted[idx];
    }

    mutable std::mutex mtx_;
    Clock::time_point start_time_;
    Clock::time_point last_fps_time_;
    Clock::time_point last_push_time_;
    double current_fps_ = 0.0;
    int frame_count_interval_ = 0;
    long long total_frames_ = 0;
    long long total_detections_ = 0;
    std::vector<double> recent_latencies_;
    std::map<std::string, long long> class_counts_;
};

#endif // DETECTION_STATS_HPP
