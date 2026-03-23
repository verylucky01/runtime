/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "capture_model_utils.hpp"
#include "inner_thread_local.hpp"

namespace cce {
namespace runtime {
bool IsEventCapturing(const Event * const evt, const Stream * const stm)
{
    if (evt->GetEventFlag() == RT_EVENT_EXTERNAL) {
        return false;
    }
    CaptureModel *mdl = nullptr;
    const Event *captureEvt = evt->GetCaptureEvent();
    if (captureEvt != nullptr) {
         const Stream *captureEvtStm = captureEvt->GetCaptureStream();
         if (captureEvtStm != nullptr) {
            mdl = dynamic_cast<CaptureModel *>(captureEvtStm->Model_());
            if ((mdl != nullptr) && (mdl->IsCapturing())) {
                return true;
            }
         }
    }
    const Stream *captureStm = stm->GetCaptureStream();
    if (captureStm != nullptr) {
        mdl = dynamic_cast<CaptureModel *>(captureStm->Model_());
        if ((mdl != nullptr) && (mdl->IsCapturing())) {
            return true;
        }
    }
    return false;
}

bool IsCrossCaptureModel(const Event * const evt, const Stream * const stm)
{
    CaptureModel *evtMdl = nullptr;
    const Stream * const captureStm = evt->GetCaptureStream();
    if (captureStm != nullptr) {
        evtMdl = dynamic_cast<CaptureModel *>(captureStm->Model_());
    }
    CaptureModel *stmMdl = dynamic_cast<CaptureModel *>(stm->Model_());
    // stmMdl and evtMdl must not be nullptr at this time.
    return (stmMdl != evtMdl);
}

void TerminateCapture(const Event * const evt, const Stream * const stm)
{
    CaptureModel *stmMdl = nullptr;
    const Stream *captureStm = stm->GetCaptureStream();
    if (captureStm != nullptr) {
        stmMdl = dynamic_cast<CaptureModel *>(captureStm->Model_());
    }
    CaptureModel *evtMdl = evt->GetCaptureModel();
    if (evtMdl != nullptr) {
        evtMdl->TerminateCapture();
    }
    if ((stmMdl != nullptr) && (stmMdl != evtMdl)) {
        stmMdl->TerminateCapture();
    }
}

rtError_t GetCaptureStream(Context * const ctx, Stream * const stm, const Event * const evt, Stream ** const captureStm)
{
    Stream *curStm = stm->GetCaptureStream();
    if (curStm != nullptr) {
        *captureStm = curStm;
        RT_LOG(RT_LOG_INFO, "Capture stream has been created, stream_id=[%d->%d].",
            stm->Id_(), curStm->Id_());
        return RT_ERROR_NONE;
    }

    Event *captureEvt = evt->GetCaptureEvent();
    COND_RETURN_ERROR_MSG_INNER((captureEvt == nullptr), RT_ERROR_EVENT_NULL,
        "Capture event is invalid, stream_id=%d, event_id=%d.",
        stm->Id_(), evt->EventId_());

    Stream *evtCaptureStm = captureEvt->GetCaptureStream();
    COND_RETURN_ERROR_MSG_INNER(evtCaptureStm == nullptr, RT_ERROR_MODEL_NULL,
        "Event capture stream is invalid, stream_id=%d, event_id=[%d->%d].",
        stm->Id_(), evt->EventId_(), captureEvt->EventId_());

    CaptureModel *captureMdl = dynamic_cast<CaptureModel *>(evtCaptureStm->Model_());
    COND_RETURN_ERROR_MSG_INNER(captureMdl == nullptr, RT_ERROR_MODEL_NULL,
        "Capture model is invalid, stream_id=%d, event_id=[%d->%d].",
        stm->Id_(), evt->EventId_(), captureEvt->EventId_());

    const rtError_t error = ctx->StreamAddToCaptureModelProc(stm, captureMdl);
    ERROR_RETURN_MSG_INNER(error, "New capture stream failed, stream_id=%d, event_id=[%d->%d].",
        stm->Id_(), evt->EventId_(), captureEvt->EventId_());

    curStm = stm->GetCaptureStream();
    COND_RETURN_ERROR_MSG_INNER(curStm == nullptr, RT_ERROR_STREAM_NULL,
        "Capture stream is invalid, stream_id=%d, event_id=[%d->%d].",
        stm->Id_(), evt->EventId_(), captureEvt->EventId_());

    RT_LOG(RT_LOG_INFO, "Capture event_id=%d, stream_id=[%d->%d] bounded to model_id=%u.",
        captureEvt->EventId_(), stm->Id_(), curStm->Id_(), captureMdl->Id_());
    *captureStm = curStm;
    return RT_ERROR_NONE;
}

bool IsCapturedTask(const Stream * const launchStm, const TaskInfo *submitTask)
{
    return (launchStm != submitTask->stream);
}

rtError_t CheckCaptureStreamThreadIsMatch(const Stream * const stm)
{
    const rtStreamCaptureMode streamCaptureMode = stm->GetStreamCaptureMode();
    const uint32_t threadId = stm->GetBeginCaptureThreadId();
    if (threadId == runtime::GetCurrentTid()) {
        return RT_ERROR_NONE;
    }
    if (streamCaptureMode != RT_STREAM_CAPTURE_MODE_RELAXED) {
        RT_LOG(RT_LOG_ERROR, "end capture in the wrong thread.");
        return RT_ERROR_STREAM_CAPTURE_WRONG_THREAD;
    }
    return RT_ERROR_NONE;
}

bool IsSoftwareSqCaptureModel(Model * const mdl)
{
    if (mdl->GetModelType() != ModelType::RT_MODEL_CAPTURE_MODEL) {
        return false;
    }
    CaptureModel *capMdl = dynamic_cast<CaptureModel *>(mdl);
    return capMdl != nullptr && capMdl->IsSoftwareSqEnable();
}

rtError_t CheckCaptureModelSupportSoftwareSq(Device* const dev)
{
    NULL_PTR_RETURN(dev, RT_ERROR_DEVICE_NULL);
    if (!(dev->IsSupportFeature(RtOptionalFeatureType::RT_FEATURE_MODEL_ACL_GRAPH_SOFTWARE_ENABLE))) {
        const rtChipType_t chipType = Runtime::Instance()->GetChipType();
        RT_LOG(RT_LOG_WARNING, "chipType=%d does not support", chipType);
        RT_LOG_OUTER_MSG_WITH_FUNC(ErrorCode::EE1005);
        return RT_ERROR_FEATURE_NOT_SUPPORT;
    }

    if (!(dev->CheckFeatureSupport(TS_FEATURE_SOFTWARE_SQ_ENABLE))) {
        RT_LOG(RT_LOG_WARNING, "tsfw does not support");
        RT_LOG_OUTER_MSG_WITH_FUNC(ErrorCode::EE1005);
        return RT_ERROR_FEATURE_NOT_SUPPORT;
    }

    if (!(NpuDriver::CheckIsSupportFeature(dev->Id_(), FEATURE_TRSDRV_SQ_SUPPORT_DYNAMIC_BIND))) {
        RT_LOG(RT_LOG_WARNING, "drv does not support");
        RT_LOG_OUTER_MSG_WITH_FUNC(ErrorCode::EE1005);
        return RT_ERROR_DRV_NOT_SUPPORT;
    }

    return RT_ERROR_NONE;
}

rtError_t ConstructNopTask(Stream* stm, uint8_t* sqeBufferBackup, uint32_t& sendSqeNum)
{
    rtError_t error = RT_ERROR_NONE;
    TaskInfo taskSubmit = {};
    TaskInfo* rtNopTask = stm->AllocTask(&taskSubmit, TS_TASK_TYPE_NOP, error);
    NULL_PTR_RETURN(rtNopTask, error);
    Device* dev = stm->Device_();
    std::function<void()> const errRecycle = [&dev, &rtNopTask]() { (void)dev->GetTaskFactory()->Recycle(rtNopTask); };
    ScopeGuard tskErrRecycle(errRecycle);
    error = NopTaskInit(rtNopTask);
    ERROR_RETURN(
        error, "nop task init failed,stream_id=%d, task_id=%hu, retCode=%#x.", stm->Id_(), rtNopTask->id, error);

    rtTsCommand_t cmdLocal = {};
    cmdLocal.cmdType = RT_TASK_COMMAND_TYPE_STARS_SQE;
    ToConstructSqe(rtNopTask, cmdLocal.cmdBuf.u.starsSqe);
    sendSqeNum = GetSendSqeNum(rtNopTask);
    error = stm->StarsAddTaskToStreamForModelUpdate(rtNopTask, sendSqeNum);
    ERROR_RETURN(error, "Add task failed stream_id=%d, task_id=%u.", stm->Id_(), rtNopTask->id);
    auto ret = memcpy_s(
        RtPtrToPtr<void*>(sqeBufferBackup + sizeof(rtStarsSqe_t) * rtNopTask->pos), sendSqeNum * sizeof(rtStarsSqe_t),
        RtPtrToPtr<void*, rtStarsSqe_t*>(cmdLocal.cmdBuf.u.starsSqe), sendSqeNum * sizeof(rtStarsSqe_t));

    COND_RETURN_ERROR(
        ret != EOK, RT_ERROR_INVALID_VALUE, "memcpy_s failed, device_id=%u, stream_id=%d, task_id=%hu, error=%d",
        dev->Id_(), stm->Id_(), rtNopTask->id, ret);
    Complete(rtNopTask, dev->Id_());
    GET_THREAD_TASKID_AND_STREAMID(rtNopTask, stm->Id_());
    tskErrRecycle.ReleaseGuard();
    stm->CacheCaptureTaskId(rtNopTask->id);
    RT_LOG(RT_LOG_INFO, "construct nop task finish, stream_id=%d, task_id=%hu.", stm->Id_(), rtNopTask->id);
    return error;
}

rtError_t CheckCaptureModelForUpdate(Stream* stm) {
    NULL_PTR_RETURN(stm, RT_ERROR_STREAM_NULL);
    Device* dev = stm->Device_();
    static rtError_t isSupportResult = CheckCaptureModelSupportSoftwareSq(dev);
    COND_RETURN_WITH_NOLOG((isSupportResult != RT_ERROR_NONE), isSupportResult);

    Model* const mdl = stm->Model_();
    NULL_PTR_RETURN(mdl, RT_ERROR_MODEL_NULL);
    COND_RETURN_ERROR(
        mdl->GetModelType() != RT_MODEL_CAPTURE_MODEL, RT_ERROR_INVALID_VALUE, "now only support aclGraph.");
    CaptureModel* captureModel = dynamic_cast<CaptureModel*>(mdl);
    NULL_PTR_RETURN(captureModel, RT_ERROR_MODEL_NULL);
    if (!captureModel->CanUpdate()) {
        RT_LOG(
            RT_LOG_ERROR, "model is not ready for update, model_id=%u, current status=%d", captureModel->Id_(),
            captureModel->GetCaptureModelStatus());
        return RT_ERROR_MODEL_NOT_READY_FOR_UPDATE;
    }
    return RT_ERROR_NONE;
}

} // namespace runtime
} // namespace cce