/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "task_fail_callback_manager.hpp"
#include "task_info.hpp"
#include "runtime.hpp"
#include "kernel/kernel.hpp"
#include "drv/driver.hpp"

namespace cce {
namespace runtime {

namespace {
constexpr uint32_t CRC_CHECK_RATIO_LARGE = 25U;
constexpr uint32_t CRC_CHECK_RATIO_MEDIUM = 50U;
constexpr uint32_t MAX_CHECK_SIZE = 1024U * 1024U;
constexpr uint32_t FULL_CHECK_THRESHOLD = 256U * 1024U;
constexpr uint32_t MEDIUM_CHECK_THRESHOLD = 1024U * 1024U;

static uint32_t CalculateCrc32(const void *data, uint32_t size)
{
    if ((data == nullptr) || (size == 0U)) {
        return 0U;
    }

    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    uint32_t crc = 0xFFFFFFFFU;

    for (uint32_t i = 0U; i < size; i++) {
        crc ^= bytes[i];
        for (uint32_t j = 0U; j < 8U; j++) {
            crc = (crc >> 1) ^ ((crc & 0x01U) ? 0xEDB88320U : 0U);
        }
    }

    return ~crc;
}

static void *AllocateTempBuffer(const Device *device, uint32_t memoryCheckSize)
{
    Driver *driver = device->Driver_();
    void *hostTempBuffer = nullptr;
    rtError_t error = driver->HostMemAlloc(&hostTempBuffer, static_cast<uint64_t>(memoryCheckSize), device->Id_());
    if ((error != RT_ERROR_NONE) || (hostTempBuffer == nullptr)) {
        RT_LOG(RT_LOG_ERROR, "Failed to allocate host memory for memory check, size=%u, error=%u", memoryCheckSize, error);
        return nullptr;
    }

    return hostTempBuffer;
}

static void FreeTempBuffer(const Device *device, void *hostTempBuffer)
{
    if ((hostTempBuffer == nullptr) || (device == nullptr)) {
        return;
    }
    Driver *driver = device->Driver_();
    (void)driver->HostMemFree(hostTempBuffer);
}

static uint32_t CalculateCheckSize(uint64_t binarySize)
{
    if (binarySize <= FULL_CHECK_THRESHOLD) {
        return static_cast<uint32_t>(binarySize);
    }

    if (binarySize <= MEDIUM_CHECK_THRESHOLD) {
        return static_cast<uint32_t>((binarySize / 100U) * CRC_CHECK_RATIO_MEDIUM);
    }

    uint32_t memoryCheckSize = static_cast<uint32_t>((binarySize / 100U) * CRC_CHECK_RATIO_LARGE);
    if (memoryCheckSize > MAX_CHECK_SIZE) {
        memoryCheckSize = MAX_CHECK_SIZE;
    }

    return memoryCheckSize;
}

static uint32_t CalculateDeviceCrc(const Device *device, void *deviceAddr, void *hostTempBuffer, uint32_t memoryCheckSize)
{
    Driver *driver = device->Driver_();
    rtError_t error = driver->MemCopySync(
        hostTempBuffer, static_cast<uint64_t>(memoryCheckSize), deviceAddr, static_cast<uint64_t>(memoryCheckSize),
        RT_MEMCPY_DEVICE_TO_HOST);
    if (error != RT_ERROR_NONE) {
        RT_LOG(RT_LOG_ERROR, "Failed to copy device memory to host, error=%u", error);
        return 0U;
    }

    return CalculateCrc32(hostTempBuffer, memoryCheckSize);
}

static uint32_t CalcDeviceCrc32(const Device *device, Program *program, uint32_t deviceId, uint32_t offset, 
    uint32_t memoryCheckSize)
{
    void *deviceBaseAddr = program->GetBinBaseAddr(deviceId);
    if (deviceBaseAddr == nullptr) {
        return 0U;
    }

    void *hostTempBuffer = AllocateTempBuffer(device, memoryCheckSize);
    if (hostTempBuffer == nullptr) {
        return 0U;
    }

    void *deviceKernelAddr = static_cast<uint8_t *>(deviceBaseAddr) + offset;
    uint32_t deviceCrcValue = CalculateDeviceCrc(device, deviceKernelAddr, hostTempBuffer, memoryCheckSize);
    
    FreeTempBuffer(device, hostTempBuffer);
    return deviceCrcValue;
}

static void LogMemoryLayoutInfo(Program *program, uint32_t deviceId, uint32_t offset, 
    uint32_t length, uint32_t memoryCheckSize)
{
    RT_LOG(RT_LOG_DEBUG, "[CRC Debug] ========== Memory Layout Analysis ==========");
    RT_LOG(RT_LOG_DEBUG, "[CRC Debug] Program Info: program=%p, programId=%u, binary_=%p, textData=%p, binarySize=%u",
        program, program->Id_(), program->GetBinary(), program->Data(), program->GetBinarySize());
    RT_LOG(RT_LOG_DEBUG, "[CRC Debug] Device Memory: deviceId=%u, deviceBaseAddr=%p",
        deviceId, program->GetBinBaseAddr(deviceId));
    RT_LOG(RT_LOG_DEBUG, "[CRC Debug] All Device BaseAddrs: dev0=%p, dev1=%p, dev2=%p, dev3=%p, dev4=%p, dev5=%p, dev6=%p, dev7=%p",
        program->GetBinBaseAddr(0U), program->GetBinBaseAddr(1U), 
        program->GetBinBaseAddr(2U), program->GetBinBaseAddr(3U),
        program->GetBinBaseAddr(4U), program->GetBinBaseAddr(5U),
        program->GetBinBaseAddr(6U), program->GetBinBaseAddr(7U));
    RT_LOG(RT_LOG_DEBUG, "[CRC Debug] Kernel Segment: offset=%u, length=%u, checkSize=%u",
        offset, length, memoryCheckSize);
}

static void LogAddressMapping(const void *textData, uint32_t offset, 
    const void *deviceBaseAddr, const uint8_t *hostAddr, const void *deviceKernelAddr)
{
    RT_LOG(RT_LOG_DEBUG, "[CRC Debug] Address Mapping:");
    RT_LOG(RT_LOG_DEBUG, "[CRC Debug]   Host:   textData=%p + offset=%u = hostAddr=%p",
        textData, offset, hostAddr);
    RT_LOG(RT_LOG_DEBUG, "[CRC Debug]   Device: baseAddr_=%p + offset=%u = deviceKernelAddr=%p",
        deviceBaseAddr, offset, deviceKernelAddr);
}

static bool CheckMemorySegment(Program *program, const Device *device, 
    uint32_t deviceId, uint32_t offset, uint32_t length)
{
    uint32_t memoryCheckSize = CalculateCheckSize(length);
    
    LogMemoryLayoutInfo(program, deviceId, offset, length, memoryCheckSize);
    
    const void *textData = program->Data();
    void *deviceBaseAddr = program->GetBinBaseAddr(deviceId);
    
    if (textData == nullptr) {
        RT_LOG(RT_LOG_DEBUG, "[CRC Debug] textData is nullptr, skip check");
        return true;
    }
    if (deviceBaseAddr == nullptr) {
        RT_LOG(RT_LOG_DEBUG, "[CRC Debug] deviceBaseAddr is nullptr, skip check");
        return true;
    }
    if (device == nullptr) {
        RT_LOG(RT_LOG_DEBUG, "[CRC Debug] device is nullptr, skip check");
        return true;
    }
    
    uint32_t binarySize = program->GetBinarySize();
    if ((offset + memoryCheckSize) > binarySize) {
        RT_LOG(RT_LOG_DEBUG, "[CRC Debug] offset=%u + checkSize=%u > binarySize=%u, skip check",
            offset, memoryCheckSize, binarySize);
        return true;
    }
    
    const uint8_t *hostAddr = static_cast<const uint8_t *>(textData) + offset;
    void *deviceKernelAddr = static_cast<uint8_t *>(deviceBaseAddr) + offset;
    
    LogAddressMapping(textData, offset, deviceBaseAddr, hostAddr, deviceKernelAddr);
    
    uint32_t hostCrcValue = CalculateCrc32(hostAddr, memoryCheckSize);
    uint32_t deviceCrcValue = CalcDeviceCrc32(device, program, deviceId, offset, memoryCheckSize);
    
    RT_LOG(RT_LOG_DEBUG, "[CRC Debug] CRC Result: hostCrc=0x%x, deviceCrc=0x%x, match=%s",
        hostCrcValue, deviceCrcValue, (hostCrcValue == deviceCrcValue) ? "yes" : "no");
    
    if ((deviceCrcValue != 0U) && (hostCrcValue != deviceCrcValue)) {
        RT_LOG(RT_LOG_ERROR, "Memory corruption! offset=%u, len=%u, hostCrc=0x%x, devCrc=0x%x",
            offset, length, hostCrcValue, deviceCrcValue);
        return false;
    }
    return true;
}

static void CheckSingleKernelMemoryCorruption(Program *program, const Kernel *kernel, const Device *device, 
    uint32_t deviceId, const char *kernelName)
{
    if (kernel == nullptr) {
        return;
    }

    uint32_t aicSegmentLength = 0U;
    uint32_t aivSegmentLength = 0U;
    kernel->GetKernelLength(aicSegmentLength, aivSegmentLength);

    RT_LOG(RT_LOG_INFO, 
        "CheckSingleKernelMemoryCorruption: kernel_name=%s, offset1=%u, offset2=%u, "
        "aicLen=%u, aivLen=%u, program=%p, deviceId=%u",
        kernelName, kernel->Offset_(), kernel->Offset2_(), 
        aicSegmentLength, aivSegmentLength, program, deviceId);

    if ((aicSegmentLength == 0U) && (aivSegmentLength == 0U)) {
        RT_LOG(RT_LOG_INFO, "Skip check for kernel_name=%s", kernelName);
        return;
    }

    if (aicSegmentLength > 0U) {
        bool aicResult = CheckMemorySegment(program, device, deviceId, kernel->Offset_(), aicSegmentLength);
        if (!aicResult) {
            RT_LOG(RT_LOG_ERROR, 
                "Kernel AIC segment memory corruption! kernel_name=%s, offset=%u, len=%u, program=%p, deviceId=%u",
                kernelName, kernel->Offset_(), aicSegmentLength, static_cast<const void *>(program), deviceId);
        }
    }

    if (aivSegmentLength > 0U) {
        bool aivResult = CheckMemorySegment(program, device, deviceId, kernel->Offset2_(), aivSegmentLength);
        if (!aivResult) {
            RT_LOG(RT_LOG_ERROR, 
                "Kernel AIV segment memory corruption! kernel_name=%s, offset=%u, len=%u, program=%p, deviceId=%u",
                kernelName, kernel->Offset2_(), aivSegmentLength, static_cast<const void *>(program), deviceId);
        }
    }
}
}  // namespace

static const Kernel *LookupKernelByName(Program *program, const char *kernelName)
{
    const Kernel *kernel = program->GetKernelByName(kernelName);
    if (kernel != nullptr) {
        return kernel;
    }

    for (uint32_t i = 0U; i < program->kernelPos_; i++) {
        if ((program->KernelTable_[i].kernel != nullptr) &&
            (program->KernelTable_[i].kernel->Name_() == kernelName)) {
            RT_LOG(RT_LOG_INFO, 
                "Kernel found in Program::KernelTable_ by name=%s (AllKernelRegister API)", kernelName);
            return program->KernelTable_[i].kernel;
        }
    }

    Runtime *runtime = Runtime::Instance();
    if (runtime == nullptr) {
        return nullptr;
    }
    const void *stubFunc = runtime->StubFuncLookup(kernelName);
    if (stubFunc != nullptr) {
        kernel = runtime->KernelLookup(stubFunc);
        if (kernel != nullptr) {
            RT_LOG(RT_LOG_INFO, 
                "Kernel found in global kernelTable by name=%s (legacy rtFunctionRegister API)", kernelName);
            return kernel;
        }
    }

    RT_LOG(RT_LOG_INFO, "Kernel not found by name=%s in all kernel tables", kernelName);
    return nullptr;
}

void CheckKernelMemoryCorruption(Program *program, const Device *device, uint32_t deviceId, 
    rtExceptionArgsInfo_t *kernelInfo)
{
    if (program == nullptr) {
        return;
    }

    const char *kernelName = "unknown";
    const Kernel *kernel = nullptr;

    if ((kernelInfo != nullptr) && (kernelInfo->exceptionKernelInfo.kernelName != nullptr)) {
        kernelName = kernelInfo->exceptionKernelInfo.kernelName;
        kernel = LookupKernelByName(program, kernelName);
    }

    if (kernel != nullptr) {
        CheckSingleKernelMemoryCorruption(program, kernel, device, deviceId, kernelName);
    } else {
        RT_LOG(RT_LOG_INFO, "Kernel not found by name=%s, skip single kernel memory check", kernelName);
    }
}

}  // namespace runtime
}  // namespace cce
