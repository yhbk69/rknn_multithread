// coreNum.hpp - RKNN 多线程推理核心绑定工具
// 提供 NPU 核心分配功能，支持全局轮询和按通道固定分配两种策略
#ifndef CORENUM_H
#define CORENUM_H

#include <mutex>
#include "rknn_api.h"

// RK3588 NPU 核心数量
inline constexpr int RK3588 = 3;

// 全局轮询分配（旧策略，线程安全）
inline int get_core_num()
{
    static int core_num = 0;
    static std::mutex mtx;

    std::lock_guard<std::mutex> lock(mtx);

    int temp = core_num % RK3588;
    core_num++;
    return temp;
}

// P2-1: 按通道固定核心分配
// 每路通道的实例按 (channel_id + instance_idx) % 3 分配核心，
// 使不同通道的实例均匀分布在 3 个 NPU 核心上，减少核心争抢
inline int get_core_for_channel(int channel_id, int instance_idx = 0) {
    return (channel_id + instance_idx) % RK3588;
}

#endif