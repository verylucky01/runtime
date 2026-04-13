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
#include <cinttypes>
#include "runtime.hpp"
#include "event.hpp"
#include "notify.hpp"
#include "count_notify.hpp"
#include "npu_driver.hpp"
#include "error_message_manage.hpp"
#include "thread_local_container.hpp"
#include "inner_thread_local.hpp"
#include "task_info.hpp"
#include "capture_model.hpp"
#include "capture_model_utils.hpp"
#include "capture_adapt.hpp"
#include "buffer_allocator.hpp"
#include "memcpy_c.hpp"

namespace cce {
namespace runtime {
namespace {
constexpr size_t NOTIFY_INDEX = 2U;
} // namespace

rtError_t Context::UpdateEndGraphTask(Stream * const origCaptureStream, Stream * const exeStream, Notify *ntf) const
{
    uint16_t taskId = origCaptureStream->GetLastTaskId();
    TaskInfo *rtNotifyRecord = origCaptureStream->Device_()->GetTaskFactory()->GetTask(origCaptureStream->Id_(), taskId);
    COND_RETURN_ERROR(rtNotifyRecord == nullptr, RT_ERROR_STREAM_CAPTURED, "EndGraph task is NULL");

    COND_RETURN_ERROR(rtNotifyRecord->type != TS_TASK_TYPE_NOTIFY_RECORD, RT_ERROR_STREAM_INVALID,
        "EndGraph task type=%u", rtNotifyRecord->type);
    rtNotifyRecord->u.notifyrecordTask.notifyId = ntf->GetNotifyId();
    uint8_t sqeMem[RT_STARS_SQE_LEN] = {0};
    ConstructStarsSqeForNotifyRecordTask(rtNotifyRecord, sqeMem);

    void *targetAddrOfUpdatedSqe = RtValueToPtr<void *>(origCaptureStream->GetSqBaseAddr() +
        (rtNotifyRecord->pos * sizeof(rtStarsSqe_t)));
    uint64_t realSize = 0U;
    auto error = MemcopyAsync(
        targetAddrOfUpdatedSqe, sizeof(rtStarsSqe_t), sqeMem, sizeof(sqeMem), RT_MEMCPY_HOST_TO_DEVICE_EX, exeStream,
        &realSize);
    COND_RETURN_ERROR(error != RT_ERROR_NONE, error, "update task fail error=0x%x", error);
    RT_LOG(RT_LOG_WARNING, "exec stream_id=%d target stream_id=%u pos=%u",
        exeStream->Id_(), origCaptureStream->Id_(), rtNotifyRecord->pos);
    return error;
}

rtError_t Context::AllocCascadeCaptureStream(const Stream * const stm, Model *const captureModel, Stream **newCaptureStream)
{
    Stream *newCaptureStreamTmp = nullptr;

    if (captureModel == nullptr) {
        RT_LOG(RT_LOG_ERROR, "Capture model is null, device_id=%u, original stream_id=%d.",
            device_->Id_(), stm->Id_());
        return RT_ERROR_MODEL_NULL;
    }

    CaptureModel *captureModelTmp = dynamic_cast<CaptureModel *>(captureModel);
    const uint32_t flag = GetCaptureStreamFlag();
    /* create capture stream */
    rtError_t error = StreamCreate(0U, flag, &newCaptureStreamTmp, nullptr, captureModelTmp->IsSoftwareSqEnable());
    if (error != RT_ERROR_NONE) {
        RT_LOG(RT_LOG_ERROR, "Capture stream create failed, device_id=%u, original stream_id=%d, retCode=%#x.",
            device_->Id_(), stm->Id_(), error);
        return error;
    }

    if (captureModelTmp->IsSoftwareSqEnable()) {
        /* add stream to model */
        error = ModelAddStream(captureModel, newCaptureStreamTmp, RT_INVALID_FLAG);
    } else {
        /* bind stream to model */
        error = ModelBindStream(captureModel, newCaptureStreamTmp, RT_INVALID_FLAG);
    }

    if (error != RT_ERROR_NONE) {
        RT_LOG(RT_LOG_ERROR, "Bind capture stream failed, device_id=%u, model_id=%u, stream_id=%d, "
            "original stream_id=%d, retCode=%#x.",
            device_->Id_(), captureModel->Id_(), newCaptureStreamTmp->Id_(), stm->Id_(), error);
        StreamDestroy(newCaptureStreamTmp);
        return error;
    }

    *newCaptureStream = newCaptureStreamTmp;

    return RT_ERROR_NONE;
}

void Context::FreeCascadeCaptureStream(Stream * const cascadeCaptureStm)
{
    if (cascadeCaptureStm == nullptr) {
        RT_LOG(RT_LOG_ERROR, "cascade capture stream is null.");
        return;
    }

    if (cascadeCaptureStm->Model_() != nullptr) {
        CaptureModel *captureModelTmp = dynamic_cast<CaptureModel *>(cascadeCaptureStm->Model_());
        if (captureModelTmp->IsSoftwareSqEnable()) {
            /* steam is add to model, only need del from model */
            (void)ModelDelStream(cascadeCaptureStm->Model_(), cascadeCaptureStm);
        } else {
            /* steam is bind to model, only need unbind from model */
            (void)ModelUnbindStream(cascadeCaptureStm->Model_(), cascadeCaptureStm);
        }
    }

    (void)StreamDestroy(cascadeCaptureStm, true);

    return;
}

rtError_t Context::StreamAddToCaptureModelProc(Stream * const stm, Model * const captureMdl, const bool isOriginal)
{
    Stream *captureStream = nullptr;
    const int32_t streamId = stm->Id_();
    if (captureMdl == nullptr) {
        RT_LOG(RT_LOG_ERROR, "Capture model is null, device_id=%u, original stream_id=%d.", device_->Id_(), streamId);
        return RT_ERROR_MODEL_NULL;
    }

    if (captureMdl->GetModelType() != RT_MODEL_CAPTURE_MODEL) {
        RT_LOG(RT_LOG_ERROR, "model type not match, device_id=%u, original stream_id=%d, model_id=%u.",
            device_->Id_(), streamId, captureMdl->Id_());
        return RT_ERROR_INVALID_VALUE;
    }

    CaptureModel *captureModelTmp = dynamic_cast<CaptureModel *>(captureMdl);
    if (captureModelTmp->IsCaptureFinish() || captureModelTmp->IsCaptureInvalid()) {
        RT_LOG(RT_LOG_ERROR, "model capture status mismatch, device_id=%u, original stream_id=%d, model_id=%u.",
            device_->Id_(), streamId, captureMdl->Id_());
        return RT_ERROR_MODEL_CAPTURE_STATUS;
    }

    const uint32_t flag = GetCaptureStreamFlag();
    /* create capture stream */
    rtError_t error = StreamCreate(0U, flag, &captureStream, nullptr, captureModelTmp->IsSoftwareSqEnable());
    if (error != RT_ERROR_NONE) {
        RT_LOG(RT_LOG_ERROR, "Capture stream create failed, device_id=%u, original stream_id=%d, retCode=%#x.",
            device_->Id_(), streamId, error);
        return error;
    }

    if (captureModelTmp->IsSoftwareSqEnable()) {
        /* add stream to model */
        error = ModelAddStream(captureMdl, captureStream, RT_HEAD_STREAM);
    } else {
        /* bind stream to model */
        error = ModelBindStream(captureMdl, captureStream, RT_HEAD_STREAM);
    }

    if (error != RT_ERROR_NONE) {
        RT_LOG(RT_LOG_ERROR, "Bind capture stream failed, device_id=%u, model_id=%u, stream_id=%d, "
            "original stream_id=%d, retCode=%#x.",
            device_->Id_(), captureMdl->Id_(), captureStream->Id_(), streamId, error);
        (void)StreamDestroy(captureStream);
        return error;
    }

    /* stm begin capture */
    const rtStreamCaptureStatus status = stm->GetCaptureStatus();
    /* check capture status again */
    if (status != RT_STREAM_CAPTURE_STATUS_NONE) {
        RT_LOG(RT_LOG_ERROR, "stream is already in capture status, device_id=%u, stream_id=%d, status=%s.",
            device_->Id_(), streamId, ((status == RT_STREAM_CAPTURE_STATUS_ACTIVE) ? "active" : "invalidated"));

        if (captureModelTmp->IsSoftwareSqEnable()) {
            /* steam is add to model, only need destroy model */
            (void)ModelDelStream(captureMdl, captureStream);
        } else {
            /* steam is bind to model, only need destroy model */
            (void)ModelUnbindStream(captureMdl, captureStream);
        }
        (void)StreamDestroy(captureStream);
        return RT_ERROR_STREAM_CAPTURED;
    }

    captureStream->MarkOrigCaptureStream(isOriginal);
    stm->EnterCapture(captureStream);
    return RT_ERROR_NONE;
}

bool Context::IsCaptureModeSupport(void) const
{
    const rtStreamCaptureMode contextCaptureMode = GetContextCaptureMode();
    /* no capture scene, support */
    if (contextCaptureMode == RT_STREAM_CAPTURE_MODE_MAX) {
        return true;
    }

    const rtStreamCaptureMode threadCaptureMode = InnerThreadLocalContainer::GetThreadCaptureMode();
    const rtStreamCaptureMode exchangeCaptureMode = InnerThreadLocalContainer::GetThreadExchangeCaptureMode();
    if (exchangeCaptureMode == RT_STREAM_CAPTURE_MODE_RELAXED) {
        return true;
    }

    if (exchangeCaptureMode == RT_STREAM_CAPTURE_MODE_GLOBAL) {
        if ((contextCaptureMode == RT_STREAM_CAPTURE_MODE_GLOBAL) ||
            (threadCaptureMode == RT_STREAM_CAPTURE_MODE_THREAD_LOCAL)) {
            return false;
        }
    }

    if (exchangeCaptureMode == RT_STREAM_CAPTURE_MODE_THREAD_LOCAL) {
        if ((threadCaptureMode == RT_STREAM_CAPTURE_MODE_GLOBAL) ||
            (threadCaptureMode == RT_STREAM_CAPTURE_MODE_THREAD_LOCAL)) {
            return false;
        }
    }

    return true;
}

rtError_t Context::StreamBeginCapture(Stream * const stm, const rtStreamCaptureMode mode)
{
    Model *captureModel = nullptr;

    BufferAllocator::OpenHugeBuff();

    const rtStreamCaptureStatus status = stm->GetCaptureStatus();
    const int32_t streamId = stm->Id_();

    RT_LOG(RT_LOG_INFO, "capture begin, device_id=%u, original stream_id=%d.", device_->Id_(), streamId);

    /* check capture status */
    if (status != RT_STREAM_CAPTURE_STATUS_NONE) {
        RT_LOG(RT_LOG_ERROR, "stream is already in capture status, device_id=%u, stream_id=%d, status=%s.",
            device_->Id_(), streamId, ((status == RT_STREAM_CAPTURE_STATUS_ACTIVE) ? "active" : "invalidated"));
        return RT_ERROR_STREAM_CAPTURED;
    }

    /* create capture model */
    rtError_t error = ModelCreate(&captureModel, RT_MODEL_CAPTURE_MODEL);
    if (error != RT_ERROR_NONE) {
        RT_LOG(RT_LOG_ERROR, "Capture model create failed, device_id=%u, original stream_id=%d, retCode=%#x.",
            device_->Id_(), streamId, error);
        return error;
    }

    if ((stm->Device_()->IsSupportFeature(RtOptionalFeatureType::RT_FEATURE_MODEL_ACL_GRAPH_SOFTWARE_ENABLE)) && 
        (stm->Device_()->CheckFeatureSupport(TS_FEATURE_SOFTWARE_SQ_ENABLE)) &&
        (NpuDriver::CheckIsSupportFeature(device_->Id_(), FEATURE_TRSDRV_SQ_SUPPORT_DYNAMIC_BIND)) &&
        (!Runtime::Instance()->GetConnectUbFlag())) {
        CaptureModel *captureModelTmp = dynamic_cast<CaptureModel *>(captureModel);
        captureModelTmp->SetSoftwareSqEnable();
    }

    std::unique_lock<std::mutex> taskLock(captureLock_);
    error = StreamAddToCaptureModelProc(stm, captureModel, true);
    if (error != RT_ERROR_NONE) {
        RT_LOG(RT_LOG_ERROR, "add stream to capture model failed, device_id=%u, model_id=%u, "
            "original stream_id=%d, retCode=%#x.",
            device_->Id_(), captureModel->Id_(), streamId, error);
        ModelDestroy(captureModel);
        return error;
    }

    CaptureModeEnter(stm, mode);

    RT_LOG(RT_LOG_EVENT, "capture begin success, device_id=%u, model_id=%u,"
 	    " original stream_id=%d, capture stream_id=%d, stream_status=%d.",
 	    device_->Id_(), captureModel->Id_(), streamId, stm->GetCaptureStream()->Id_(), stm->GetCaptureStatus());

    return RT_ERROR_NONE;
}

rtError_t Context::CheckCaptureModelValidity(Model * const captureMdl) const
{
    NULL_PTR_RETURN(captureMdl, RT_ERROR_MODEL_NULL);
    std::list<Stream *> streams = captureMdl->StreamList_();
    COND_RETURN_ERROR((streams.empty() == true),
        RT_ERROR_STREAM_UNJOINED,
        "No streams is bound to the capture module, model_id=%u.",
        captureMdl->Id_());

    CaptureModel * const mdl = dynamic_cast<CaptureModel * const>(captureMdl);
    std::set<uint16_t> & streamIds = mdl->GetTaskGroupStreamIds();
    COND_RETURN_ERROR((!streamIds.empty()),
        RT_ERROR_STREAM_TASKGRP_STATUS,
        "A task group is not closed in the capture module, model_id=%u.",
        captureMdl->Id_());

    bool isOnlyOrigStream = true;
    bool hasRecordOrigStream = false;
    for (auto it = streams.begin(); it != streams.end(); it++) {
        if (((*it)->IsOrigCaptureStream()) ||
            ((*it)->IsLastLevelCaptureStream() == false) ||
            (mdl->IsAddStream(*it))) {
            continue;
        }

        isOnlyOrigStream = false;
        const int32_t streamId = (*it)->Id_();
        const uint32_t taskId = (*it)->GetLastTaskId();
        Event *event = nullptr;
        CaptureCntNotify cntInfo = {0, 0U};
        const rtError_t ret = GetCaptureEventFromTask(device_, static_cast<uint32_t>(streamId), taskId, event, cntInfo);
        COND_RETURN_WITH_NOLOG((ret != RT_ERROR_NONE), ret);
        COND_RETURN_ERROR((event == nullptr),
            RT_ERROR_EVENT_NULL,
            "No event object, stream_id=%d, task_id=%u.",
            streamId, taskId);
        COND_RETURN_ERROR((event->GetEventFlag() == RT_EVENT_EXTERNAL),
            RT_ERROR_STREAM_UNJOINED,
            "The event flag is external, stream_id=%d, task_id=%u, event_id=%d.",
            streamId, taskId, event->EventId_());
        COND_RETURN_ERROR((event->IsCaptureStreamWaited() == false),
            RT_ERROR_STREAM_UNJOINED,
            "A free-state event record task was discovered, stream_id=%d, task_id=%u, event_id=%d.",
            streamId, taskId, event->EventId_());
        if (event->IsRecordOrigCaptureStream(*it)) {
            hasRecordOrigStream = true;
        }
    }
    COND_RETURN_ERROR(((isOnlyOrigStream == false) && (hasRecordOrigStream == false)), RT_ERROR_STREAM_UNJOINED,
        "capture model contains a stream that was not joined to the original stream.");
    return RT_ERROR_NONE;
}

rtError_t Context::CreateNotify(Notify **notify, uint32_t flag)
{
    const uint32_t deviceId = device_->Id_();
    *notify = new (std::nothrow) Notify(deviceId, device_->DevGetTsId());
    COND_RETURN_ERROR_MSG_CALL(ERR_MODULE_SYSTEM, *notify == nullptr, RT_ERROR_NOTIFY_NEW,
                               "Notify create failed, new notify failed.");

    (*notify)->SetNotifyFlag(flag);
    const rtError_t error = (*notify)->Setup();
    ERROR_PROC_RETURN_MSG_INNER(error, DELETE_O(*notify);,
        "Notify create failed, setup failed, device_id=%d, retCode=%#x", deviceId, static_cast<uint32_t>(error));

    return RT_ERROR_NONE;
}

rtError_t Context::AddNotifyToAddedCaptureStream(Stream * const oriSingleStm, CaptureModel * const captureMdl)
{
    rtError_t error = RT_ERROR_NONE;
    auto &streams = captureMdl->GetAddStreamMap();
    Api * const apiObj = Runtime::Instance()->ApiImpl_();
    NULL_PTR_RETURN_MSG(apiObj, RT_ERROR_API_NULL);
    for (auto &streamObj : streams) {
        Notify *notify = nullptr;
        error = CreateNotify(&notify, RT_NOTIFY_DEFAULT);
        COND_RETURN_WITH_NOLOG((error != RT_ERROR_NONE), error); // notify在模型销毁时，统一进行释放
        captureMdl->AddNotify(notify);
        Stream *const lastStm = streamObj.second.back();
 	    error = apiObj->NotifyRecord(notify, lastStm);
        ERROR_RETURN(error,
            "Notify record failed, device_id=%u, single stream_id=%d, "
            "capture model_id=%u, stream_id=%d, notify_id=%u, retCode=%#x",
            device_->Id_(), oriSingleStm->Id_(), captureMdl->Id_(),
            streamObj.second.back()->Id_(), notify->GetNotifyId(), error);
        TaskInfo *task = device_->GetTaskFactory()->GetTask(lastStm->Id_(), lastStm->GetLastTaskId());
        if (task != nullptr) {
            task->modelSeqId = captureMdl->GenerateSeqId();
            RT_LOG(RT_LOG_INFO, "Alloc task sequence id=%u, device id=%u, stream_id=%d, task_id=%u",
                task->modelSeqId, device_->Id_(), lastStm->Id_(), lastStm->GetLastTaskId());
        }
        error = apiObj->NotifyWait(notify, oriSingleStm, MAX_UINT32_NUM);
        ERROR_RETURN(error,
            "Notify wit failed, device_id=%u, single stream_id=%d, "
            "capture model_id=%u, stream_id=%d, notify_id=%u, retCode=%#x",
            device_->Id_(), oriSingleStm->Id_(), captureMdl->Id_(),
            streamObj.second.back()->Id_(), notify->GetNotifyId(), error);
    }
    return error;
}

rtError_t Context::SetNotifyForExeModel(CaptureModel* const captureMdl)
{
    /* for exe stream and add stream alloc notify */
    rtError_t error = RT_ERROR_NONE;
    const std::map<Stream *, std::vector<Stream *>> &streams = captureMdl->GetAddStreamMap();
    for (size_t i = 0U; i < (streams.size() * NOTIFY_INDEX); i++) {
        Notify *notify = nullptr;
        error = CreateNotify(&notify, RT_NOTIFY_DEFAULT);
        COND_RETURN_WITH_NOLOG((error != RT_ERROR_NONE), error);
        captureMdl->AddExeNotify(notify);
        RT_LOG(RT_LOG_DEBUG, "create notify id=%u", notify->GetNotifyId());
    }
    return error;
}

static inline void ClearCaptureModel(Context * const ctx, Stream * const stm, Model *mdl = nullptr)
{
    stm->ExitCapture();
    /* steam is bound to model, only need destroy model */
    if (mdl != nullptr) {
        (void)ctx->ModelDestroy(mdl);
    }
}

rtError_t Context::StreamEndCapture(Stream * const stm, Model ** const captureMdl)
{
    RT_LOG(RT_LOG_INFO, "capture end, device_id=%u, original stream_id=%d.", device_->Id_(), stm->Id_());
    *captureMdl = nullptr;
    std::unique_lock<std::mutex> taskLock(captureLock_);
    const rtStreamCaptureStatus status = stm->GetCaptureStatus();
    /* check capture status */
    if (status == RT_STREAM_CAPTURE_STATUS_NONE) {
        RT_LOG(RT_LOG_ERROR, "stream is not in capture status, device_id=%u, origin stream_id=%d, status=NONE.",
            device_->Id_(), stm->Id_());
        return RT_ERROR_STREAM_NOT_CAPTURED;
    }

    Stream *captureStream = stm->GetCaptureStream();
    NULL_STREAM_PTR_RETURN_MSG(captureStream);
    if (!(captureStream->IsOrigCaptureStream())) {
        RT_LOG(RT_LOG_ERROR, "The capture was not initiated in this stream. device_id=%u, stream_id=%d.",
            device_->Id_(), stm->Id_());
        return RT_ERROR_STREAM_CAPTURE_UNMATCHED;
    }

    rtError_t error = CheckCaptureStreamThreadIsMatch(stm);
    COND_PROC_RETURN_ERROR(error != RT_ERROR_NONE, error,
        CaptureModeExit(stm);
        ClearCaptureModel(this, stm);,
        "end capture in the wrong thread.");
    CaptureModeExit(stm);

    Model *captureModel = captureStream->Model_();
    if (captureModel == nullptr) {
        RT_LOG(RT_LOG_ERROR, "captureModel is null, device_id=%u, origin stream_id=%d, status=%s.",
            device_->Id_(), stm->Id_(), ((status == RT_STREAM_CAPTURE_STATUS_ACTIVE) ? "active" : "invalidated"));
        ClearCaptureModel(this, stm);
        return RT_ERROR_MODEL_NULL;
    }

    CaptureModel *captureModelTmp = RtPtrToPtr<CaptureModel *, Model *>(captureModel);
    if (status == RT_STREAM_CAPTURE_STATUS_INVALIDATED ||
        captureModelTmp->IsCaptureInvalid()) {
        RT_LOG(RT_LOG_ERROR, "current capture is invalid, device_id=%u, origin stream_id=%d.",
            device_->Id_(), stm->Id_());
        ClearCaptureModel(this, stm, captureModel);
        return RT_ERROR_STREAM_CAPTURE_INVALIDATED;
    }

    error = CheckCaptureModelValidity(captureModel);
    COND_PROC_RETURN_ERROR(error != RT_ERROR_NONE, error,
        ClearCaptureModel(this, stm, captureModel),
        "Failed to verify the validity of the capture model, retCode=%#x.", static_cast<uint32_t>(error));

    captureStream = stm->GetCaptureStream();
    NULL_PTR_PROC_RETURN_ERROR(captureStream, RT_ERROR_STREAM_NULL,
        ClearCaptureModel(this, stm, captureModel));
    error = AddNotifyToAddedCaptureStream(stm, static_cast<CaptureModel *>(captureModelTmp));
    if (error != RT_ERROR_NONE) {
        RT_LOG(RT_LOG_ERROR, "set notify for add capture stream failed, device_id=%u, origin stream_id=%d, "
            "capture model_id=%u, stream_id=%d, retCode=%#x.",
            device_->Id_(), stm->Id_(), captureModel->Id_(), captureStream->Id_(), error);
        ClearCaptureModel(this, stm, captureModel);
        return error;
    }

    error = SetNotifyForExeModel(captureModelTmp);
    if (error != RT_ERROR_NONE) {
        RT_LOG(RT_LOG_ERROR, "set notify for capture model failed, device_id=%u, origin stream_id=%d, "
            "capture model_id=%u, stream_id=%d, retCode=%#x.",
            device_->Id_(), stm->Id_(), captureModel->Id_(), captureStream->Id_(), error);
        ClearCaptureModel(this, stm, captureModel);
        return error;
    }

    error = captureModelTmp->ResetCaptureEvents(stm);
    COND_PROC_RETURN_ERROR(error != RT_ERROR_NONE, error, ClearCaptureModel(this, stm, captureModel),
        "Failed to reset capture events, retCode=%#x.", static_cast<uint32_t>(error));

    if (!captureModelTmp->IsSoftwareSqEnable()) {
        Api * const apiObj = Runtime::Instance()->ApiImpl_();
        error = apiObj->ModelEndGraph(captureModel, captureStream, 0U);
        COND_PROC_RETURN_ERROR(error != RT_ERROR_NONE, error, ClearCaptureModel(this, stm, captureModel),
            "capture model end graph failed, device_id=%u, origin stream_id=%d, "
            "capture model_id=%u, stream_id=%d, retCode=%#x.",
            device_->Id_(), stm->Id_(), captureModel->Id_(), captureStream->Id_(), error);
        error = captureModel->LoadComplete();
        COND_PROC_RETURN_ERROR(error != RT_ERROR_NONE, error, ClearCaptureModel(this, stm, captureModel),
            "capture model load complete failed, device_id=%u, origin stream_id=%d, "
            "capture model_id=%u, stream_id=%d, retCode=%#x.",
            device_->Id_(), stm->Id_(), captureModel->Id_(), captureStream->Id_(), error);
    }

    (void)captureModel->ModelExecuteType();
    /* stm end capture */
    stm->ExitCapture();
    *captureMdl = captureModel;

    RT_LOG(RT_LOG_EVENT,
        "capture end success, device_id=%u, original stream_id=%d, capture stream_id=%d, capture model_id=%u.",
        device_->Id_(), stm->Id_(), captureStream->Id_(), captureModel->Id_());

    return RT_ERROR_NONE;
}

rtError_t Context::StreamGetCaptureInfo(const Stream * const stm, rtStreamCaptureStatus * const status,
                                        Model ** const captureMdl) const
{
    Stream *captureStream = stm->GetCaptureStream();
    const rtStreamCaptureStatus statusTmp = stm->GetCaptureStatus();
    Model *mdlTmp = nullptr;
    uint32_t modelId = 0xFFFFU;

    if ((statusTmp != RT_STREAM_CAPTURE_STATUS_NONE) && (captureStream != nullptr)) {
        mdlTmp = captureStream->Model_();
        if (mdlTmp == nullptr) {
            RT_LOG(RT_LOG_WARNING, "stream is not in capture status, device_id=%u, stream_id=%d.",
                device_->Id_(), stm->Id_());
        } else {
 	        modelId = mdlTmp->Id_();
 	    }
    }

    if (status != nullptr) {
        *status = statusTmp;
    }

    if (captureMdl != nullptr) {
        *captureMdl = mdlTmp;
    }

    RT_LOG(RT_LOG_INFO, "device_id=%u, model_id=%u, original stream_id=%d, stream_status=%d",
 	    device_->Id_(), modelId, stm->Id_(), statusTmp);

    return RT_ERROR_NONE;
}

rtError_t Context::ModelGetNodes(const Model * const mdl, uint32_t * const num)
{
    /* model can not bind or unbind while get nodes */
    std::unique_lock<std::mutex> taskLock(streamLock_);
    *num = mdl->ModelGetNodes();
    return RT_ERROR_NONE;
}

rtError_t Context::ModelDebugDotPrint(const Model * const mdl)
{
    std::unique_lock<std::mutex> taskLock(streamLock_);
    return mdl->ModelDebugDotPrint();
}

rtError_t Context::ModelDebugJsonPrint(const Model * const mdl, const char* path, const uint32_t flags)
{
    std::unique_lock<std::mutex> taskLock(streamLock_);
    return mdl->ModelDebugJsonPrint(path, flags);
}

rtError_t Context::StreamAddToModel(Stream * const stm, Model * const captureMdl)
{
    std::unique_lock<std::mutex> taskLock(captureLock_);
    return static_cast<CaptureModel *>(captureMdl)->AddStreamToCaptureModel(stm);
}

rtError_t Context::ThreadExchangeCaptureMode(rtStreamCaptureMode * const mode) const
{
    const rtStreamCaptureMode exchangeCaptureModeOld = InnerThreadLocalContainer::GetThreadExchangeCaptureMode();

    /* set new mode */
    InnerThreadLocalContainer::SetThreadExchangeCaptureMode(*mode);

    /* return old mode */
    *mode = exchangeCaptureModeOld;

    return RT_ERROR_NONE;
}

void Context::CaptureModeEnter(Stream * const stm, rtStreamCaptureMode mode)
{
    stm->SetStreamCaptureMode(mode);
    stm->SetBeginCaptureThreadId(runtime::GetCurrentTid());
    captureModeRefNum_[mode]++;
    InnerThreadLocalContainer::ThreadCaptureModeEnter(mode);

    /* mode, 0: global; 1: thread; 2: relax; 3: max */
    if (mode < GetContextCaptureMode()) {
        SetContextCaptureMode(mode);
    }
}

void Context::CaptureModeExit(Stream * const stm)
{
    const rtStreamCaptureMode streamCaptureMode = stm->GetStreamCaptureMode();
    stm->SetStreamCaptureMode(RT_STREAM_CAPTURE_MODE_MAX);
    stm->SetBeginCaptureThreadId(UINT32_MAX);

    /* end capture is already finished in this stm */
    if (static_cast<uint32_t>(streamCaptureMode) >= RT_STREAM_CAPTURE_MODE_MAX) {
        return;
    }

    if (captureModeRefNum_[streamCaptureMode] > 0U) {
        captureModeRefNum_[streamCaptureMode]--;
    }

    InnerThreadLocalContainer::ThreadCaptureModeExit(streamCaptureMode);

    if (GetCaptureModeRefNum(RT_STREAM_CAPTURE_MODE_GLOBAL) != 0U) {
        return;
    }

    if (GetCaptureModeRefNum(RT_STREAM_CAPTURE_MODE_THREAD_LOCAL) != 0U) {
        SetContextCaptureMode(RT_STREAM_CAPTURE_MODE_THREAD_LOCAL);
        return;
    }

    if (GetCaptureModeRefNum(RT_STREAM_CAPTURE_MODE_RELAXED) != 0U) {
        SetContextCaptureMode(RT_STREAM_CAPTURE_MODE_RELAXED);
        return;
    }

    SetContextCaptureMode(RT_STREAM_CAPTURE_MODE_MAX);

    return;
}

rtError_t Context::StreamBeginTaskGrp(Stream * const stm)
{
    const std::lock_guard<std::mutex> tskGrpLock(stm->GetTaskGrpMutex());
    const StreamTaskGroupStatus status = stm->GetTaskGroupStatus();
    COND_RETURN_ERROR_MSG_INNER(status != StreamTaskGroupStatus::NONE,
        RT_ERROR_STREAM_TASKGRP_STATUS,
        "Task group is repeatedly started, or a task group is being updated.");

    Stream *captureStream = stm->GetCaptureStream();
    NULL_PTR_RETURN_MSG(captureStream, RT_ERROR_STREAM_NOT_CAPTURED);

    CaptureModel *mdl = dynamic_cast<CaptureModel *>(captureStream->Model_());
    NULL_PTR_RETURN(mdl, RT_ERROR_MODEL_NULL);

    std::unique_ptr<TaskGroup> taskGrp(new (std::nothrow) TaskGroup);
    COND_RETURN_ERROR_MSG_CALL(ERR_MODULE_SYSTEM, taskGrp == nullptr,
        RT_ERROR_MEMORY_ALLOCATION, "Create task group object failed.");

    const uint8_t prefetchCnt = stm->Device_()->GetDevProperties().taskPrefetchCount;
    for (uint8_t idx = 0; idx < prefetchCnt; idx++) {
        const rtError_t ret = SendNopTask(this, stm);
        ERROR_RETURN_MSG_INNER(ret, "Launch Nop task %u error %#x.", idx, static_cast<uint32_t>(ret));
    }

    captureStream = stm->GetCaptureStream();
    NULL_PTR_RETURN_MSG(captureStream, RT_ERROR_STREAM_NOT_CAPTURED);
    COND_RETURN_ERROR((mdl != dynamic_cast<CaptureModel *>(captureStream->Model_())),
        RT_ERROR_STREAM_CAPTURE_CONFLICT, "Capture model conflict.");
    (void)stm->UpdateTaskGroupStatus(StreamTaskGroupStatus::SAMPLE);
    captureStream->UpdateCurrentTaskGroup(taskGrp);
    mdl->InsertTaskGroupStreamId(static_cast<uint16_t>(captureStream->Id_()));
    return RT_ERROR_NONE;
}

rtError_t Context::StreamEndTaskGrp(Stream * const stm, TaskGroup ** const handle) const
{
    *handle = nullptr;
    const std::lock_guard<std::mutex> tskGrpLock(stm->GetTaskGrpMutex());
    const StreamTaskGroupStatus status = stm->GetTaskGroupStatus();
    COND_RETURN_ERROR_MSG_INNER(status != StreamTaskGroupStatus::SAMPLE,
        RT_ERROR_STREAM_TASKGRP_STATUS,
        "The end operation cannot be performed on a stream that has not started a task group.");

    Stream * const captureStream = stm->GetCaptureStream();
    NULL_PTR_RETURN(captureStream, RT_ERROR_STREAM_NOT_CAPTURED);

    CaptureModel *mdl = dynamic_cast<CaptureModel *>(captureStream->Model_());
    NULL_PTR_RETURN(mdl, RT_ERROR_MODEL_NULL);

    std::unique_ptr<TaskGroup> &taskGrp = captureStream->GetCurrentTaskGroup();
    NULL_PTR_RETURN(taskGrp, RT_ERROR_STREAM_TASKGRP_NULL);

    rtError_t errorCode = mdl->GetTaskGroupErrCode();
    if ((errorCode != RT_ERROR_NONE) || (mdl->IsCaptureInvalid()) ||
        (stm->GetCaptureStatus() == RT_STREAM_CAPTURE_STATUS_INVALIDATED)) {
        taskGrp.reset();
        *handle = nullptr;
        errorCode = (errorCode != RT_ERROR_NONE) ? errorCode : RT_ERROR_STREAM_CAPTURE_INVALIDATED;
    } else {
        *handle = taskGrp.get();
        mdl->AddTaskGroupList(taskGrp);
    }
    captureStream->ResetTaskGroup();
    mdl->DeleteTaskGroupStreamId(static_cast<uint16_t>(captureStream->Id_()));
    (void)stm->UpdateTaskGroupStatus(StreamTaskGroupStatus::NONE);
    return errorCode;
}

rtError_t Context::StreamBeginTaskUpdate(Stream * const stm, TaskGroup * handle) const
{
    const std::lock_guard<std::mutex> tskGrpLock(stm->GetTaskGrpMutex());
    COND_RETURN_OUT_ERROR_MSG_CALL(stm->GetTaskGroupStatus() != StreamTaskGroupStatus::NONE,
        RT_ERROR_STREAM_TASKGRP_STATUS, "Unable to start update tasks because the stream is busy.");

    COND_RETURN_ERROR_MSG_INNER(handle->isUpdate,
        RT_ERROR_STREAM_TASKGRP_STATUS, "The handle only can be updated by one stream.");

    const rtError_t ret = stm->UpdateTaskGroupStatus(StreamTaskGroupStatus::UPDATE);
    ERROR_RETURN(ret, "update stream task group status failed, ret:%#x, status:%d.",
        static_cast<uint32_t>(ret), stm->GetTaskGroupStatus());

    stm->SetUpdateTaskGroup(handle);
    RT_LOG(RT_LOG_INFO, "Success to begin update tasks, stream_id=%d.", stm->Id_());
    return RT_ERROR_NONE;
}

rtError_t Context::StreamEndTaskUpdate(Stream * const stm) const
{
    const std::lock_guard<std::mutex> tskGrpLock(stm->GetTaskGrpMutex());
    COND_RETURN_OUT_ERROR_MSG_CALL(stm->GetTaskGroupStatus() != StreamTaskGroupStatus::UPDATE,
        RT_ERROR_STREAM_TASKGRP_STATUS, "Unable to end update tasks because the stream is busy.");

    TaskGroup *updateTaskGroup = stm->GetUpdateTaskGroup();
    NULL_PTR_RETURN_MSG_OUTER(updateTaskGroup, RT_ERROR_INVALID_VALUE);
    (void)stm->UpdateTaskGroupStatus(StreamTaskGroupStatus::NONE);

    const size_t taskIndex = updateTaskGroup->updateTaskIndex;
    COND_PROC_RETURN_ERROR_MSG_INNER(taskIndex != updateTaskGroup->taskIds.size(),
        RT_ERROR_STREAM_TASKGRP_UPDATE, stm->ResetUpdateTaskGroup();,
        "Update tasks failed, stream_id=%d, total=%zu, success=%zu, failed=%zu.",
        stm->Id_(), updateTaskGroup->taskIds.size(), taskIndex, (updateTaskGroup->taskIds.size() - taskIndex));

    stm->ResetUpdateTaskGroup();
    RT_LOG(RT_LOG_INFO, "stream_id=%d update tasks result: total=%zu, success=%zu, remain=%zu",
        stm->Id_(), updateTaskGroup->taskIds.size(), taskIndex, (updateTaskGroup->taskIds.size() - taskIndex));
    return RT_ERROR_NONE;
}

} // namespace runtime
} // namespace cce
