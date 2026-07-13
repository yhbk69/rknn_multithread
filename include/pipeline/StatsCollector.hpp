/*
 * StatsCollector.hpp - 帧率统计收集器
 *
 * 计算实时 FPS 和总平均 FPS，用于检测线程的性能监控。
 * 使用滑动窗口方式计算实时 FPS，避免频繁更新。
 */

#ifndef PIPELINE_STATS_COLLECTOR_HPP
#define PIPELINE_STATS_COLLECTOR_HPP

#include <chrono>

class StatsCollector {
public:
    using Clock = std::chrono::steady_clock;

    /* 重置统计状态，开始新的统计周期 */
    void start() {
        start_time_ = Clock::now();
        last_fps_time_ = start_time_;
        current_fps_ = 0.0;
    }

    /* 获取从 start() 以来经过的毫秒数 */
    long long elapsedMs() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - start_time_).count();
    }

    /*
     * 更新实时 FPS（每 interval_frames 帧计算一次）
     * @param frame_count     当前帧数
     * @param interval_frames FPS 计算间隔（默认30帧）
     * @return 当前实时 FPS
     */
    double updateFps(int frame_count, int interval_frames = 30) {
        if (frame_count % interval_frames == 0 && frame_count > 0) {
            auto now = Clock::now();
            auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_fps_time_).count();
            if (dt > 0)
                current_fps_ = (double)interval_frames / dt * 1000.0;
            last_fps_time_ = now;
        }
        return current_fps_;
    }

    /* 获取当前实时 FPS（上次 updateFps 计算的结果） */
    double getCurrentFps() const { return current_fps_; }

    /*
     * 计算总平均 FPS
     * @param frames 总帧数
     * @return 平均 FPS
     */
    double calcAvgFps(int frames) const {
        auto dt = elapsedMs();
        return (dt > 0) ? (double)frames / dt * 1000.0 : 0.0;
    }

private:
    Clock::time_point start_time_;   // 统计起始时间
    Clock::time_point last_fps_time_; // 上次 FPS 计算时间
    double current_fps_ = 0.0;       // 当前实时 FPS
};

#endif
