/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef CCE_RUNTIME_UVM_CALLBACK_HPP
#define CCE_RUNTIME_UVM_CALLBACK_HPP

#include "runtime.hpp"
#include "runtime/base.h"
#include "base.hpp"
#include "driver/ascend_hal.h"
#include "runtime/mem.h"
#include "npu_driver.hpp"
#include "api.hpp"

namespace cce {
namespace runtime {
typedef struct MemsetCallbackStruct {
    void *ptr;
    uint64_t destMax;
    uint32_t val;
    uint64_t cnt;
} MemsetCallbackStruct;

typedef struct rtMemcpyCallbackParam {
    void * dst;
    uint64_t destMax;
    const void * src;
    uint64_t cnt;
    rtMemcpyKind_t kind;
    bool checkKind;
    Stream* stm;
} rtMemcpyCallbackParam;

class UvmCallback {
public :
    UvmCallback() = default;
    virtual ~UvmCallback() = default;

    UvmCallback(const UvmCallback &) = delete;
    UvmCallback &operator=(const UvmCallback &) = delete;
    UvmCallback(UvmCallback &&) = delete;
    UvmCallback &operator=(UvmCallback &&) = delete;

    // UVM Callback
    static void MemsetAsyncCallback(void *fnData);
    static void MemcpyAsyncCallback(void *userData);

    static bool IsUvmMem(const void * const ptr, const uint64_t cnt);
    static void CreateMemcpyCallbackParam(void * const dst, const uint64_t destMax, const void * const src, const uint64_t cnt,
    const rtMemcpyKind_t kind, bool checkKind, Stream * const curStm, rtMemcpyCallbackParam* memcpyCallbackParam);
};    
}
}

#endif  // CCE_RUNTIME_UVM_CALLBACK_HPP