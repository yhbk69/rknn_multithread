/*
 * Copyright (C) 2020 Rockchip Electronics Co., Ltd.
 * Authors:
 *  PutinLee <putin.lee@rock-chips.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef _LIBS_RGA_MUTEX_H
#define _LIBS_RGA_MUTEX_H

#ifndef ANDROID
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#include <pthread.h>


// 仅在使用 clang 编译器时启用线程安全注解。
// 使用其他编译器编译时，这些注解会被安全地移除。
#if defined(__clang__) && (!defined(SWIG))
#define THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
#define THREAD_ANNOTATION_ATTRIBUTE__(x)  // 空操作（非 clang 编译器时无效）
#endif

#define CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(capability(x))

#define SCOPED_CAPABILITY THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

#define GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

#define PT_GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))

#define ACQUIRED_BEFORE(...) THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))

#define ACQUIRED_AFTER(...) THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))

#define REQUIRES(...) THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))

#define REQUIRES_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(requires_shared_capability(__VA_ARGS__))

#define ACQUIRE(...) THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(__VA_ARGS__))

#define ACQUIRE_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(acquire_shared_capability(__VA_ARGS__))

#define RELEASE(...) THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))

#define RELEASE_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(release_shared_capability(__VA_ARGS__))

#define TRY_ACQUIRE(...) THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_capability(__VA_ARGS__))

#define TRY_ACQUIRE_SHARED(...) \
    THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_shared_capability(__VA_ARGS__))

#define EXCLUDES(...) THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

#define ASSERT_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(assert_capability(x))

#define ASSERT_SHARED_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_capability(x))

#define RETURN_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

#define NO_THREAD_SAFETY_ANALYSIS THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

class Condition;

/*
 * 注意：此类最初为 Win32 构建的代码设计。对于不在 Win32 上构建的代码，
 * 建议使用 std::mutex 和 std::lock_guard 代替。
 *
 * 简单互斥锁类，基于 pthread_mutex_t 实现，实现方式依赖于系统。
 *
 * 互斥锁必须由锁定它的线程解锁。该锁不可重入，
 * 即同一线程不能多次锁定它。
 */
class CAPABILITY("mutex") Mutex {
  public:
    enum {
        PRIVATE = 0,
        SHARED = 1
    };

    Mutex();
    explicit Mutex(const char* name);
    explicit Mutex(int type, const char* name = nullptr);
    ~Mutex();

    // 锁定或解锁互斥锁
    int32_t lock() ACQUIRE();
    void unlock() RELEASE();

    // 尝试锁定；成功返回 0，否则返回错误
    int32_t tryLock() TRY_ACQUIRE(0);

    int32_t timedLock(int64_t timeoutNs) TRY_ACQUIRE(0);

    // 自动管理互斥锁。Autolock 构造时自动锁定，离开作用域时自动释放。
    class SCOPED_CAPABILITY Autolock {
      public:
        inline explicit Autolock(Mutex& mutex) ACQUIRE(mutex) : mLock(mutex) {
            mLock.lock();
        }
        inline explicit Autolock(Mutex* mutex) ACQUIRE(mutex) : mLock(*mutex) {
            mLock.lock();
        }
        inline ~Autolock() RELEASE() {
            mLock.unlock();
        }

      private:
        Mutex& mLock;
        // 不可复制或移动 — 仅声明
        Autolock(const Autolock&);
        Autolock& operator=(const Autolock&);
    };

  private:
    friend class Condition;

    // 互斥锁不可复制
    Mutex(const Mutex&);
    Mutex& operator=(const Mutex&);

    pthread_mutex_t mMutex;
};

// ---------------------------------------------------------------------------
inline Mutex::Mutex() {
    pthread_mutex_init(&mMutex, nullptr);
}
inline Mutex::Mutex(__attribute__((unused)) const char* name) {
    pthread_mutex_init(&mMutex, nullptr);
}
inline Mutex::Mutex(int type, __attribute__((unused)) const char* name) {
    if (type == SHARED) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&mMutex, &attr);
        pthread_mutexattr_destroy(&attr);
    } else {
        pthread_mutex_init(&mMutex, nullptr);
    }
}
inline Mutex::~Mutex() {
    pthread_mutex_destroy(&mMutex);
}
inline int32_t Mutex::lock() {
    return -pthread_mutex_lock(&mMutex);
}
inline void Mutex::unlock() {
    pthread_mutex_unlock(&mMutex);
}
inline int32_t Mutex::tryLock() {
    return -pthread_mutex_trylock(&mMutex);
}
inline int32_t Mutex::timedLock(int64_t timeoutNs) {
    timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    timeoutNs += now.tv_sec*1000000000 + now.tv_nsec;
    const struct timespec ts = {
        /* .tv_sec = */ static_cast<time_t>(timeoutNs / 1000000000),
        /* .tv_nsec = */ static_cast<long>(timeoutNs % 1000000000),
    };
    return -pthread_mutex_timedlock(&mMutex, &ts);
}

// ---------------------------------------------------------------------------

/*
 * 自动互斥锁。在函数顶部声明一个 AutoMutex 实例，
 * 当函数返回时，AutoMutex 离开作用域并自动释放锁。
 */

typedef Mutex::Autolock AutoMutex;
#endif // __ANDROID_VNDK__
#endif // _LIBS_RGA_MUTEX_H
