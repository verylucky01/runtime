/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "davinci_kernel_task.h"
#include "stream.hpp"
#include "runtime.hpp"
#include "rt_stars_define.h"
#include "context.hpp"
#include "event.hpp"
#include "notify.hpp"
#include "context.hpp"
#include "stars.hpp"
#include "stub_task.hpp"
#include "device.hpp"
#include "task_execute_time.h"

namespace cce {
namespace runtime {
#if F_DESC("DavinciKernelTask")

void ShowDavinciTaskMixDebug(const rtFftsPlusMixAicAivCtx_t * const fftsCtx)
{
    if (CheckLogLevel(static_cast<int32_t>(RUNTIME), DLOG_INFO) == 0) {
        return;
    }

    const uint32_t * const buf = RtPtrToPtr<const uint32_t *>(fftsCtx);
    RT_LOG(RT_LOG_INFO, "The DavinciTask Mix context-buf:"
        "%#010x, %#010x, %#010x, %#010x, %#010x, %#010x, %#010x, %#010x, "
        "%#010x, %#010x, %#010x, %#010x, %#010x, %#010x, %#010x, %#010x, "
        "%#010x, %#010x, %#010x, %#010x, %#010x, %#010x, %#010x, %#010x, "
        "%#010x, %#010x, %#010x, %#010x, %#010x, %#010x, %#010x, %#010x, ",
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
        buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15],
        buf[16], buf[17], buf[18], buf[19], buf[20], buf[21], buf[22], buf[23],
        buf[24], buf[25], buf[26], buf[27], buf[28], buf[29], buf[30], buf[31]);

