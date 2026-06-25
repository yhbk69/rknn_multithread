/*
 * LockFreeQueue.hpp - MPMC 队列
 *
 * 基于 CAS (Compare-And-Swap) 操作实现的多生产者多消费者队列。
 * pop() 使用互斥锁保护关键区，防止并发消费者间的 ABA 竞态
 * （CAS 成功后旧哨兵节点的 shared_ptr 拷贝与 delete 的竞态）。
 *
 * 特点：
 *   - push() 完全无锁
 *   - pop() 使用轻量锁保护（队列最大 16 项，竞争极低）
 *   - 内存安全，使用 shared_ptr 管理数据
 *
 * 使用场景：
 *   - rknnPool 中的任务队列（有界、低竞争）
 *   - CascadePipeline 中的帧队列
 */

#ifndef LOCKFREEQUEUE_H
#define LOCKFREEQUEUE_H

#include <atomic>
#include <memory>
#include <optional>
#include <mutex>

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

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
    mutable std::mutex pop_mtx_;    // 保护 pop()/empty() 关键区

public:
    LockFreeQueue() {
        Node* sentinel = new Node();
        head_.store(sentinel);
        tail_.store(sentinel);
    }

    ~LockFreeQueue() {
        Node* current = head_.load();
        while (current) {
            Node* next = current->next.load();
            delete current;
            current = next;
        }
    }

    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    /*
     * push - 入队操作（多生产者安全，完全无锁）
     */
    void push(std::shared_ptr<T> item) {
        Node* new_node = new Node(std::move(item));

        while (true) {
            Node* tail = tail_.load();
            Node* next = tail->next.load();

            if (tail == tail_.load()) {
                if (next == nullptr) {
                    if (tail->next.compare_exchange_weak(next, new_node)) {
                        tail_.compare_exchange_strong(tail, new_node);
                        return;
                    }
                } else {
                    tail_.compare_exchange_strong(tail, next);
                }
            }
        }
    }

    /*
     * pop - 出队操作（多消费者安全）
     *
     * 使用互斥锁保护：CAS 成功后需要 delete 旧哨兵节点，
     * 但其他线程可能仍在读取该节点的 next->data（shared_ptr 拷贝非原子），
     * 导致 heap-use-after-free。轻量锁消除此竞态。
     */
    std::optional<std::shared_ptr<T>> pop() {
        std::lock_guard<std::mutex> lock(pop_mtx_);

        Node* head = head_.load();
        Node* tail = tail_.load();
        Node* next = head->next.load();

        if (head == tail) {
            if (next == nullptr) {
                return std::nullopt;
            }
            tail_.compare_exchange_strong(tail, next);
            // 重读状态
            head = head_.load();
            tail = tail_.load();
            next = head->next.load();
            if (head == tail) {
                return std::nullopt;
            }
        }

        std::shared_ptr<T> data = next->data;
        head_.store(next);
        delete head;
        return data;
    }

    /*
     * empty - 检查队列是否为空
     * 注意：在并发环境下，返回值可能立即过时
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(pop_mtx_);
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
