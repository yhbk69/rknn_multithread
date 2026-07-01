/*
 * rknnPool.hpp — RKNN 模型线程池管理器（模板类）
 *
 * 功能：管理多个模型实例，通过线程池实现并发推理，按负载选择最优实例
 *
 * 核心设计：
 *   - 每个模型实例绑定一个 NPU 核心（P2-1 按通道固定分配）
 *   - 第一个实例完整加载模型，后续实例共享权重（rknn_dup_context）
 *   - put() 非阻塞，get() 阻塞等待推理完成
 *   - 队列满时丢弃最旧帧，防止 OOM
 *   - P2-2: 动态负载均衡，基于历史推理时间选择最优实例
 *
 * 线程安全：
 *   - put() 和 get() 可从不同线程调用
 *   - P1 修复: 使用无锁队列替代 std::queue，减少锁竞争
 *
 * 模板参数：
 *   - rknnModel: 模型类（如 YOLOv5Engine），需提供 infer()、rknn_init()、get_pctx()
 *   - inputType: 输入类型（通常为 cv::Mat）
 *   - outputType: 输出类型（通常为 cv::Mat）
 */
#ifndef RKNNPOOL_H
#define RKNNPOOL_H

#include "ThreadPool.hpp"
#include "coreNum.hpp"
#include "LockFreeQueue.hpp"
#include <vector>
#include <iostream>
#include <mutex>
#include <queue>
#include <memory>
#include <future>
#include <atomic>
#include <chrono>
#include <numeric>

// rknnModel模型类, inputType模型输入类型, outputType模型输出类型
template <typename rknnModel, typename inputType, typename outputType>
class rknnPool
{
private:
    int threadNum;                  // 线程池中的线程数量（即模型实例数量）
    std::string modelPath;          // 模型文件路径
    int channel_id_ = 0;            // P2-1: 通道编号，用于按通道固定 NPU 核心分配

    long long id;                   // 任务分配计数器
    std::mutex idMtx;               // idMtx: 保护 id 的互斥锁
    std::unique_ptr<dpool::ThreadPool> pool;                   // 线程池实例

    struct TaskItem {
        std::future<outputType> fut;
        int model_id;
        double infer_ms = 0;        // P2-2: 本次推理耗时（ms）
    };
    lfq::LockFreeQueue<TaskItem> task_queue_;                  // 任务队列

    std::vector<std::shared_ptr<rknnModel>> models;            // 模型实例数组
    int last_model_id_ = -1;                                   // 最近一次 get() 使用的模型 ID
    static constexpr size_t MAX_QUEUE_SIZE = 16;               // 队列最大容量，超过时丢弃旧帧

    // P2-2: 动态负载均衡 — 每个实例的指数移动平均推理时间
    static constexpr double EMA_ALPHA = 0.3;                   // EMA 平滑系数（越大越偏向近期）
    std::vector<std::atomic<double>> avg_latency_;             // 各实例平均推理时间（ms）
    std::vector<std::atomic<int>> task_count_;                 // 各实例累计任务数（用于冷启动）

protected:
    /* P2-2: 选择平均推理时间最短的模型实例（动态负载均衡） */
    int getModelId();

public:
    rknnPool(const std::string &modelPath, int threadNum, int channelId = 0);

    /* 创建线程池和模型实例，第一个实例完整加载模型，后续实例共享权重 */
    int init();

    /* 提交推理任务到线程池（非阻塞），队列满时丢弃最旧帧 */
    int put(const inputType &inputData);

    /* 获取最早的推理结果（阻塞等待），按 FIFO 顺序返回 */
    int get(outputType &outputData);

    /* 获取最近一次 get() 对应模型的检测结果（线程安全，返回拷贝） */
    detect_result_group_t getLastDetectResult() const;

    /* 动态设置所有模型实例的阈值（需 rknnModel 提供 set_thresholds 方法） */
    void set_thresholds(float conf, float nms);

    /* P2-2: 获取各实例的平均推理时间（用于监控/调试） */
    std::vector<double> getAvgLatencies() const;

    /* 获取待处理任务数（用于跳帧决策） */
    int pendingCount() const { return (int)task_queue_.size_approx(); }

    /* 析构函数：等待所有剩余推理任务完成后释放资源 */
    ~rknnPool();
};

// 构造函数：初始化模型路径、线程数和任务计数器
template <typename rknnModel, typename inputType, typename outputType>
rknnPool<rknnModel, inputType, outputType>::rknnPool(const std::string &modelPath, int threadNum, int channelId)
    : avg_latency_(threadNum), task_count_(threadNum)
{
    this->modelPath = modelPath;
    this->threadNum = threadNum;
    this->id = 0;
    this->channel_id_ = channelId;
    // 初始化各实例平均延迟为较大值，冷启动时均匀分配
    for (int i = 0; i < threadNum; i++) {
        avg_latency_[i].store(1000.0);  // 1000ms 初始值，冷启动后快速收敛
        task_count_[i].store(0);
    }
}

