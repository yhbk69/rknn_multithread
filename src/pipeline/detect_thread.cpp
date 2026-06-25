#include "pipeline/DetectThread.hpp"
#include "websocket.hpp"
#include "Logger.hpp"

#include <QVector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void DetectThread::run() {
    auto pipeline = std::make_unique<CascadePipeline>();
    std::vector<ModelConfig> model_configs;
    if (use_cascade_)
        model_configs = cascade_models_;
    else
        model_configs.push_back(single_cfg_);
    if (pipeline->init(model_configs, channel_id_) != 0) {
        emit error(QString("[Ch%1] CascadePipeline init failed!").arg(channel_id_));
        emit finished();
        return;
    }
    pipeline->set_thresholds(conf_threshold_, nms_threshold_);

    FrameReader reader(video_path_);
    if (!reader.open()) {
        emit error(QString("[Ch%1] Cannot open video: %2").arg(channel_id_).arg(QString::fromStdString(video_path_)));
        emit finished();
        return;
    }

    StatsCollector stats;
    stats.start();
    ResultRenderer renderer;

    int frames = 0;

    while (running_.load()) {
        while (paused_.load() && running_.load()) {
            if (step_once_.exchange(false)) break;
            QThread::msleep(50);
        }
        if (!running_.load()) break;

        cv::Mat img;
        if (!reader.read(img)) {
            emit error(QString("[Ch%1] Failed to read frame").arg(channel_id_));
            break;
        }

        long long infer_start = stats.elapsedMs();

        int roi_x = 0, roi_y = 0;
        cv::Mat detect_img;
        if (roi_enabled_ && !roi_rect_.isNull()) {
            int x1 = qBound(0, roi_rect_.x(), img.cols - 1);
            int y1 = qBound(0, roi_rect_.y(), img.rows - 1);
            int x2 = qBound(x1 + 1, roi_rect_.x() + roi_rect_.width(), img.cols);
            int y2 = qBound(y1 + 1, roi_rect_.y() + roi_rect_.height(), img.rows);
            roi_x = x1; roi_y = y1;
            detect_img = img(cv::Range(y1, y2), cv::Range(x1, x2)).clone();
        } else {
            detect_img = img;
        }

        if (pipeline->put(detect_img) != 0) break;

        if (frames >= thread_num_ && pipeline->get(detect_img) != 0) break;

        if (frames >= thread_num_) {
            detect_result_group_t result = pipeline->getLastDetectResult();

            if (roi_enabled_ && !roi_rect_.isNull()) {
                for (int i = 0; i < result.count; i++) {
                    result.results[i].box.left += roi_x;
                    result.results[i].box.right += roi_x;
                    result.results[i].box.top += roi_y;
                    result.results[i].box.bottom += roi_y;
                }
                detect_img.copyTo(img(cv::Range(roi_y, roi_y + detect_img.rows),
                                      cv::Range(roi_x, roi_x + detect_img.cols)));
            }

            if (auto ws = ws_.lock()) {
                if (ws->isAlarmEnabled()) {
                    int alarm_cnt = ws->checkAndAlarm(&result, frames, detect_img);
                    if (alarm_cnt > 0) {
                        for (int i = 0; i < result.count; i++) {
                            const detect_result_t& det = result.results[i];
                            if (det.name[0] != '\0') {
                                std::string cls(det.name);
                                if (ws->config().alarm_class_names.count(cls) > 0)
                                    alarm_counts_[cls]++;
                            }
                        }
                    }
                }
            }

            QVector<FrameDetections::Det> dets;
            std::vector<std::string> class_names;
            for (int i = 0; i < result.count; i++) {
                const detect_result_t& det = result.results[i];
                dets.append({QString::fromUtf8(det.name), det.prop,
                             det.box.left, det.box.top, det.box.right, det.box.bottom});
                if (det.name[0] != '\0')
                    class_names.push_back(det.name);
            }
            for (const auto& name : class_names)
                class_counts_[name]++;
            if (!dets.isEmpty())
                emit detectionBatch(frames, dets);
        }

        cv::Mat& out_frame = (frames >= thread_num_) ? detect_img : img;
        double infer_time = (double)(stats.elapsedMs() - infer_start);
        double current_fps = stats.updateFps(frames);
        renderer.drawFps(out_frame, current_fps);

        if (frame_queue_)
            frame_queue_->push(channel_id_, out_frame, current_fps);

        if (frames % 5 == 0) {
            emit statsUpdated(frames, current_fps, infer_time);
            if (frames % 30 == 0) {
                long long total_alarms = 0;
                for (auto& [k, v] : alarm_counts_) total_alarms += v;
                json cls_j;
                for (auto& [k, v] : class_counts_) cls_j[k] = v;
                emit statsPanelUpdated(total_alarms, QString::fromStdString(cls_j.dump()));
            }
        }

        frames++;
    }

    while (true) {
        cv::Mat img;
        if (pipeline->get(img) != 0) break;
        if (frame_queue_)
            frame_queue_->push(channel_id_, img, stats.getCurrentFps());
    }

    double avg_fps = stats.calcAvgFps(frames);
    emit statsUpdated(frames, avg_fps, 0);
    emit error(QString("[Ch%1] Finished. Frames: %2, Avg FPS: %3")
                   .arg(channel_id_).arg(frames).arg(avg_fps, 0, 'f', 2));
    emit finished();
}
