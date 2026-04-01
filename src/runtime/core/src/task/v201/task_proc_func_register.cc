/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "task_info.hpp"
#include "task_manager_david.h"
#include "stars_david.hpp"
#include "dqs_task_info.hpp"
#include "task_info.h"
#include "task_manager.h"

namespace cce {
namespace runtime {

// sample
static void UpdateTaskToSqeFunc(void)
{
    PfnTaskToDavidSqe *v201Func = GetDavidSqeFuncAddr();
    v201Func[TS_TASK_TYPE_DQS_MBUF_FREE] = &ConstructSqeForDqsMbufFreeTask;
    v201Func[TS_TASK_TYPE_DQS_ENQUEUE] = &ConstructSqeForDqsEnqueueTask;
    v201Func[TS_TASK_TYPE_DQS_DEQUEUE] = &ConstructSqeForDqsDequeueTask;
    v201Func[TS_TASK_TYPE_DQS_ZERO_COPY] = &ConstructSqeForDqsZeroCopyTask;
    v201Func[TS_TASK_TYPE_DQS_SCHED_END] = &ConstructSqeForDqsSchedEndTask;
    v201Func[TS_TASK_TYPE_DQS_PREPARE] = &ConstructSqeForDqsPrepareTask;
    v201Func[TS_TASK_TYPE_DQS_INTER_CHIP_PREPROC] = &ConstructSqeForDqsInterChipPreProcTask;
    v201Func[TS_TASK_TYPE_DQS_INTER_CHIP_POSTPROC] = &ConstructSqeForDqsInterChipPostProcTask;
    v201Func[TS_TASK_TYPE_DQS_ADSPC] = &ConstructSqeForDqsAdspcTask;
    v201Func[TS_TASK_TYPE_DQS_BATCH_DEQUEUE] = &ConstructSqeForDqsBatchDequeueTask;
    v201Func[TS_TASK_TYPE_DQS_CONDITION_COPY] = &ConstructSqeForDqsConditionCopyTask;
    v201Func[TS_TASK_TYPE_DQS_FRAME_ALIGN] = &ConstructSqeForDqsFrameAlignTask;

    return;
}

static void UpdateDoCompleteSuccFunc(const std::vector<rtChipType_t> &chipTypes)
{
    PfnDoCompleteSucc *doCompleteSuccFunc = nullptr;
    for (auto chipType : chipTypes) {
        doCompleteSuccFunc = g_taskFuncArrays[chipType].doCompleteSuccFunc;

        doCompleteSuccFunc[TS_TASK_TYPE_DQS_PREPARE] = &DoCompleteSuccess;
        doCompleteSuccFunc[TS_TASK_TYPE_DQS_ADSPC] = &DoCompleteSuccess;
        doCompleteSuccFunc[TS_TASK_TYPE_DQS_BATCH_DEQUEUE] = &DoCompleteSuccess;
    }

    return;
}

static void UpdatePrintErrorInfoFunc(const std::vector<rtChipType_t> &chipTypes)
{
    PfnPrintErrorInfo *printErrorInfoFunc = nullptr;
    for (auto chipType : chipTypes) {
        printErrorInfoFunc = g_taskFuncArrays[chipType].printErrorInfoFunc;

        printErrorInfoFunc[TS_TASK_TYPE_DQS_MBUF_FREE] = &PrintErrorInfoForDqsMbufFreeTask;
        printErrorInfoFunc[TS_TASK_TYPE_DQS_PREPARE] = &PrintErrorInfoForDqsPrepareTask;
        printErrorInfoFunc[TS_TASK_TYPE_DQS_ZERO_COPY] = &PrintErrorInfoForDqsZeroCopyTask;
        printErrorInfoFunc[TS_TASK_TYPE_DQS_INTER_CHIP_PREPROC] = &PrintErrorInfoForDqsInterChipPreProcTask;
        printErrorInfoFunc[TS_TASK_TYPE_DQS_INTER_CHIP_POSTPROC] = &PrintErrorInfoForDqsInterChipPostProcTask;
        printErrorInfoFunc[TS_TASK_TYPE_DQS_ADSPC] = &PrintErrorInfoForDqsAdspcTask;
        printErrorInfoFunc[TS_TASK_TYPE_DQS_BATCH_DEQUEUE] = &PrintErrorInfoForDqsBatchDequeueTask;
        printErrorInfoFunc[TS_TASK_TYPE_DQS_CONDITION_COPY] = &PrintErrorInfoForDqsConditionCopyTask;
    }

    return;
}

static void UpdateTaskUninitFunc(const std::vector<rtChipType_t> &chipTypes)
{
    PfnTaskUnInit *taskUnInitFunc = nullptr;
    for (auto chipType : chipTypes) {
        taskUnInitFunc = g_taskFuncArrays[chipType].taskUnInitFunc;

        taskUnInitFunc[TS_TASK_TYPE_DQS_MBUF_FREE] = &DqsMbufFreeTaskUnInit;
        taskUnInitFunc[TS_TASK_TYPE_DQS_PREPARE] = &DqsPrepareTaskUnInit;
        taskUnInitFunc[TS_TASK_TYPE_DQS_ZERO_COPY] = &DqsZeroCopyTaskUnInit;
        taskUnInitFunc[TS_TASK_TYPE_DQS_INTER_CHIP_PREPROC] = &DqsInterChipPreProcTaskUnInit;
        taskUnInitFunc[TS_TASK_TYPE_DQS_INTER_CHIP_POSTPROC] = &DqsInterChipPostProcTaskUnInit;
        taskUnInitFunc[TS_TASK_TYPE_DQS_ADSPC] = &DqsAdspcTaskUnInit;
        taskUnInitFunc[TS_TASK_TYPE_DQS_BATCH_DEQUEUE] = &DqsBatchDequeTaskUnInit;
        taskUnInitFunc[TS_TASK_TYPE_DQS_CONDITION_COPY] = &DqsConditionCopyTaskUnInit;
        taskUnInitFunc[TS_TASK_TYPE_DQS_FRAME_ALIGN] = &DqsFrameAlignTaskUnInit;
    }

    return;
}

void RegTaskFunc(void)
{
    static const std::vector<rtChipType_t> chipTypes = {
        CHIP_MC62CM12A
    };
    
    RegDavidTaskFunc();

    // 更新差异化的ToSqe钩子函数
    UpdateTaskToSqeFunc();
    // 更新差异化complete钩子函数
    UpdateDoCompleteSuccFunc(chipTypes);
    // 更新差异化error info钩子函数
    UpdatePrintErrorInfoFunc(chipTypes);
    // 更新差异化uninit钩子函数
    UpdateTaskUninitFunc(chipTypes);

    return;
}

}
}