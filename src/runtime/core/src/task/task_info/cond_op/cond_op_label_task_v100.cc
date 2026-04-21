/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "stream.hpp"
#include "runtime.hpp"
#include "task_info_v100.h"
#include "cond_op_label_task.h"

namespace cce {
namespace runtime {

#if F_DESC("LabelSetTask")
void ConstructSqeForLabelSetTask(TaskInfo* taskInfo, rtStarsSqe_t * const command)
{
    Stream * const stm = taskInfo->stream;
    command->phSqe.type = RT_STARS_SQE_TYPE_PLACE_HOLDER;
    command->phSqe.l2_lock = 0U;
    command->phSqe.ie = 0U;
    command->phSqe.pre_p = 0U;
    command->phSqe.post_p = 0U;
    command->phSqe.wr_cqe = stm->GetStarsWrCqeFlag();
    command->phSqe.res0 = 0U;

    command->phSqe.task_type = TS_TASK_TYPE_LABEL_SET;
    command->phSqe.task_id = taskInfo->id;
    command->phSqe.rt_streamID = static_cast<uint16_t>(stm->Id_());
    command->phSqe.kernel_credit = RT_STARS_DEFAULT_KERNEL_CREDIT;
    Model *mdl = stm->Model_();
    if (mdl != nullptr) {
        mdl->LabelCountInc();
    }
    PrintSqe(command, "LabelSet");
    RT_LOG(RT_LOG_INFO, "LabelSetTask stream_id:%d task_id:%u", stm->Id_(), static_cast<uint32_t>(taskInfo->id));
}
#endif

}  // namespace runtime
}  // namespace cce
