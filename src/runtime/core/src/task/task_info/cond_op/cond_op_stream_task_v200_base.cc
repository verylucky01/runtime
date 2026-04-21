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
#include "runtime.hpp"
#include "stars_cond_isa_helper.hpp"
#include "cond_op_stream_task.h"

namespace cce {
namespace runtime {

void ConstructDavidSqeForStreamLabelSwitchByIndexTask(TaskInfo * const taskInfo, rtDavidSqe_t * const davidSqe, uint64_t sqBaseAddr)
{
    UNUSED(sqBaseAddr);
    ConstructDavidSqeForHeadCommon(taskInfo, davidSqe);
    RtDavidStarsFunctionCallSqe &sqe = davidSqe->fuctionCallSqe;
    StmLabelSwitchByIdxTaskInfo * const stmLblSwiByIdx = &(taskInfo->u.stmLabelSwitchIdxTask);
    sqe.header.type = RT_DAVID_SQE_TYPE_COND;
    sqe.condsSubType = CONDS_SUB_TYPE_LABEL_SWITCH_BY_INDEX;
    sqe.kernelCredit = RT_STARS_DEFAULT_KERNEL_CREDIT_DAVID;
    sqe.sqeLength = 0U;
    sqe.csc = 1U;

    const uint64_t funcAddr = RtPtrToValue(stmLblSwiByIdx->funcCallSvmMem);
    const uint64_t funcCallSize = static_cast<uint64_t>(sizeof(rtStarsLabelSwitchByIndexFc_t));

    // func call size is rs2[19:0]*4Byte
    ConstructFunctionCallInstr(funcAddr, (funcCallSize / 4UL), sqe);

    PrintDavidSqe(davidSqe, "StreamLabelSwitchByIndexTask");
    RT_LOG(RT_LOG_INFO, "StreamLabelSwitchByIndex, deviceId=%u, streamId=%d, taskId=%hu",
        taskInfo->stream->Device_()->Id_(), taskInfo->stream->Id_(), taskInfo->id);
}

void ConstructDavidSqeForStreamSwitchTask(TaskInfo * const taskInfo, rtDavidSqe_t *const davidSqe, uint64_t sqBaseAddr)
{
    Stream * const stm = taskInfo->stream;
    StreamSwitchTaskInfo * const streamSwitchTask = &(taskInfo->u.streamswitchTask);
    UNUSED(sqBaseAddr);
    ConstructDavidSqeForHeadCommon(taskInfo, davidSqe);
    RtDavidStarsFunctionCallSqe &sqe = davidSqe->fuctionCallSqe;

    sqe.header.type = RT_DAVID_SQE_TYPE_COND;
    if (streamSwitchTask->isCondEx) {
        sqe.condsSubType = CONDS_SUB_TYPE_STREAM_SWITCH_EX;
    } else {
        sqe.condsSubType = CONDS_SUB_TYPE_STREAM_SWITCH;
    }

    sqe.kernelCredit = RT_STARS_DEFAULT_KERNEL_CREDIT_DAVID;
    sqe.sqeLength = 0U;
    sqe.csc = 1U;

    const uint64_t funcAddr = RtPtrToValue(streamSwitchTask->funcCallSvmMem);

    // func call size is rs2[19:0]*4Byte
    ConstructFunctionCallInstr(funcAddr, (streamSwitchTask->funCallMemSize / 4UL), sqe);

    PrintDavidSqe(davidSqe, "StreamSwitchTask");
    RT_LOG(RT_LOG_INFO, "StreamSwitchTask, deviceId=%u, streamId=%d, taskId=%hu, trueStreamId=%u.",
        stm->Device_()->Id_(), stm->Id_(), taskInfo->id, streamSwitchTask->trueStreamId);
    return;
}

}  // namespace runtime
}  // namespace cce
