/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "stars_david.hpp"
#include "stream.hpp"
#include "profiling_task.h"

namespace cce {
namespace runtime {

#if F_DESC("ProfilingEnableTask")
void ConstructDavidSqeForProfilingEnableTask(TaskInfo * const taskInfo, rtDavidSqe_t * const davidSqe,
    uint64_t sqBaseAddr)
{
    UNUSED(sqBaseAddr);
    ProfilingEnableTaskInfo * const profilingEnableTaskInfo = &(taskInfo->u.profilingEnableTaskInfo);
    Stream * const stream = taskInfo->stream;

    ConstructDavidSqeForHeadCommon(taskInfo, davidSqe);
    RtDavidPlaceHolderSqe *const sqe = &(davidSqe->phSqe);
    sqe->header.type = RT_DAVID_SQE_TYPE_PLACE_HOLDER;
    sqe->header.preP = 1U;
    sqe->taskType = TS_TASK_TYPE_PROFILER_DYNAMIC_ENABLE;
    sqe->kernelCredit = RT_STARS_DEFAULT_KERNEL_CREDIT_DAVID;
    sqe->u.dynamicProfilingInfo.pid = profilingEnableTaskInfo->pid;
    sqe->u.dynamicProfilingInfo.isTaskBasedProfEn = profilingEnableTaskInfo->isTaskBasedProfEn;
    sqe->u.dynamicProfilingInfo.isSocLogEn = profilingEnableTaskInfo->isHwtsLogEn;
    PrintDavidSqe(davidSqe, "DynamicProfilingEnableTask");
    RT_LOG(RT_LOG_INFO, "ProfilingEnableTask, device_id=%u, stream_id=%d, task_id=%hu, pid=%llu."
        "isSocLogEn=%hhu, isTaskBasedProfEn=%hhu.",
        taskInfo->stream->Device_()->Id_(), stream->Id_(), taskInfo->id, profilingEnableTaskInfo->pid,
        sqe->u.dynamicProfilingInfo.isSocLogEn, sqe->u.dynamicProfilingInfo.isTaskBasedProfEn);
}
#endif

#if F_DESC("ProfilingDisableTask")
void ConstructDavidSqeForProfilingDisableTask(TaskInfo * const taskInfo, rtDavidSqe_t * const davidSqe,
    uint64_t sqBaseAddr)
{
    UNUSED(sqBaseAddr);
    ProfilingDisableTaskInfo * const profilingDisableTaskInfo = &(taskInfo->u.profilingDisableTaskInfo);
    Stream * const stream = taskInfo->stream;

    ConstructDavidSqeForHeadCommon(taskInfo, davidSqe);
    RtDavidPlaceHolderSqe * const sqe = &(davidSqe->phSqe);
    sqe->header.type = RT_DAVID_SQE_TYPE_PLACE_HOLDER;
    sqe->header.preP = 1U;
    sqe->taskType = TS_TASK_TYPE_PROFILER_DYNAMIC_DISABLE;
    sqe->kernelCredit = RT_STARS_DEFAULT_KERNEL_CREDIT_DAVID;
    sqe->u.dynamicProfilingInfo.pid = profilingDisableTaskInfo->pid;
    sqe->u.dynamicProfilingInfo.isTaskBasedProfEn = profilingDisableTaskInfo->isTaskBasedProfEn;
    sqe->u.dynamicProfilingInfo.isSocLogEn = profilingDisableTaskInfo->isHwtsLogEn;
    PrintDavidSqe(davidSqe, "DynamicProfilingDisableTask");
    RT_LOG(RT_LOG_INFO, "ProfilingDisableTask, device_id=%u, stream_id=%d, task_id=%hu, pid=%u, "
        "isSocLogEn=%hhu, isTaskBasedProfEn=%hhu.",
        taskInfo->stream->Device_()->Id_(), stream->Id_(), taskInfo->id,
        sqe->u.dynamicProfilingInfo.pid, sqe->u.dynamicProfilingInfo.isSocLogEn,
        sqe->u.dynamicProfilingInfo.isTaskBasedProfEn);
}
#endif

#if F_DESC("ProfilerTraceExTask")
void ConstructDavidSqeForProfilerTraceExTask(TaskInfo *taskInfo, rtDavidSqe_t *const davidSqe,
    uint64_t sqBaseAddr)
{
    UNUSED(sqBaseAddr);
    Stream * const stm = taskInfo->stream;
    ConstructDavidSqeForHeadCommon(taskInfo, davidSqe);
    RtDavidPlaceHolderSqe * const sqe = &(davidSqe->phSqe);
    sqe->header.type = RT_DAVID_SQE_TYPE_PLACE_HOLDER;
    sqe->header.preP = 1U;
    sqe->taskType = TS_TASK_TYPE_PROFILER_TRACE_EX;
    sqe->kernelCredit = RT_STARS_DEFAULT_KERNEL_CREDIT_DAVID;
    sqe->u.profileTraceInfo.profilerTraceId = taskInfo->u.profilerTraceExTask.profilerTraceId;
    sqe->u.profileTraceInfo.modelId = taskInfo->u.profilerTraceExTask.modelId;
    sqe->u.profileTraceInfo.tagId = taskInfo->u.profilerTraceExTask.tagId;

    PrintDavidSqe(davidSqe, "ProfilerTraceExTask");
    RT_LOG(RT_LOG_INFO, "ProfilerTraceExTask, device_id=%u, stream_id=%d, task_id=%hu, task_sn=%u.",
        stm->Device_()->Id_(), stm->Id_(), taskInfo->id, taskInfo->taskSn);
}
#endif

}  // namespace runtime
}  // namespace cce