    return;
}

void FillFftsPlusMixSqeSubtask(const AicTaskInfo *taskInfo, uint8_t *const subtype)
{
    *subtype = 0U;
    switch (taskInfo->kernel->GetMixType()) {
        case MIX_AIC:
            *subtype = RT_CTX_TYPE_MIX_AIC;
            break;
        case MIX_AIV:
            *subtype = RT_CTX_TYPE_MIX_AIV;
            break;
        case MIX_AIC_AIV_MAIN_AIC:
            *subtype = RT_CTX_TYPE_MIX_AIC;
            break;
        case MIX_AIC_AIV_MAIN_AIV:
            *subtype = RT_CTX_TYPE_MIX_AIV;
            break;
        default:
            break;
    }
    return;
}

void FillFftsMixSqeForDavinciTask(TaskInfo* taskInfo, rtStarsSqe_t *const command, uint32_t minStackSize, rtError_t copyRet)
{
    rtFftsPlusSqe_t *sqe = &(command->fftsPlusSqe);
    AicTaskInfo *aicTaskInfo = &(taskInfo->u.aicTaskInfo);
    Stream * const stm = taskInfo->stream;
    Device *dev = stm->Device_();
    uint64_t stackSize = KERNEL_STACK_SIZE_32K;
    uint64_t stackPhyBase = RtPtrToValue(dev->GetStackPhyBase32k());
    Program *programPtr = nullptr;
    const Kernel *kernelPtr = aicTaskInfo->kernel;
    if (kernelPtr != nullptr) {
        programPtr = kernelPtr->Program_();
        if (programPtr != nullptr) {
            stackSize = programPtr->GetStackSize();
            RT_LOG(
                RT_LOG_INFO, "kernelNames_=%s, stackSize=%lu.", programPtr->GetKernelNamesBuffer().c_str(), stackSize);
        }
    }

    if (likely(minStackSize == 0)) {
        if (stackSize == KERNEL_STACK_SIZE_16K) {
            stackPhyBase = RtPtrToValue(dev->GetStackPhyBase16k());
        }
        RT_LOG(RT_LOG_INFO, "stackSize=%uByte, stackPhyBase=%#llx", stackSize, stackPhyBase);
    } else {
        if (minStackSize <= KERNEL_STACK_SIZE_32K) {
            stackPhyBase = RtPtrToValue(dev->GetStackPhyBase32k());
            stackSize = KERNEL_STACK_SIZE_32K;
        } else {
            stackPhyBase = RtPtrToValue(dev->GetCustomerStackPhyBase());
            stackSize = Runtime::Instance()->GetDeviceCustomerStackSize();
        }
        RT_LOG(RT_LOG_INFO,
            "minStackSize=%uByte, stackSize=%uByte, stackPhyBase=%#llx",
            minStackSize,
            stackSize,
            stackPhyBase);
    }

    for (size_t i = 0LU; i < (sizeof(sqe->reserved16) / sizeof(sqe->reserved16[0])); i++) {
        sqe->reserved16[i] = 0U;
    }
    rtStarsFftsPlusHeader_t *sqeHeader = RtPtrToPtr<rtStarsFftsPlusHeader_t *>(&(sqe->sqeHeader));
    sqeHeader->type = 0U;
    // if h2d copy fail, change sqe type to 63 STARS_SQE_TYPE_INVALID
    if (copyRet != RT_ERROR_NONE) {
        sqeHeader->type = 63U;
    }
    sqeHeader->ie = 0U;
    sqeHeader->preP = 0U;
    sqeHeader->postP = 0U;
    if ((aicTaskInfo->comm.kernelFlag & RT_KERNEL_DUMPFLAG) != 0U) {
        if (stm->IsOverflowEnable()) {
            sqeHeader->preP = RT_STARS_SQE_INT_DIR_TO_TSCPU;
        }
        sqeHeader->postP = RT_STARS_SQE_INT_DIR_TO_TSCPU;
        sqe->reserved16[1U] = sqe->reserved16[1U] | SQE_BIZ_FLAG_DATADUMP;
    }
    if ((stm->IsDebugRegister() && (!stm->GetBindFlag()))) {
        sqeHeader->postP = RT_STARS_SQE_INT_DIR_TO_TSCPU;
    }
    sqeHeader->wrCqe = stm->GetStarsWrCqeFlag();

    sqeHeader->rtStreamId = static_cast<uint16_t>(stm->Id_());
    sqeHeader->taskId = taskInfo->id;
    sqeHeader->overflowEn = stm->IsOverflowEnable();
    sqeHeader->blockDim = aicTaskInfo->comm.dim;
    sqe->fftsType = RT_FFTS_PLUS_TYPE;
    sqe->kernelCredit = static_cast<uint8_t>(GetAicoreKernelCredit(aicTaskInfo->timeout));
    FillFftsPlusMixSqeSubtask(aicTaskInfo, &sqe->subType);
    const Stream *dsaStm = stm->Device_()->TsFFtsDsaStream_();

    uint16_t dsaSqId = 0U;
    if (dsaStm != nullptr) {
        dsaSqId = static_cast<uint16_t>(dsaStm->GetSqId());
    }

    sqe->dsaSqId = dsaSqId;
    sqe->totalContextNum = 1U;
    sqe->readyContextNum = 1U;
    sqe->preloadContextNum = 1U;
    sqe->stackPhyBaseL = static_cast<uint32_t>(stackPhyBase);
    sqe->stackPhyBaseH = static_cast<uint32_t>(stackPhyBase >> UINT32_BIT_NUM);
    const uint64_t devMemAddr = RtPtrToValue(aicTaskInfo->descAlignBuf);
    sqe->contextAddressBaseL = static_cast<uint32_t>(devMemAddr);
    sqe->contextAddressBaseH =
        (static_cast<uint32_t>(devMemAddr >> UINT32_BIT_NUM)) & MASK_17_BIT;

    if (Runtime::Instance()->GetL2CacheProfFlag()) {
        sqeHeader->postP = RT_STARS_SQE_INT_DIR_TO_TSCPU;
        sqe->reserved16[1U] = sqe->reserved16[1U] | SQE_BIZ_FLAG_L2CACHE;
    }

    if ((!stm->GetBindFlag()) && (Runtime::Instance()->GetBiuperfProfFlag())) {
        if (sqeHeader->postP == RT_STARS_SQE_INT_DIR_TO_TSCPU) {
            RT_LOG(RT_LOG_WARNING, "post-p has already be set, service scenarios conflict.");
        } else {
            sqeHeader->preP = RT_STARS_SQE_INT_DIR_TO_TSCPU;
            sqeHeader->postP = RT_STARS_SQE_INT_DIR_TO_TSCPU;
            sqe->reserved16[1U] = sqe->reserved16[1U] | SQE_BIZ_FLAG_BIUPERF;
        }
    }

    if (programPtr != nullptr && programPtr->IsDcacheLockOp()) {
        sqeHeader->postP = RT_STARS_SQE_INT_DIR_TO_TSCPU;
        sqe->subType = RT_STARS_DCACHE_LOCK_OP;
    }

    RT_LOG(RT_LOG_INFO, "kernelFlag=%d, l2CacheProfFlag=%d, bindFlag=%d, biuperfProfFlag=%d, postP=%u, subType=0x%x, "
        "kernelCredit=%u",
        aicTaskInfo->comm.kernelFlag, Runtime::Instance()->GetL2CacheProfFlag(), stm->GetBindFlag(),
        Runtime::Instance()->GetBiuperfProfFlag(), sqeHeader->postP, sqe->subType, sqe->kernelCredit);

    PrintSqe(command, "MixTask");
    return;
}

#endif

}  // namespace runtime
}  // namespace cce
