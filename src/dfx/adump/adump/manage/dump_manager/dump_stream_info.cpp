/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "dump_stream_info.h"
namespace Adx {

static std::atomic<uint64_t> g_dumpNumber(0);

uint64_t GetNextDumpNumber()
{
    return g_dumpNumber.fetch_add(1);
}

void DumpResourceSafeMap::CleanupThreadLoop()
{
    IDE_LOGI("Cleanup thread started");
    while (cleanupThreadActive_.load()) {
        std::string key;
        {
            std::unique_lock<std::mutex> lock(cleanupMtx_);
            cleanupCv_.wait(lock, [this]() { return !cleanupQueue_.empty() || !cleanupThreadActive_.load(); });

            if (cleanupQueue_.empty() && !cleanupThreadActive_.load()) {
                IDE_LOGI("Cleanup thread exiting");
                return;
            }

            if (!cleanupQueue_.empty()) {
                key = cleanupQueue_.front();
                cleanupQueue_.pop();
            }
        }

        if (!key.empty()) {
            IDE_LOGI("Cleanup thread removing key: %s", key.c_str());
            remove(key);
        }
    }
}

void DumpResourceSafeMap::StartCleanupThread()
{
    if (cleanupThreadActive_.load()) {
        return;
    }

    std::lock_guard<std::mutex> lock(cleanupMtx_);
    if (cleanupThreadActive_.load()) {
        return;
    }
    cleanupThreadActive_.store(true);
    cleanupThread_ = std::thread(&DumpResourceSafeMap::CleanupThreadLoop, this);
    IDE_LOGI("Cleanup thread started");
}

void DumpResourceSafeMap::StopCleanupThread()
{
    {
        std::lock_guard<std::mutex> lock(cleanupMtx_);
        if (!cleanupThreadActive_.load()) {
            return;
        }
        cleanupThreadActive_.store(false);
    }
    cleanupCv_.notify_one();

    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }
    IDE_LOGI("Cleanup thread stopped");
}

void DumpResourceSafeMap::EnqueueCleanup(const std::string key)
{
    StartCleanupThread();
    {
        std::lock_guard<std::mutex> lock(cleanupMtx_);
        cleanupQueue_.push(key);
    }
    cleanupCv_.notify_one();
    IDE_LOGI("Enqueued key for cleanup: %s", key.c_str());
}

bool DumpResourceSafeMap::IsCleanupThreadActive()
{
    return cleanupThreadActive_.load();
}

int32_t DumpStreamCreate(DumpStreamInfo** ptr)
{
    if (ptr == nullptr) {
        IDE_LOGE("Dump stream create failed, ptr is null");
        return ADUMP_FAILED;
    }
    
    DumpStreamInfo* dumpPtr = new (std::nothrow) DumpStreamInfo();
    if (dumpPtr == nullptr) {
        IDE_LOGE("Dump stream malloc failed");
        *ptr = nullptr;
        return ADUMP_FAILED;
    }
    
    aclError ret = aclrtCreateEventExWithFlag(&(dumpPtr->mainStmEvt), ACL_EVENT_SYNC);
    if (ret != ACL_SUCCESS) {
        IDE_LOGE("create main stream event failed");
        delete dumpPtr;
        return ret;
    }
    
    ret = aclrtCreateEventExWithFlag(&(dumpPtr->dumpStmEvt), ACL_EVENT_SYNC);
    if (ret != ACL_SUCCESS) {
        IDE_LOGE("create dump stream event failed");
        aclrtDestroyEvent(dumpPtr->mainStmEvt);
        delete dumpPtr;
        return ret;
    }
    
    ret = aclrtCreateStream(&(dumpPtr->stm));
    if (ret != ACL_SUCCESS) {
        IDE_LOGE("create dump stream failed");
        aclrtDestroyEvent(dumpPtr->mainStmEvt);
        aclrtDestroyEvent(dumpPtr->dumpStmEvt);
        delete dumpPtr;
        return ret;
    }
    
    *ptr = dumpPtr;
    return ADUMP_SUCCESS;
}

void DumpStreamFree(DumpStreamInfo* ptr)
{
    if (!ptr) {
        return;
    }
    if (ptr->mainStmEvt) {
        aclrtDestroyEvent(ptr->mainStmEvt);
    }
    if (ptr->dumpStmEvt) {
        aclrtDestroyEvent(ptr->dumpStmEvt);
    }
    if (ptr->stm) {
        aclrtDestroyStream(ptr->stm);
    }
    ptr->inputTensors.clear();
    ptr->outputTensors.clear();
}

