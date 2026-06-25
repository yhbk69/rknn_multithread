/*
 * VideoRecorder.hpp - 视频录制器
 *
 * 将检测结果帧录制到 MP4 文件。
 * 线程安全：可在 DetectThread 中调用。
 *
 * 使用：
 *   VideoRecorder rec;
 *   rec.open("output.mp4", 25, 640, 480);
 *   rec.write(frame);  // 每帧调用
 *   rec.close();       // 停止录制
 */

#ifndef VIDEO_RECORDER_HPP
#define VIDEO_RECORDER_HPP

#include <opencv2/videoio.hpp>
#include <opencv2/core.hpp>
#include <string>
#include <mutex>
#include <atomic>

class VideoRecorder {
public:
    VideoRecorder() = default;
    ~VideoRecorder() { close(); }

    // 禁用拷贝
    VideoRecorder(const VideoRecorder&) = delete;
    VideoRecorder& operator=(const VideoRecorder&) = delete;

    /*
     * open - 打开录制文件
     * @param path 输出文件路径（.mp4）
     * @param fps 帧率
     * @param width 视频宽度
     * @param height 视频高度
     * @return 成功返回 true
     */
    bool open(const std::string& path, double fps, int width, int height) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (writer_.isOpened()) writer_.release();

        int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
        writer_.open(path, fourcc, fps, cv::Size(width, height));
        if (!writer_.isOpened()) return false;

        recording_.store(true);
        frame_count_ = 0;
        return true;
    }

    /*
     * write - 写入一帧
     * @param frame BGR 格式的 cv::Mat
     */
    void write(const cv::Mat& frame) {
        if (!recording_.load()) return;
        std::lock_guard<std::mutex> lock(mtx_);
        if (writer_.isOpened()) {
            writer_.write(frame);
            frame_count_++;
        }
    }

    /*
     * close - 停止录制并关闭文件
     */
    void close() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (writer_.isOpened()) writer_.release();
        recording_.store(false);
    }

    bool isRecording() const { return recording_.load(); }
    int frameCount() const { return frame_count_; }

private:
    cv::VideoWriter writer_;
    std::mutex mtx_;
    std::atomic<bool> recording_{false};
    int frame_count_ = 0;
};

#endif // VIDEO_RECORDER_HPP
