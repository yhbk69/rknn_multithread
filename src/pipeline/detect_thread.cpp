/*
 * detect_thread.cpp - 单通道检测线程实现
 *
 * 主循环流程：读帧 → ROI裁剪 → NPU推理 → 坐标还原 → WebSocket报警 → UI更新
 *
 * 线程模型：
 *   - 继承 QThread，在 run() 中执行检测循环
 *   - 通过 Qt 信号槽与主线程通信（statsUpdated, detectionBatch 等）
 *   - CascadePipeline 内部使用 rknnPool 线程池并发推理
 */

#include "pipeline/DetectThread.hpp"
#include "pipeline/VideoRecorder.hpp"
#include "websocket.hpp"
#include "Logger.hpp"

#include <QVector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * 线程主循环 — 检测流水线
 *
 * 整体流程：
 *   1. 初始化：创建 CascadePipeline + FrameReader
 *   2. 主循环：读帧 → 推理 → 报警 → UI更新
 *   3. 清理：排空剩余帧 → 释放资源
 *
 * 帧同步策略：
 *   - 前 thread_num 帧只 put 不 get（填充线程池）
 *   - 第 thread_num 帧开始 put+get 同步处理
 *   - 保证 put/get 顺序一致（FIFO）
 */
void DetectThread::run() {
    // ========== 阶段1：初始化 ==========

    // 创建级联流水线（内部管理 rknnPool 线程池）
    auto pipeline = std::make_unique<CascadePipeline>();

    // 准备模型配置：级联模式用 cascade_models_，单模型模式用 single_cfg_
    std::vector<ModelConfig> model_configs;
    if (use_cascade_)
        model_configs = cascade_models_;
    else
        model_configs.push_back(single_cfg_);

    // 初始化流水线：加载 RKNN 模型、创建 NPU 推理线程池
    if (pipeline->init(model_configs, channel_id_) != 0) {
        emit error(QString("[Ch%1] CascadePipeline init failed!").arg(channel_id_));
        emit finished();
        return;
    }

    // 设置检测阈值
    pipeline->set_thresholds(conf_threshold_, nms_threshold_);

    // 打开视频源（支持文件/摄像头/RTSP）
    FrameReader reader(video_path_);
    if (!reader.open()) {
        emit error(QString("[Ch%1] Cannot open video: %2]")
                       .arg(channel_id_)
                       .arg(QString::fromStdString(video_path_)));
        emit finished();
        return;
    }

    // 初始化统计收集器和渲染器
    StatsCollector stats;
    stats.start();  // 记录起始时间
    ResultRenderer renderer;  // 用于绘制 FPS 文字

    int frames = 0;  // 已处理帧计数器

    // ========== 阶段2：主检测循环 ==========
    while (running_.load()) {

        // --- 暂停处理 ---
        // 暂停时阻塞在此，直到 resume() 或 stepFrame() 被调用
        while (paused_.load() && running_.load()) {
            if (step_once_.exchange(false)) break;  // 单帧步进：跳出暂停
            QThread::msleep(50);  // 暂停时低频轮询，避免 CPU 空转
        }
        if (!running_.load()) break;

        // --- 读取一帧 ---
        // readLatest() 对 RTSP 流会丢弃积压帧，只保留最新帧
        cv::Mat img;
        if (!reader.readLatest(img)) {
            emit error(QString("[Ch%1] Failed to read frame").arg(channel_id_));
            break;
        }

        // 记录推理开始时间（用于计算推理耗时）
        long long infer_start = stats.elapsedMs();

        // --- 跳帧策略（可选，默认关闭） ---
        // 当流水线队列积压严重时（>=12帧），丢弃新帧避免延迟累积
        // 注意：阈值必须高于正常 pending 数，否则会过度丢帧导致 FPS 暴跌
        if (skip_frames_ && frames >= thread_num_ && pipeline->pendingCount() >= 12) {
            dropped_frames_++;
            if (dropped_frames_ % 30 == 0)
                emit frameDropped(channel_id_, dropped_frames_);
            continue;  // 跳过本帧，不进行推理
        }

        // --- ROI 围栏处理 ---
        // 如果启用了 ROI（围栏），只检测 ROI 区域内的目标
        int roi_x = 0, roi_y = 0;
        cv::Mat detect_img;
        if (roi_enabled_ && !roi_rect_.isNull()) {
            // 将 ROI 矩形裁剪到图像范围内（防止越界）
            int x1 = qBound(0, roi_rect_.x(), img.cols - 1);
            int y1 = qBound(0, roi_rect_.y(), img.rows - 1);
            int x2 = qBound(x1 + 1, roi_rect_.x() + roi_rect_.width(), img.cols);
            int y2 = qBound(y1 + 1, roi_rect_.y() + roi_rect_.height(), img.rows);
            roi_x = x1; roi_y = y1;

            // 提取 ROI 子矩阵（不 clone，直接引用原图内存，零拷贝）
            // 注意：推理时内部会处理格式转换，不会修改原图
            detect_img = img(cv::Range(y1, y2), cv::Range(x1, x2));
        } else {
            // 无 ROI，使用全图
            detect_img = img;
        }

        // --- 提交帧到流水线（非阻塞） ---
        // put() 将帧放入 rknnPool 任务队列，立即返回
        if (pipeline->put(detect_img) != 0) break;

        // --- 获取推理结果（阻塞等待 NPU 完成） ---
        // 前 thread_num 帧跳过 get()，让线程池填充任务
        // 之后每帧都 get()，保证 FIFO 顺序
        if (frames >= thread_num_ && pipeline->get(detect_img) != 0) break;

        // --- 处理检测结果 ---
        if (frames >= thread_num_) {
            // 获取最新的检测结果（包含所有检测框）
            detect_result_group_t result = pipeline->getLastDetectResult();

            // --- ROI 坐标还原 ---
            // 检测是在 ROI 子图上进行的，需要将坐标还原到全图
            // 例如：ROI 左上角是 (100,50)，检测框在 ROI 内是 (20,30)
            //       还原后应该是 (120,80)
            if (roi_enabled_ && !roi_rect_.isNull()) {
                for (int i = 0; i < result.count; i++) {
                    result.results[i].box.left += roi_x;
                    result.results[i].box.right += roi_x;
                    result.results[i].box.top += roi_y;
                    result.results[i].box.bottom += roi_y;
                }
                // 将 ROI 区域的检测结果复制回原图对应位置
                detect_img.copyTo(img(cv::Range(roi_y, roi_y + detect_img.rows),
                                      cv::Range(roi_x, roi_x + detect_img.cols)));
            }

            // --- WebSocket 报警处理 ---
            // checkAndAlarm() 遍历检测结果，对匹配 alarm_class_names 的目标
            // 生成报警消息并通过 WebSocket 广播给所有客户端
            if (auto ws = ws_.lock()) {  // weak_ptr 提升为 shared_ptr
                if (ws->isAlarmEnabled()) {
                    int alarm_cnt = ws->checkAndAlarm(&result, frames, detect_img);
                    if (alarm_cnt > 0) {
                        // 统计各报警类别数量
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

            // --- 转换检测结果为 UI 格式 ---
            // detect_result_group_t → QVector<FrameDetections::Det>
            // 通过 Qt 信号槽传递给主线程的 GUI 显示
            QVector<FrameDetections::Det> dets;
            std::vector<std::string> class_names;
            for (int i = 0; i < result.count; i++) {
                const detect_result_t& det = result.results[i];
                // 构造 Det 结构体（类名、置信度、边界框坐标）
                dets.append({QString::fromUtf8(det.name), det.prop,
                             det.box.left, det.box.top, det.box.right, det.box.bottom});
                if (det.name[0] != '\0')
                    class_names.push_back(det.name);
            }
            // 更新各类别检测计数（用于统计面板显示）
            for (const auto& name : class_names)
                class_counts_[name]++;
            // 发送检测结果到主线程（触发 UI 更新）
            if (!dets.isEmpty())
                emit detectionBatch(frames, dets);
        }

        // --- 帧渲染 ---
        // 选择输出帧：有推理结果时用 detect_img（已绘制检测框），否则用原图
        cv::Mat& out_frame = (frames >= thread_num_) ? detect_img : img;

        // 计算推理耗时
        double infer_time = (double)(stats.elapsedMs() - infer_start);
        // 更新实时 FPS（每30帧计算一次）
        double current_fps = stats.updateFps(frames);
        // 在帧左上角绘制 FPS 文字
        renderer.drawFps(out_frame, current_fps);

        // --- 视频录制（可选） ---
        // 如果启用了录制，将带检测框的帧写入视频文件
        if (recorder_ && recorder_->isRecording())
            recorder_->write(out_frame);

        // --- 推送帧到 GUI ---
        // 将帧放入 FrameQueue，GUI 线程定时轮询取出显示
        if (frame_queue_)
            frame_queue_->push(channel_id_, out_frame, current_fps);

        // --- 定期发送统计更新 ---
        if (frames % 5 == 0) {
            // 每5帧更新一次 FPS/帧数/推理时间
            emit statsUpdated(frames, current_fps, infer_time);
            if (frames % 30 == 0) {
                // 每30帧更新一次报警统计和类别统计
                long long total_alarms = 0;
                for (auto& [k, v] : alarm_counts_) total_alarms += v;
                json cls_j;
                for (auto& [k, v] : class_counts_) cls_j[k] = v;
                emit statsPanelUpdated(total_alarms, QString::fromStdString(cls_j.dump()));
            }
        }

        frames++;
    }

    // ========== 阶段3：清理 ==========
    // 排空流水线中剩余的帧（pipeline 可能还有未处理的帧）
    while (true) {
        cv::Mat img;
        if (pipeline->get(img) != 0) break;  // 队列为空，退出
        if (frame_queue_)
            frame_queue_->push(channel_id_, img, stats.getCurrentFps());
    }

    // 发送最终统计信息
    double avg_fps = stats.calcAvgFps(frames);
    emit statsUpdated(frames, avg_fps, 0);
    emit error(QString("[Ch%1] Finished. Frames: %2, Avg FPS: %3")
                   .arg(channel_id_).arg(frames).arg(avg_fps, 0, 'f', 2));
    emit finished();  // 通知主线程：检测完成
}
