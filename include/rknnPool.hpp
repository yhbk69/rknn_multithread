/*
 * rknnPool.hpp
 * RKNN 模型线程池管理器的模板类
 * 负责管理多个模型实例和异步推理任务，通过线程池实现并发推理，
 * 并通过轮询方式分配任务到不同模型实例，达到负载均衡的效果。
 */
#ifndef RKNNPOOL_H
#define RKNNPOOL_H

#include "ThreadPool.hpp"
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

    long long id;                   // 任务分配计数器，用于轮询选择模型实例
    std::mutex idMtx, queueMtx;     // idMtx: 保护 id 的互斥锁; queueMtx: 保护结果队列的互斥锁
    std::unique_ptr<dpool::ThreadPool> pool;                   // 线程池实例
    std::queue<std::future<outputType>> futs;                  // 推理结果的异步任务队列（按提交顺序排列）
    std::vector<std::shared_ptr<rknnModel>> models;            // 模型实例数组

protected:
    int getModelId();  // 轮询获取模型实例 ID，实现负载均衡

public:
    rknnPool(const std::string modelPath, int threadNum);
    int init();  // 创建线程池和模型实例，第一个实例完整加载模型，后续实例共享权重
    // 提交推理任务到线程池（非阻塞）
    int put(inputType inputData);
    // 获取最早的推理结果（阻塞等待）
    int get(outputType &outputData);
    ~rknnPool();  // 析构函数：等待所有剩余推理任务完成后释放资源
};

// 构造函数：初始化模型路径、线程数和任务计数器
template <typename rknnModel, typename inputType, typename outputType>
rknnPool<rknnModel, inputType, outputType>::rknnPool(const std::string modelPath, int threadNum)
{
    this->modelPath = modelPath;
    this->threadNum = threadNum;
    this->id = 0;
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
    for (int i = 0, ret = 0; i < threadNum; i++)
    {
        ret = models[i]->init(models[0]->get_pctx(), i != 0);
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
template <typename rknnModel, typename inputType, typename outputType>
int rknnPool<rknnModel, inputType, outputType>::put(inputType inputData)
{
    std::lock_guard<std::mutex> lock(queueMtx);
    futs.push(pool->submit(&rknnModel::infer, models[this->getModelId()], inputData));
    return 0;
}

// 获取最早的推理结果（阻塞等待）
// 按照提交顺序（FIFO）获取结果，如果队列为空则返回 1 表示无结果
template <typename rknnModel, typename inputType, typename outputType>
int rknnPool<rknnModel, inputType, outputType>::get(outputType &outputData)
{
    std::lock_guard<std::mutex> lock(queueMtx);
    if(futs.empty() == true)
        return 1;
    outputData = futs.front().get();
    futs.pop();
    return 0;
}

// 析构函数：等待所有剩余推理任务完成后释放资源
template <typename rknnModel, typename inputType, typename outputType>
rknnPool<rknnModel, inputType, outputType>::~rknnPool()
{
    while (!futs.empty())
    {
        outputType temp = futs.front().get();
        futs.pop();
    }
}

#endif
