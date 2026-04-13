/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "context.hpp"

namespace cce {
namespace runtime {

rtError_t Context::TryRecycleCaptureModelResource(const uint32_t allocSqNum, const uint32_t ntfCnt,
    const CaptureModel * const excludeMdl)
{
    rtError_t error = RT_ERROR_NONE;
    uint32_t releaseSqNum = 0U;
    uint32_t totalReleaseSqNum = 0U;
    uint32_t totalReleaseNtfNum = 0U;

    modelLock_.Lock();
    for (Model *model : models_) {
        if ((model != nullptr) && (model->GetModelType() == RT_MODEL_CAPTURE_MODEL)) {
            CaptureModel *captureMdl = dynamic_cast<CaptureModel *>(model);

            if (allocSqNum <= totalReleaseSqNum && ntfCnt <= totalReleaseNtfNum) break;

            if (!captureMdl->IsSoftwareSqEnable() || (captureMdl == excludeMdl)) {
                continue;
            }

            if (allocSqNum > totalReleaseSqNum) {
                if (captureMdl->ModelSqOperTryLock()) {
                    releaseSqNum = 0U;
                    error = captureMdl->ReleaseSqCq(releaseSqNum);
                    captureMdl->ModelSqOperUnLock();
                    COND_PROC(error != RT_ERROR_NONE, break);
                    totalReleaseSqNum += releaseSqNum;
                }
            }

            if (ntfCnt > totalReleaseNtfNum) {
                if (captureMdl->ModelSqOperTryLock()) {
                    error = captureMdl->ReleaseNotifyId();
                    captureMdl->ModelSqOperUnLock();
                    COND_PROC(error != RT_ERROR_NONE, continue);
                    totalReleaseNtfNum++;
                }
            }
        }
    }
    modelLock_.Unlock();

    return error;
}

} // namespace runtime
} // namespace cce