// 初始化方法：创建线程池和模型实例
// 第一个模型实例（i=0）完整加载模型权重，后续实例（i>0）共享第一个实例的权重以节省内存
template <typename rknnModel, typename inputType, typename outputType>
int rknnPool<rknnModel, inputType, outputType>::init()
{
    try
    {
        this->pool = std::make_unique<dpool::ThreadPool>(this->threadNum);
        for (int i = 0; i < this->threadNum; i++)
            models.push_back(std::make_shared<rknnModel>(this->modelPath.c_str()));
    }
    catch (const std::bad_alloc &e)
    {
        std::cout << "Out of memory: " << e.what() << std::endl;
        return -1;
    }
    // 初始化模型，第一个实例完整加载，后续实例共享权重
    // P2-1: 按 (channel_id + instance_idx) % 3 分配 NPU 核心
    for (int i = 0, ret = 0; i < threadNum; i++)
    {
        int core = get_core_for_channel(channel_id_, i);
        ret = models[i]->rknn_init(models[0]->get_pctx(), i != 0, core);
        if (ret != 0)
            return ret;
    }

    return 0;
}

// P2-2: 动态负载均衡 — 选择平均推理时间最短的实例
// 冷启动时（任务数 < threadNum），轮询分配以收集各核心的基准性能
template <typename rknnModel, typename inputType, typename outputType>
int rknnPool<rknnModel, inputType, outputType>::getModelId()
{
    std::lock_guard<std::mutex> lock(idMtx);

    // 冷启动阶段：轮询分配，确保每个实例至少执行一次
    int min_tasks = task_count_[0].load();
    for (int i = 1; i < threadNum; i++) {
        int cnt = task_count_[i].load();
        if (cnt < min_tasks) min_tasks = cnt;
    }
    if (min_tasks == 0) {
        // 有实例尚未执行过，选择任务数最少的
        int best = 0;
        for (int i = 1; i < threadNum; i++) {
            if (task_count_[i].load() < task_count_[best].load())
                best = i;
        }
        return best;
    }

    // 稳态阶段：选择平均推理时间最短的实例
    int best = 0;
    double best_lat = avg_latency_[0].load();
    for (int i = 1; i < threadNum; i++) {
        double lat = avg_latency_[i].load();
        if (lat < best_lat) {
            best_lat = lat;
            best = i;
        }
    }
    return best;
}

// 提交推理任务到线程池（非阻塞）
// 将输入数据和对应的模型实例绑定后提交到线程池执行，返回的 future 存入队列
template <typename rknnModel, typename inputType, typename outputType>
int rknnPool<rknnModel, inputType, outputType>::put(const inputType &inputData)
{
    // 检查队列是否已满
    if (task_queue_.size_approx() >= MAX_QUEUE_SIZE)
    {
        auto old = task_queue_.pop();
    }

    int modelId = this->getModelId();
    auto model = models[modelId];
    inputType img = inputData;

    // P2-2: 在 lambda 中测量推理时间
    TaskItem item;
    item.fut = pool->submit([model, modelId, this](inputType img) -> outputType {
        auto t0 = std::chrono::steady_clock::now();
        outputType result = model->infer(img, nullptr);
        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        // 更新该实例的指数移动平均推理时间
        double old_lat = this->avg_latency_[modelId].load();
        double new_lat = EMA_ALPHA * ms + (1.0 - EMA_ALPHA) * old_lat;
        this->avg_latency_[modelId].store(new_lat);
        this->task_count_[modelId].fetch_add(1);

        return result;
    }, std::move(img));
    item.model_id = modelId;

    task_queue_.push(std::make_shared<TaskItem>(std::move(item)));
    return 0;
}

// 获取最早的推理结果（阻塞等待）
// 按照提交顺序（FIFO）获取结果，如果队列为空则返回 1 表示无结果
template <typename rknnModel, typename inputType, typename outputType>
int rknnPool<rknnModel, inputType, outputType>::get(outputType &outputData)
{
    auto item_opt = task_queue_.pop();
    if (!item_opt) {
        return 1;
    }

    auto& item = **item_opt;
    outputData = item.fut.get();
    last_model_id_ = item.model_id;
    return 0;
}

// 获取最近一次 get() 对应模型的检测结果
template <typename rknnModel, typename inputType, typename outputType>
detect_result_group_t rknnPool<rknnModel, inputType, outputType>::getLastDetectResult() const
{
    return models[last_model_id_]->getLastDetectResult();
}

// 动态设置所有模型实例的阈值
template <typename rknnModel, typename inputType, typename outputType>
void rknnPool<rknnModel, inputType, outputType>::set_thresholds(float conf, float nms)
{
    for (auto& model : models) {
        model->setThresholds(conf, nms);
    }
}

// P2-2: 获取各实例的平均推理时间
template <typename rknnModel, typename inputType, typename outputType>
std::vector<double> rknnPool<rknnModel, inputType, outputType>::getAvgLatencies() const
{
    std::vector<double> result(threadNum);
    for (int i = 0; i < threadNum; i++)
        result[i] = avg_latency_[i].load();
    return result;
}

// 析构函数：等待所有剩余推理任务完成后释放资源
template <typename rknnModel, typename inputType, typename outputType>
rknnPool<rknnModel, inputType, outputType>::~rknnPool()
{
    while (auto item_opt = task_queue_.pop()) {
        (*item_opt)->fut.get();
    }
}

#endif
