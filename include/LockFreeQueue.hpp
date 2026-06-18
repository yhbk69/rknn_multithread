/*
 * LockFreeQueue.hpp - 无锁 MPMC 队列
 *
 * 基于 CAS (Compare-And-Swap) 操作实现的无锁队列，
 * 支持多生产者多消费者并发访问，适用于高性能场景。
 *
 * 特点：
 *   - 无锁设计，避免线程阻塞
 *   - 支持多线程并发 push/pop
 *   - 内存安全，使用 shared_ptr 管理数据
 *
 * 使用场景：
 *   - 替代 rknnPool 中的 std::queue<cv::Mat>
 *   - 替代 CascadePipeline 中的 std::queue
 */

#ifndef LOCKFREEQUEUE_H
#define LOCKFREEQUEUE_H

#include <atomic>
#include <memory>
#include <optional>

namespace lfq {

template<typename T>
class LockFreeQueue {
private:
    struct Node {
        std::shared_ptr<T> data;
        std::atomic<Node*> next{nullptr};

        Node() = default;
        explicit Node(std::shared_ptr<T> d) : data(std::move(d)) {}
    };

    // 哨兵节点，简化空队列处理
    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;

public:
    LockFreeQueue() {
        Node* sentinel = new Node();
        head_.store(sentinel);
        tail_.store(sentinel);
    }

    ~LockFreeQueue() {
        // 清理所有节点
        Node* current = head_.load();
        while (current) {
            Node* next = current->next.load();
            delete current;
            current = next;
        }
    }

    // 禁用拷贝
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    /*
     * push - 入队操作（多生产者安全）
     * @param item 要入队的数据
     *
     * 实现原理：
     * 1. 创建新节点
     * 2. CAS 循环将新节点链接到 tail_
     * 3. 更新 tail_ 指针
     */
    void push(std::shared_ptr<T> item) {
        Node* new_node = new Node(std::move(item));

        while (true) {
            Node* tail = tail_.load();
            Node* next = tail->next.load();

            // 检查 tail 是否仍然是队尾
            if (tail == tail_.load()) {
                if (next == nullptr) {
                    // tail 确实指向队尾，尝试链接新节点
                    if (tail->next.compare_exchange_weak(next, new_node)) {
                        // 链接成功，尝试更新 tail
                        tail_.compare_exchange_strong(tail, new_node);
                        return;
                    }
                } else {
                    // tail 落后了，帮助更新
                    tail_.compare_exchange_strong(tail, next);
                }
            }
        }
    }

    /*
     * pop - 出队操作（多消费者安全）
     * @return 出队的数据，队列为空返回 std::nullopt
     *
     * 实现原理：
     * 1. 读取 head_ 和 tail_
     * 2. 如果队列为空，返回 nullopt
     * 3. CAS 循环更新 head_ 指针
     * 4. 返回数据
     */
    std::optional<std::shared_ptr<T>> pop() {
        while (true) {
            Node* head = head_.load();
            Node* tail = tail_.load();
            Node* next = head->next.load();

            // 检查 head 是否仍然是队首
            if (head == head_.load()) {
                if (head == tail) {
                    // 队列可能为空
                    if (next == nullptr) {
                        // 确认队列为空
                        return std::nullopt;
                    }
                    // tail 落后了，帮助更新
                    tail_.compare_exchange_strong(tail, next);
                } else {
                    // 队列非空，读取数据
                    std::shared_ptr<T> data = next->data;
                    // 尝试更新 head
                    if (head_.compare_exchange_weak(head, next)) {
                        delete head;  // 删除旧的哨兵节点
                        return data;
                    }
                }
            }
        }
    }

    /*
     * empty - 检查队列是否为空
     * 注意：在并发环境下，返回值可能立即过时
     */
    bool empty() const {
        Node* head = head_.load();
        Node* tail = tail_.load();
        return (head == tail) && (head->next.load() == nullptr);
    }

    /*
     * size_approx - 近似队列大小（用于监控）
     * 注意：在并发环境下，返回值可能不精确
     */
    size_t size_approx() const {
        Node* head = head_.load();
        Node* tail = tail_.load();
        size_t count = 0;
        Node* current = head->next.load();
        while (current && current != tail->next.load()) {
            count++;
            current = current->next.load();
        }
        return count;
    }
};

} // namespace lfq

#endif // LOCKFREEQUEUE_H
