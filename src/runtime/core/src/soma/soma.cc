/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "soma.hpp"
#include "stream_mem_pool.hpp"

namespace cce {
namespace runtime {

rtError_t SomaApi::StreamMemPoolCreate(rtMemPool_t *memPool, const rtMemPoolProps *poolProps)
{
    rtMemPoolProps curPoolProps = *poolProps;
    size_t freeSize = 0;
    size_t totalSize = 0;
    uint32_t phyDevId;
    rtError_t error = Runtime::Instance()->ChgUserDevIdToDeviceId(curPoolProps.devId, &phyDevId, false);
    ERROR_RETURN_MSG_INNER(error, "Convert user devId to physical devId failed! userDevId=%u, error Code=%u!",
        curPoolProps.devId, static_cast<uint32_t>(error));

    error = Runtime::Instance()->CurrentContext()->Device_()->Driver_()->MemGetInfoEx(phyDevId, 
        RT_MEMORYINFO_HBM, &freeSize, &totalSize);
    ERROR_RETURN_MSG_INNER(error, "Get device totalSize failed, deviceId=%u, retCode=%#x!",
        phyDevId, static_cast<uint32_t>(error));

    SegmentManager *retMemPool = nullptr;
    error = SomaApi::CreateMemPool(curPoolProps, totalSize, retMemPool);
    ERROR_RETURN(error, "Failed to create MemPool, size=%zu, error code=%u", curPoolProps.maxSize, error);

    RT_LOG(RT_LOG_DEBUG, "Allocate segment with basePtr %p and size %zu.", retMemPool->PoolSegAddr(), retMemPool->PoolSize());
    error = Runtime::Instance()->CurrentContext()->Device_()->Driver_()->StreamMemPoolCreate(retMemPool->DeviceId(),
        retMemPool->MemPoolId(), retMemPool->PoolSegAddr(), retMemPool->PoolSize(), false);
    if (error != RT_ERROR_NONE) {
        (void)SomaApi::DestroyMemPool(retMemPool);
        RT_LOG(RT_LOG_ERROR, "stream mem pool alloc failed in drv.");
        return error;
    }

    *memPool = RtPtrToPtr<rtMemPool_t>(retMemPool);
    RT_LOG(RT_LOG_DEBUG, "stream mem pool create success, memPool=%p.", memPool);
    return RT_ERROR_NONE;
}

rtError_t SomaApi::StreamMemPoolDestroy(const rtMemPool_t memPool)
{
    rtError_t error;
    SegmentManager *delMemPool = RtPtrToPtr<SegmentManager*>(memPool);
    error = SomaApi::CheckMemPool(delMemPool);
    ERROR_RETURN(error, "Failed to destroy memPoolId=%p", delMemPool);
    error = Runtime::Instance()->CurrentContext()->Device_()->Driver_()->StreamMemPoolDestroy(
        delMemPool->DeviceId(), delMemPool->MemPoolId());
    ERROR_RETURN(error, "Failed to destroy memPoolId in device, deviceId=",
        delMemPool->DeviceId(), delMemPool->MemPoolId());
    (void)SomaApi::DestroyMemPool(delMemPool);
    return error;
}

rtError_t SomaApi::StreamMemPoolSetAttr(rtMemPool_t memPool, rtMemPoolAttr attr, void *value)
{
    if (!PoolRegistry::Instance().QueryMemPool(RtPtrToPtr<SegmentManager *>(memPool))) {
        RT_LOG(RT_LOG_ERROR, "Memory pool is not created.");
        return RT_ERROR_INVALID_VALUE;
    }
    return RtPtrToPtr<SegmentManager *>(memPool)->SetAttribute(attr, value);
}

rtError_t SomaApi::StreamMemPoolGetAttr(rtMemPool_t memPool, rtMemPoolAttr attr, void *value)
{
    if (!PoolRegistry::Instance().QueryMemPool(RtPtrToPtr<SegmentManager *>(memPool))) {
        RT_LOG(RT_LOG_ERROR, "Memory pool is not created.");
        return RT_ERROR_INVALID_VALUE;
    }
    return RtPtrToPtr<SegmentManager *>(memPool)->GetAttribute(attr, value);
}

rtError_t SomaApi::CreateMemPool(rtMemPoolProps &poolProps, size_t totalSize, SegmentManager *&retMemPool)
{
    if (poolProps.maxSize == 0) {
        RT_LOG(RT_LOG_DEBUG, "Create stream memory pool by default maxSize.");
        // Align the memory pool size to DEVICE_POOL_ALIGN_SIZE with down-rounding
        poolProps.maxSize = totalSize & ~(DEVICE_POOL_ALIGN_SIZE - 1);
    } else {
        // Align the memory pool size to DEVICE_POOL_ALIGN_SIZE with up-rounding
        poolProps.maxSize = (poolProps.maxSize + DEVICE_POOL_ALIGN_SIZE - 1) & ~(DEVICE_POOL_ALIGN_SIZE - 1);
    }
    // Align the memory pool size to DEVICE_POOL_ALIGN_SIZE
    poolProps.maxSize = (poolProps.maxSize + DEVICE_POOL_ALIGN_SIZE - 1) & ~(DEVICE_POOL_ALIGN_SIZE - 1);

    COND_RETURN_ERROR(poolProps.maxSize > totalSize, RT_ERROR_MEMORY_ALLOCATION,
        "Memory pool size (%zu bytes) exceeds device total memory (%zu bytes).", poolProps.maxSize, totalSize);

    return PoolRegistry::Instance().CreateMemPool(poolProps.maxSize, poolProps.devId, true, retMemPool);
}

rtError_t SomaApi::CheckMemPool(SegmentManager *memPool)
{
    return PoolRegistry::Instance().CheckRemoveMemPool(memPool);
}

rtError_t SomaApi::DestroyMemPool(SegmentManager *memPool)
{
    return PoolRegistry::Instance().RemoveMemPool(memPool);
}

rtError_t SomaApi::AllocFromMemPool(void **ptr, uint64_t size, rtMemPool_t memPool, int32_t streamId, ReuseFlag &flag)
{
    COND_RETURN_ERROR(ptr == nullptr, RT_ERROR_MEM_POOL_NULL,
        "Output pointer cannot be null for memory allocation.");
    COND_RETURN_ERROR(memPool == nullptr, RT_ERROR_MEM_POOL_NULL,
        "Memory pool handle is invalid.");
    COND_RETURN_ERROR(streamId == INVALID_STREAM_ID, RT_ERROR_MEM_POOL_NULL,
        "Stream ID is invalid for memory allocation.");
 
    // Align the allocation size to DEVICE_POOL_MIN_BLOCK_SIZE
    size = (size + DEVICE_POOL_MIN_BLOCK_SIZE - 1) & ~(DEVICE_POOL_MIN_BLOCK_SIZE - 1);
 
    SegmentManager *curMemPool = RtPtrToPtr<SegmentManager *>(memPool);
    Segment *seg = nullptr;
    rtError_t error = curMemPool->SegmentAlloc(seg, size, streamId, flag);
    COND_RETURN_ERROR((error != RT_ERROR_NONE || seg == nullptr), RT_ERROR_MEM_POOL_ALLOC,
        "Failed to allocate segment from memory pool, size=%" PRIx64 ", memPoolId=%" PRIx64 ", streamId=%d.",
        size, curMemPool->MemPoolId(), streamId);
 
    *ptr = RtValueToPtr<void *>(seg->basePtr);
    return RT_ERROR_NONE;
}
 
rtError_t SomaApi::FreeToMemPool(void *ptr, bool forceFree)
{
    COND_RETURN_ERROR(ptr == nullptr, RT_ERROR_MEM_POOL_NULL, "Unable to free null pointer.");
    uint64_t p = RtPtrToValue(ptr);
    SegmentManager *curMemPool = PoolRegistry::Instance().FindMemPoolByPtr(p);
    COND_RETURN_ERROR(curMemPool == nullptr, RT_ERROR_POOL_PTR_NOTFOUND,
        "Unable to locate which memory pool the pointer is in, ptr=%" PRIx64 ".", p);
 
    rtError_t error = curMemPool->SegmentFree(p, forceFree);
    ERROR_RETURN(error, "Unable to free ptr=%" PRIx64 ".", p);
 
    return RT_ERROR_NONE;
}
 
rtError_t SomaApi::MemPoolTrimTo(rtMemPool_t memPool, uint64_t minBytesToKeep)
{
    COND_RETURN_ERROR(memPool == nullptr, RT_ERROR_INVALID_VALUE, "MemPoolTrimTo invalid memPool: nullptr.");

    SegmentManager *curMemPool = RtPtrToPtr<SegmentManager *>(memPool);
    uint64_t poolBusySize = 0;
    uint64_t poolReservedSize = 0;
    uint32_t devId = curMemPool->DeviceId();
    uint64_t poolId = curMemPool->MemPoolId();
    uint64_t size = minBytesToKeep;

    RT_LOG(RT_LOG_INFO, "Start getting attribute of mempool.");
    rtError_t error = curMemPool->GetAttribute(rtMemPoolAttrUsedMemCurrent, &poolBusySize);
    COND_RETURN_ERROR(error != RT_ERROR_NONE, error, "Get used mem failed, ret=%d.", error);
    error = curMemPool->GetAttribute(rtMemPoolAttrReservedMemCurrent, &poolReservedSize);
    COND_RETURN_ERROR(error != RT_ERROR_NONE, error, "Get reserved mem failed, ret=%d.", error);
    COND_RETURN_ERROR(poolReservedSize < poolBusySize, RT_ERROR_INVALID_VALUE, "Invalid mempool state, poolReservedSize %lu < poolBusySize %lu.", poolReservedSize, poolBusySize);

    uint64_t poolFreeSize = poolReservedSize - poolBusySize;
    RT_LOG(RT_LOG_DEBUG, "Call halMemPoolTrim, size=%lu, poolBusySize=%lu, poolFreeSize=%lu.", size, poolBusySize, poolFreeSize);
    
    error = Runtime::Instance()->CurrentContext()->Device_()->Driver_()->StreamMemPoolTrim(devId, poolId, &size, poolBusySize, poolFreeSize);
    COND_RETURN_ERROR(error != RT_ERROR_NONE, error, "halMemPoolTrim failed, ret=%d.", error);

    if (size != minBytesToKeep) {
        RT_LOG(RT_LOG_DEBUG, "halMemPoolTrim target not reached (expect=%lu, actual=%lu).", minBytesToKeep, size);
    }
    error = curMemPool->TrimTo(size);
    COND_RETURN_ERROR(error != RT_ERROR_NONE, error, "TrimTo failed, ret=%d.", error);

    return RT_ERROR_NONE;
}

rtError_t SomaApi::MemPoolTrimImplicit(bool includeGraphPool)
{
    rtError_t overallError = RT_ERROR_NONE;
    rtError_t error = RT_ERROR_NONE;
    uint64_t releaseThreshold = 0;
    auto memPools = PoolRegistry::Instance().EnumerateMemPools(includeGraphPool);
    if (memPools.empty()) {
        RT_LOG(RT_LOG_INFO, "No memory pools to trim.");
        return RT_ERROR_NONE;
    }

    for (auto& memPool : memPools) {
        if (memPool == nullptr) {
            RT_LOG(RT_LOG_WARNING, "Skip null memory pool in implicit trim.");
            overallError = RT_ERROR_INVALID_VALUE;
            continue;
        }

        SegmentManager* curMemPool = RtPtrToPtr<SegmentManager*>(memPool);
        uint32_t devId = curMemPool->DeviceId();
        uint64_t poolId = curMemPool->MemPoolId();

        if (!includeGraphPool) {
            error = curMemPool->GetAttribute(rtMemPoolAttrReleaseThreshold, &releaseThreshold);
            if (error != RT_ERROR_NONE) {
                RT_LOG(RT_LOG_WARNING, "Pool[%u:%lu] get release threshold failed, ret=%d.",
                       devId, poolId, error);
                overallError = error;
                continue;
            }
        }

        error = MemPoolTrimTo(memPool, releaseThreshold);
        if (error != RT_ERROR_NONE) {
            RT_LOG(RT_LOG_WARNING, "Pool[%u:%lu] implicit trim failed, ret=%d.",
                   devId, poolId, error);
            if (overallError == RT_ERROR_NONE) {
                overallError = error;
            }
            continue;
        }
    }

    if (overallError == RT_ERROR_NONE) {
        RT_LOG(RT_LOG_INFO, "All memory pools implicit trim completed successfully.");
    } else {
        RT_LOG(RT_LOG_ERROR, "Implicit trim completed with errors.");
    }

    return overallError;
}

bool SomaApi::InMemPoolRegion(void * const ptr)
{
    uint64_t p = RtPtrToValue(ptr);
    return PoolRegistry::Instance().InMemPoolRegion(p);
}

SegmentManager* SomaApi::FindMemPoolByPtr(void * const ptr)
{
    uint64_t p = RtPtrToValue(ptr);
    return PoolRegistry::Instance().FindMemPoolByPtr(p);
}

} // namespace runtime
} // namespace cce