/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "stream.hpp"
#include "runtime.hpp"
#include "stars_cond_isa_helper.hpp"
#include "context.hpp"
#include "rdma_task.h"
#include "thread_local_container.hpp"
#include "inner_thread_local.hpp"
#include "notify.hpp"

namespace cce {
namespace runtime {

#if F_DESC("RdmaSendTask")
rtError_t RdmaSendTaskInit(TaskInfo* taskInfo, const uint32_t sqId, const uint32_t wqeId)
{
    TaskCommonInfoInit(taskInfo);
    taskInfo->type = TS_TASK_TYPE_RDMA_SEND;
    taskInfo->typeName = const_cast<char_t*>("RDMA_SEND");
    taskInfo->u.rdmaSendTask.sqIndex = sqId;
    taskInfo->u.rdmaSendTask.wqeIndex = wqeId;
    return RT_ERROR_NONE;
}

void ToCommandBodyForRdmaSendTask(TaskInfo* taskInfo, rtCommand_t *const command)
{
    RT_LOG(RT_LOG_INFO, "index=0x%x, wqe_index=%u.",
        taskInfo->u.rdmaSendTask.sqIndex, taskInfo->u.rdmaSendTask.wqeIndex);
    command->u.rdmaSendTask.sqIndex = taskInfo->u.rdmaSendTask.sqIndex;
    command->u.rdmaSendTask.wqeIndex = taskInfo->u.rdmaSendTask.wqeIndex;
}

#endif

#if F_DESC("RdmaDbSendTask")
rtError_t RdmaDbSendTaskInit(TaskInfo* taskInfo, const uint32_t dbIndex, const uint64_t dbInfo, const uint32_t taskSeq)
{
    RdmaDbSendTaskInfo* rdmaDbSend = &taskInfo->u.rdmaDbSendTask;

    TaskCommonInfoInit(taskInfo);
    taskInfo->type = TS_TASK_TYPE_RDMA_DB_SEND;
    taskInfo->typeName = const_cast<char_t*>("RDMA_DB_SEND");
    RT_LOG(RT_LOG_INFO, "dbIndex=%#x, dbInfo=%#" PRIx64, dbIndex, dbInfo);
    rdmaDbSend->taskDbIndex.value = dbIndex;
    rdmaDbSend->taskDbInfo.value = dbInfo;

    // check vfid and dieid
    if (taskInfo->stream->Device_()->IsStarsPlatform()) {
        if (taskInfo->stream->GetBindFlag()) {
            RT_LOG(RT_LOG_INFO, "taskSeq=%u", taskSeq);
            rdmaDbSend->taskSeq = taskSeq;
        }
        const uint32_t vfId = rdmaDbSend->taskDbIndex.dbIndexStars.vfId;
        const uint32_t dieId = rdmaDbSend->taskDbIndex.dbIndexStars.dieId;
        COND_RETURN_ERROR_MSG_CALL(ERR_MODULE_HCCL, vfId != 0U,
            RT_ERROR_INVALID_VALUE, "invalid vfId, vfId=%u", vfId);

        const int64_t phyDieId = taskInfo->stream->Device_()->GetPhyDieId();
        COND_RETURN_ERROR_MSG_CALL(ERR_MODULE_HCCL, static_cast<uint32_t>(phyDieId) != dieId,
            RT_ERROR_INVALID_VALUE, "invalid dieId, dieId=%u, phyDieId=%u", dieId, static_cast<uint32_t>(phyDieId));
    }

    return RT_ERROR_NONE;
}

void ToCommandBodyForRdmaDbSendTask(TaskInfo* taskInfo, rtCommand_t * const command)
{
    RdmaDbSendTaskInfo* rdmaDbSend = &taskInfo->u.rdmaDbSendTask;

    RT_LOG(RT_LOG_INFO, "dbIndex=%#x, dbInfo=%#" PRIx64,
        rdmaDbSend->taskDbIndex.value, rdmaDbSend->taskDbInfo.value);
    command->u.rdmaDbSendTask.dbIndex = rdmaDbSend->taskDbIndex.value;
    command->u.rdmaDbSendTask.dbInfo = rdmaDbSend->taskDbInfo.value;
}

uint32_t GetSendSqeNumForRdmaDbSendTask(TaskInfo * const taskInfo)
{
    if (taskInfo->stream->GetBindFlag()) {
        taskInfo->bindFlag = true;
    }

    constexpr uint32_t num = 1U;
    RT_LOG(RT_LOG_INFO, "ChipType=%u, BindFlag=%hhu, SqeNum=%u", Runtime::Instance()->GetChipType(),
        taskInfo->bindFlag, num);
    return num;
}

uint64_t GetRoceDbAddrForRdmaDbSendTask(TaskInfo* const taskInfo)
{
    int64_t chipId = 0U;
    const uint32_t deviceId = taskInfo->stream->Device_()->Id_();
    const rtError_t error = taskInfo->stream->Device_()->Driver_()->GetDevInfo(deviceId, MODULE_TYPE_SYSTEM,
        INFO_TYPE_PHY_CHIP_ID, &chipId);
    if (error != RT_ERROR_NONE) {
        RT_LOG(RT_LOG_ERROR, "Get chip failed!, device_id=%u", deviceId);
        return 0ULL;
    }
    const uint64_t chipAddr = taskInfo->stream->Device_()->GetChipAddr();
    const uint64_t chipOffset = taskInfo->stream->Device_()->GetChipOffset();
    const uint64_t dieOffset  = taskInfo->stream->Device_()->GetDieOffset();

    const uint64_t dbAddr = RT_ROCEE_BASE_ADDR_128G + RT_ROCEE_VF_DB_CFG0_REG_230 +
        chipOffset * static_cast<uint64_t>(chipId) +
        dieOffset * (taskInfo->u.rdmaDbSendTask.taskDbIndex.dbIndexStars.dieId) +
        chipAddr;
    return dbAddr;
}

void ConstructSqeSinkModeForRdmaDb1SendTask(TaskInfo* taskInfo, rtStarsSqe_t * const command)
{
    RdmaDbSendTaskInfo *rdmaDbSendTask = &(taskInfo->u.rdmaDbSendTask);
    Stream * const stream = taskInfo->stream;

    // Construct sqe1
    const uint8_t sqDepthBitWidth = rdmaDbSendTask->taskDbIndex.dbIndexStars.sqDepthBitWidth;
    rtRdmaDbInfo_t dbInfo;
    dbInfo.value = rdmaDbSendTask->taskDbInfo.value;
    RtStarsRdmaSinkSqe1 &sqe1 = command->rdmaSinkSqe1;
    const uint16_t currentStreamId = static_cast<uint16_t>(stream->Id_());
    uint16_t * const execTimesSvm = stream->GetExecutedTimesSvm();
    const uint64_t svmAddr = RtPtrToValue<uint16_t *>(execTimesSvm);

    (void)memset_s(&sqe1, sizeof(rtStarsSqe_t), 0, sizeof(rtStarsSqe_t));
    ConstructRdmaSink1Instr(static_cast<uint32_t>(dbInfo.cmd.sqProducerIdx),
        sqDepthBitWidth, svmAddr, sqe1);
    sqe1.kernelCredit = RT_STARS_DEFAULT_KERNEL_CREDIT;
    sqe1.csc = 1U;
    sqe1.sqeHeader.type = RT_STARS_SQE_TYPE_COND;
    sqe1.sqeHeader.l1_lock = 1U;
    sqe1.sqeHeader.l1_unlock = 0U;
    sqe1.sqeHeader.wr_cqe = stream->GetStarsWrCqeFlag();
    sqe1.sqeHeader.block_dim = 0U;  // block_dim is not used by C-Conds
    sqe1.sqeHeader.rt_stream_id = currentStreamId;
    sqe1.sqeHeader.task_id = taskInfo->id;
    sqe1.condsSubType = CONDS_SUB_TYPE_RDMA_1;
    PrintSqe(command, "RdmaDbSendSink1");
}

void ConstructSqeSinkModeForRdmaDb2SendTask(TaskInfo* taskInfo, rtStarsSqe_t * const command)
{
    RdmaDbSendTaskInfo *rdmaDbSendTask = &(taskInfo->u.rdmaDbSendTask);
    Stream * const stream = taskInfo->stream;
    rtRdmaDbInfo_t dbInfo;
    dbInfo.value = rdmaDbSendTask->taskDbInfo.value;
    const uint16_t currentStreamId = static_cast<uint16_t>(stream->Id_());

    // Construct sqe2
    RtStarsRdmaSinkSqe2 &sqe2 = command->rdmaSinkSqe2;
    const uint64_t dbAddr = GetRoceDbAddrForRdmaDbSendTask(taskInfo);
    if (dbAddr == 0ULL) {
        sqe2.sqeHeader.type = RT_STARS_SQE_TYPE_INVALID;
        return;
    }
    dbInfo.cmd.sqProducerIdx = 0U;

    (void)memset_s(&sqe2, sizeof(rtStarsSqe_t), 0, sizeof(rtStarsSqe_t));
    ConstructRdmaSink2Instr(dbAddr, dbInfo.value, sqe2);
    sqe2.kernelCredit = RT_STARS_DEFAULT_KERNEL_CREDIT;
    sqe2.csc = 0U;
    sqe2.sqeHeader.type = RT_STARS_SQE_TYPE_COND;
    sqe2.sqeHeader.l1_lock = 0U;
    sqe2.sqeHeader.l1_unlock = 1U;
    sqe2.sqeHeader.wr_cqe = stream->GetStarsWrCqeFlag();
    sqe2.sqeHeader.block_dim = 0U;  // block_dim is not used by C-Conds
    sqe2.sqeHeader.rt_stream_id = currentStreamId;
    sqe2.sqeHeader.task_id = taskInfo->id;
    sqe2.condsSubType = CONDS_SUB_TYPE_RDMA_2;

    PrintSqe(command, "RdmaDbSendSink2");
}

void ConstructSqeNoSinkModeForRdmaDbSendTask(TaskInfo* taskInfo, rtStarsSqe_t * const command)
{
    Stream * const stream = taskInfo->stream;
    RtStarsWriteValueSqe * const sqe = &(command->writeValueSqe);

    (void)memset_s(sqe, sizeof(rtStarsSqe_t), 0, sizeof(rtStarsSqe_t));
    sqe->header.type = RT_STARS_SQE_TYPE_WRITE_VALUE;
    sqe->header.ie = RT_STARS_SQE_INT_DIR_NO;
    sqe->header.pre_p = RT_STARS_SQE_INT_DIR_NO;
    sqe->header.post_p = RT_STARS_SQE_INT_DIR_NO;
    sqe->header.wr_cqe = stream->GetStarsWrCqeFlag();
    sqe->header.rt_stream_id = static_cast<uint16_t>(stream->Id_());
    sqe->header.task_id = taskInfo->id;

    sqe->va = 0U;
    sqe->kernel_credit = RT_STARS_DEFAULT_KERNEL_CREDIT;
    sqe->awsize = RT_STARS_WRITE_VALUE_SIZE_TYPE_64BIT;

    sqe->sub_type = RT_STARS_WRITE_VALUE_SUB_TYPE_RDMA_DB_SEND;

    const uint64_t dbVal = taskInfo->u.rdmaDbSendTask.taskDbInfo.value; // GetRoceDbVal();
    const uint64_t dbAddr = GetRoceDbAddrForRdmaDbSendTask(taskInfo);   // GetRoceDbAddr();
    if (dbAddr == 0ULL) {
        sqe->header.type = RT_STARS_SQE_TYPE_INVALID;
        return;
    }
    sqe->write_value_part0 = static_cast<uint32_t>(dbVal & MASK_32_BIT);
    sqe->write_value_part1 = static_cast<uint32_t>(dbVal >> UINT32_BIT_NUM);
    sqe->write_addr_low = static_cast<uint32_t>(dbAddr & MASK_32_BIT);
    sqe->write_addr_high = static_cast<uint32_t>((dbAddr >> UINT32_BIT_NUM) & MASK_17_BIT);

    PrintSqe(command, "RdmaDbSendNoSink");
    RT_LOG(RT_LOG_INFO, "RdmaDbSendTask no-sink device_id=%d, stream_id=%d, die_id=%u, task_id=%hu, sqProducerIdx=%u, "
        " dbAddr=%#" PRIx64" dbVal:%#" PRIx64,
        taskInfo->stream->Device_()->Id_(), stream->Id_(), taskInfo->u.rdmaDbSendTask.taskDbIndex.dbIndexStars.dieId,
        taskInfo->id, taskInfo->u.rdmaDbSendTask.taskDbInfo.cmd.sqProducerIdx, dbAddr, dbVal);
}

void ConstructSqeForRdmaDbSendTask(TaskInfo* taskInfo, rtStarsSqe_t * const command)
{
    RdmaDbSendTaskInfo *rdmaDbSendTask = &(taskInfo->u.rdmaDbSendTask);
    if (taskInfo->bindFlag != 0U) {
        if (rdmaDbSendTask->taskSeq == 1U) {
            return ConstructSqeSinkModeForRdmaDb1SendTask(taskInfo, command);
        } else if (rdmaDbSendTask->taskSeq == 2U) {
            return ConstructSqeSinkModeForRdmaDb2SendTask(taskInfo, command);
        } else {
            RT_LOG(RT_LOG_ERROR, "model taskSeq must be [1, 2], but taskSeq=%u", rdmaDbSendTask->taskSeq);
        }
    }
    return ConstructSqeNoSinkModeForRdmaDbSendTask(taskInfo, command);
}

#endif

}  // namespace runtime
}  // namespace cce
