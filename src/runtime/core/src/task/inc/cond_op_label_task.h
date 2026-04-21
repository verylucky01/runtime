/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef RUNTIME_COND_OP_LABEL_TASK_H
#define RUNTIME_COND_OP_LABEL_TASK_H

#include "task_info.hpp"

namespace cce {
namespace runtime {
rtError_t LabelSetTaskInit(TaskInfo* taskInfo, const uint16_t labelIndex, void * const devDestAddr);
rtError_t LabelSwitchTaskInit(TaskInfo* taskInfo, const void *const ptr, const rtCondition_t cond,
    const uint32_t val, const uint16_t labelId);
rtError_t LabelGotoTaskInit(TaskInfo* taskInfo, const uint16_t lblId);

void ToCommandBodyForLabelSetTask(TaskInfo* taskInfo, rtCommand_t * const command);
void ToCommandBodyForLabelSwitchTask(TaskInfo* taskInfo, rtCommand_t *const command);
void ToCommandBodyForLabelGotoTask(TaskInfo* taskInfo, rtCommand_t *const command);

void SetLabelInfoForLabelSetTask(TaskInfo* taskInfo, const uint32_t pos);

}  // namespace runtime
}  // namespace cce
#endif  // RUNTIME_COND_OP_LABEL_TASK_H
