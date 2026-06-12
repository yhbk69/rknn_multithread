/*
 * Copyright (C) 2016 Rockchip Electronics Co., Ltd.
 * Authors:
 *    Zhiqin Wei <wzq@rock-chips.com>
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
 
#ifndef _LIBS_RGA_SINGLETON_H
#define _LIBS_RGA_SINGLETON_H

#ifndef ANDROID
#include "RgaMutex.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-var-template"
#endif

/*
 * Singleton<TYPE> - 线程安全的单例模式模板类
 *
 * 使用双重检查锁定（通过 Mutex::Autolock）确保多线程环境下的安全初始化，
 * 首次调用 getInstance() 时创建唯一实例，后续调用返回同一实例的引用。
 */
template <typename TYPE>
class Singleton {
  public:
    /* 获取单例实例的引用（线程安全） */
    static TYPE& getInstance() {
        Mutex::Autolock _l(sLock);
        TYPE* instance = sInstance;
        if (instance == nullptr) {
            instance = new TYPE();
            sInstance = instance;
        }
        return *instance;
    }

    /* 检查单例实例是否已创建 */
    static bool hasInstance() {
        Mutex::Autolock _l(sLock);
        return sInstance != nullptr;
    }

  protected:
    ~Singleton() { }  /* 析构函数（保护） */
    Singleton() { }   /* 构造函数（保护） */

  private:
    Singleton(const Singleton&);                /* 禁止拷贝构造 */
    Singleton& operator = (const Singleton&);    /* 禁止赋值操作 */
    static Mutex sLock;                          /* 保护实例创建的互斥锁 */
    static TYPE* sInstance;                      /* 单例实例指针 */
};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#define RGA_SINGLETON_STATIC_INSTANCE(TYPE)                 \
    template<> ::Mutex  \
        (::Singleton< TYPE >::sLock)(::Mutex::PRIVATE);  \
    template<> TYPE* ::Singleton< TYPE >::sInstance(nullptr);  /* NOLINT */ \
    template class ::Singleton< TYPE >;  /* 显式模板实例化声明 */

#endif //ANDROID
#endif //_LIBS_RGA_SINGLETON_H
