/*
 * rknnPool.hpp — RKNN 模型线程池管理器（模板类）
 *
 * 功能：管理多个模型实例，通过线程池实现并发推理，轮询分配任务到不同实例
 *
 * 核心设计：
 *   - 每个模型实例绑定一个 NPU 核心（P2-1 按通道固定分配）
 *   - 第一个实例完整加载模型，后续实例共享权重（rknn_dup_context）
 *   - put() 非阻塞，get() 阻塞等待推理完成
 *   - 队列满时丢弃最旧帧，防止 OOM
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

// rknnModel模型类, inputType模型输入类型, outputType模型输出类型
template <typename rknnModel, typename inputType, typename outputType>
class rknnPool
{
private:
    int threadNum;                  // 线程池中的线程数量（即模型实例数量）
    std::string modelPath;          // 模型文件路径
    int channel_id_ = 0;            // P2-1: 通道编号，用于按通道固定 NPU 核心分配

    long long id;                   // 任务分配计数器，用于轮询选择模型实例
    std::mutex idMtx;               // idMtx: 保护 id 的互斥锁
    std::unique_ptr<dpool::ThreadPool> pool;                   // 线程池实例

    // P1 修复: 使用无锁队列替代 std::queue，减少锁竞争
    struct TaskItem {
        std::future<outputType> fut;
        int model_id;
    };
    lfq::LockFreeQueue<TaskItem> task_queue_;                  // 无锁任务队列

    std::vector<std::shared_ptr<rknnModel>> models;            // 模型实例数组
    int last_model_id_ = -1;                                   // 最近一次 get() 使用的模型 ID
    static constexpr size_t MAX_QUEUE_SIZE = 16;               // 队列最大容量，超过时丢弃旧帧

protected:
    /* 轮询获取模型实例 ID，通过取模运算实现任务的均匀分配 */
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

    /* 析构函数：等待所有剩余推理任务完成后释放资源 */
    ~rknnPool();
};

// 构造函数：初始化模型路径、线程数和任务计数器
template <typename rknnModel, typename inputType, typename outputType>
rknnPool<rknnModel, inputType, outputType>::rknnPool(const std::string &modelPath, int threadNum, int channelId)
{
    this->modelPath = modelPath;
    this->threadNum = threadNum;
    this->id = 0;
    this->channel_id_ = channelId;
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

// 轮询获取模型实例 ID，通过取模运算实现任务的均匀分配
template <typename rknnModel, typename inputType, typename outputType>
int rknnPool<rknnModel, inputType, outputType>::getModelId()
{
    std::lock_guard<std::mutex> lock(idMtx);
    int modelId = id % threadNum;
    id++;
    return modelId;
}

// 提交推理任务到线程池（非阻塞）
// 将输入数据和对应的模型实例绑定后提交到线程池执行，返回的 future 存入队列
// P1 修复: 使用无锁队列，减少锁竞争
template <typename rknnModel, typename inputType, typename outputType>
int rknnPool<rknnModel, inputType, outputType>::put(const inputType &inputData)
{
    // 检查队列是否已满（近似检查，无锁队列的 size_approx 不精确）
    if (task_queue_.size_approx() >= MAX_QUEUE_SIZE)
    {
        // 队列满，丢弃最旧帧
        auto old = task_queue_.pop();
        if (old) {
            // old 的 future 析构会自动清理
        }
    }

    int modelId = this->getModelId();
    auto model = models[modelId];
    inputType img = inputData;

    // 创建任务项
    TaskItem item;
    item.fut = pool->submit([model](inputType img) -> outputType {
        return model->infer(img, nullptr);
    }, std::move(img));
    item.model_id = modelId;

    // 使用无锁队列入队
    task_queue_.push(std::make_shared<TaskItem>(std::move(item)));
    return 0;
}

// 获取最早的推理结果（阻塞等待）
// 按照提交顺序（FIFO）获取结果，如果队列为空则返回 1 表示无结果
// P1 修复: 使用无锁队列，减少锁竞争
template <typename rknnModel, typename inputType, typename outputType>
int rknnPool<rknnModel, inputType, outputType>::get(outputType &outputData)
{
    // 从无锁队列出队
    auto item_opt = task_queue_.pop();
    if (!item_opt) {
        return 1;  // 队列为空
    }

    auto& item = **item_opt;
    // 在无锁环境下等待推理完成
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

// 动态设置所有模型实例的阈值（需 rknnModel 提供 set_thresholds 方法）
template <typename rknnModel, typename inputType, typename outputType>
void rknnPool<rknnModel, inputType, outputType>::set_thresholds(float conf, float nms)
{
    for (auto& model : models) {
        model->setThresholds(conf, nms);
    }
}

// 析构函数：等待所有剩余推理任务完成后释放资源
template <typename rknnModel, typename inputType, typename outputType>
rknnPool<rknnModel, inputType, outputType>::~rknnPool()
{
    // 清空无锁队列中的所有任务
    while (auto item_opt = task_queue_.pop()) {
        // 等待 future 完成
        (*item_opt)->fut.get();
    }
}

#endif