std::string GenerateDumpFileName(const DumpStreamInfo* dumpInfoPtr)
{
    const std::string& dumpPath = dumpInfoPtr->dumpPath;
    const std::string& opType = dumpInfoPtr->opType;
    const std::string& opName = dumpInfoPtr->opName;
    uint32_t taskId = dumpInfoPtr->taskId;
    uint32_t streamId = dumpInfoPtr->streamId;
    uint32_t contextId = dumpInfoPtr->contextId;
    uint32_t threadId = dumpInfoPtr->threadId;
    uint32_t deviceId = dumpInfoPtr->deviceId;
    uint64_t timestamp = dumpInfoPtr->timestamp;
    uint64_t dumpNumber = dumpInfoPtr->dumpNumber;

    std::ostringstream fileNameoss;
    if (!dumpPath.empty() && dumpPath.back() == '/') {
        fileNameoss << dumpPath << deviceId << "/" << opType << "." << opName << "." << dumpNumber << "." 
                    << taskId << "." << streamId << "." << timestamp;
    } else {
        fileNameoss << dumpPath << "/" << deviceId << "/" << opType << "." << opName << "." << dumpNumber << "."
                    << taskId << "." << streamId << "." << timestamp;
    }
    if (contextId != 0 && threadId != 0) {
        fileNameoss << ".FFTSPLUS." << contextId << "." << threadId << "." << deviceId;
    }
    return fileNameoss.str();
}

void FillTensorProtoInfo(const std::vector<DumpTensor>& tensors, toolkit::dump::DumpData& data, bool isInput)
{
    for (size_t i = 0; i < tensors.size(); i++) {
        const DumpTensor& item = tensors[i];
        int32_t format = item.GetFormat();

        if (isInput) {
            auto* opInput = data.add_input();
            opInput->set_data_type(static_cast<toolkit::dump::OutputDataType>(item.GetDataType()));
            opInput->set_format(static_cast<toolkit::dump::OutputFormat>(GetPrimaryFormat(format)));
            opInput->set_sub_format(GetSubFormat(format));
            opInput->set_offset(item.GetArgsOffSet());
            opInput->set_size(item.GetSize());

            auto* shape = opInput->mutable_shape();
            for (auto dim : item.GetShape()) {
                shape->add_dim(dim);
            }
            auto* originShape = opInput->mutable_original_shape();
            for (auto dim : item.GetOriginShape()) {
                originShape->add_dim(dim);
            }
        } else {
            auto* opOutput = data.add_output();
            opOutput->set_data_type(static_cast<toolkit::dump::OutputDataType>(item.GetDataType()));
            opOutput->set_format(static_cast<toolkit::dump::OutputFormat>(GetPrimaryFormat(format)));
            opOutput->set_sub_format(GetSubFormat(format));
            opOutput->set_size(item.GetSize());

            auto* shape = opOutput->mutable_shape();
            for (auto dim : item.GetShape()) {
                shape->add_dim(dim);
            }
            auto* originShape = opOutput->mutable_original_shape();
            for (auto dim : item.GetOriginShape()) {
                originShape->add_dim(dim);
            }
        }
    }
}

toolkit::dump::DumpData BuildDumpDataProto(const DumpStreamInfo* dumpInfoPtr)
{
    toolkit::dump::DumpData dumpData;
    dumpData.set_version("2.0");
    dumpData.set_dump_time(dumpInfoPtr->timestamp);
    dumpData.set_op_name(dumpInfoPtr->opName);

    FillTensorProtoInfo(dumpInfoPtr->inputTensors, dumpData, true);
    FillTensorProtoInfo(dumpInfoPtr->outputTensors, dumpData, false);

    return dumpData;
}

size_t CalculateTensorDataSize(const std::vector<DumpTensor>& tensors)
{
    size_t totalSize = 0;
    for (const auto& tensor : tensors) {
        totalSize += tensor.GetSize();
    }
    return totalSize;
}

int32_t CopyTensorDataToBuffer(const std::vector<DumpTensor>& tensors, char* dataPtr, size_t remainingSize)
{
    for (size_t i = 0; i < tensors.size(); i++) {
        const DumpTensor& item = tensors[i];
        void* hostData = DumpMemory::CopyDeviceToHost(item.GetAddress(), item.GetSize());
        if (hostData == nullptr) {
            IDE_LOGE("Copy device to host failed, ptr: %p, size: %lu bytes.", item.GetAddress(), item.GetSize());
            return ADUMP_FAILED;
        }
        errno_t ret = memcpy_s(dataPtr, remainingSize, hostData, item.GetSize());
        if (ret != EOK) {
            IDE_LOGE(
                "Copy tensor data to dump buffer failed, dst ptr: %p, remain size: %lu bytes, src ptr: %p, size: %lu "
                "bytes.",
                dataPtr, remainingSize, hostData, item.GetSize());
            return ADUMP_FAILED;
        }
        dataPtr += item.GetSize();
        remainingSize -= item.GetSize();
        HOST_RT_MEMORY_GUARD(hostData);
    }
    return ADUMP_SUCCESS;
}

} // namespace Adx