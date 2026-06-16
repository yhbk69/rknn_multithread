#ifndef PIPELINE_STATS_COLLECTOR_HPP
#define PIPELINE_STATS_COLLECTOR_HPP

#include <chrono>

class StatsCollector {
public:
    using Clock = std::chrono::steady_clock;

    void start() {
        start_time_ = Clock::now();
        last_fps_time_ = start_time_;
        current_fps_ = 0.0;
    }

    long long elapsedMs() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - start_time_).count();
    }

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

    double getCurrentFps() const { return current_fps_; }

    double calcAvgFps(int frames) const {
        auto dt = elapsedMs();
        return (dt > 0) ? (double)frames / dt * 1000.0 : 0.0;
    }

private:
    Clock::time_point start_time_;
    Clock::time_point last_fps_time_;
    double current_fps_ = 0.0;
};

#endif
