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
 *   - 内部通过 queueMtx 保护任务队列
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
#include <vector>
#include <iostream>
#include <mutex>
#include <queue>
#include <memory>

// rknnModel模型类, inputType模型输入类型, outputType模型输出类型
template <typename rknnModel, typename inputType, typename outputType>
class rknnPool
{
private:
    int threadNum;                  // 线程池中的线程数量（即模型实例数量）
    std::string modelPath;          // 模型文件路径
    int channel_id_ = 0;            // P2-1: 通道编号，用于按通道固定 NPU 核心分配

    long long id;                   // 任务分配计数器，用于轮询选择模型实例
    std::mutex idMtx, queueMtx;     // idMtx: 保护 id 的互斥锁; queueMtx: 保护结果队列的互斥锁
    std::unique_ptr<dpool::ThreadPool> pool;                   // 线程池实例
    std::queue<std::future<outputType>> futs;                  // 推理结果的异步任务队列（按提交顺序排列）
    std::queue<int> model_ids_;                                // 每个任务对应的模型 ID
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
// 队列满时丢弃最旧的未处理帧，防止内存无限增长
template <typename rknnModel, typename inputType, typename outputType>
int rknnPool<rknnModel, inputType, outputType>::put(const inputType &inputData)
{
    std::lock_guard<std::mutex> lock(queueMtx);
    if (futs.size() >= MAX_QUEUE_SIZE)
    {
        // 队列满，直接丢弃最旧帧（future 来自 packaged_task，析构不阻塞）
        futs.pop();
        model_ids_.pop();
    }
    int modelId = this->getModelId();
    auto model = models[modelId];
    inputType img = inputData;
    futs.push(pool->submit([model](inputType img) -> outputType {
        return model->infer(img, nullptr);
    }, std::move(img)));
    model_ids_.push(modelId);
    return 0;
}

// 获取最早的推理结果（阻塞等待）
// 按照提交顺序（FIFO）获取结果，如果队列为空则返回 1 表示无结果
template <typename rknnModel, typename inputType, typename outputType>
int rknnPool<rknnModel, inputType, outputType>::get(outputType &outputData)
{
    std::future<outputType> fut;
    int model_id;
    {
        std::lock_guard<std::mutex> lock(queueMtx);
        if(futs.empty() == true)
            return 1;
        fut = std::move(futs.front());
        futs.pop();
        model_id = model_ids_.front();
        model_ids_.pop();
    }
    // 在锁外等待推理完成，不阻塞 put()
    outputData = fut.get();
    last_model_id_ = model_id;
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
    // P0 修复: 添加锁保护，防止与 put() 并发时的竞态条件
    std::lock_guard<std::mutex> lock(queueMtx);
    while (!futs.empty())
    {
        outputType temp = futs.front().get();
        futs.pop();
        model_ids_.pop();
    }
}

#endif
