/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "dump_manager.h"
#include <thread>
#include <cctype>
#include <cinttypes>
#include <map>
#include <algorithm>
#include <cerrno>
#include <sstream>
#include "str_utils.h"
#include "lib_path.h"
#include "file_utils.h"
#include "log/adx_log.h"
#include "runtime/context.h"
#include "adump_dsmi.h"
#include "common_utils.h"
#include "exception_info_common.h"
#include "proto/dump_task.pb.h"

#include "dump_config_converter.h"
#include "adump_api.h"
#include "operator_dumper.h"
#include "kernel_dfx_dumper.h"
#include "adx_dump_record.h"
#include "common/file.h"
#include "memory_utils.h"
#include "dump_memory.h"
#include "sys_utils.h"

namespace Adx {
constexpr char EXCEPTION_CB_MODULE[] = "AdumpException";
constexpr char COREDUMP_CB_MODULE[] = "AdumpCoredump";
static const uint32_t ADUMP_ACL_ERROR_RT_AICORE_OVER_FLOW = 207003; // aicore over flow
static const uint32_t ADUMP_ACL_ERROR_RT_AIVEC_OVER_FLOW = 207016;  // aivec over flow
constexpr uint32_t DUMP_SLICE_SIZE = 128 * 1024 * 1024;             // 128MB

std::vector<std::shared_ptr<OperatorPreliminary>> DumpManager::operatorMap_;

static void ExceptionCallback(rtExceptionInfo* const exception)
{
    IDE_RUN_LOGI("An exception callback message is received.");
    if (exception != nullptr) {
        if (exception->retcode == ADUMP_ACL_ERROR_RT_AICORE_OVER_FLOW ||
            exception->retcode == ADUMP_ACL_ERROR_RT_AIVEC_OVER_FLOW) {
            IDE_LOGW("Ignore exception dump request, retcode: %u.", exception->retcode);
            return;
        }
        rtExceptionArgsInfo_t exceptionArgsInfo{};
        rtExceptionExpandType_t exceptionTaskType = exception->expandInfo.type;
        if (ExceptionInfoCommon::GetExceptionInfo(*exception, exceptionTaskType, exceptionArgsInfo) != ADUMP_SUCCESS) {
            IDE_LOGW("Get exception args info failed.");
            return;
        }

        (void)DumpManager::Instance().DumpExceptionInfo(*exception);
        AdxLogFlush();
    }
}

static void NotifyCoredumpCallback(uint32_t devId, bool isOpen)
{
    static std::map<uint32_t, uint32_t> setDeviceRecord;
    static std::mutex coredumpMtx;
    std::lock_guard<std::mutex> lk(coredumpMtx);
    auto it = setDeviceRecord.find(devId);
    if (!isOpen) {
        if (it != setDeviceRecord.end()) {
            it->second--;
            if (it->second == 0) {
                setDeviceRecord.erase(it);
                IDE_LOGI("Device %u has been removed from the effective list.", devId);
            }
        }
        return;
    }
    if (it != setDeviceRecord.end()) {
        it->second++;
        return;
    }
    rtError_t ret = rtDebugSetDumpMode(RT_DEBUG_DUMP_ON_EXCEPTION);
    if (ret != RT_ERROR_NONE) {
        IDE_RUN_LOGI("detail exception dump mode not support, switch to lite exception dump, ret:%d", ret);
        DumpManager::Instance().ExceptionModeDowngrade();
    }
    IDE_LOGI("Device %u has been added to the effective list.", devId);
    setDeviceRecord[devId] = 1;
}

DumpManager& DumpManager::Instance()
{
    static DumpManager instance;
    return instance;
}

DumpManager::DumpManager()
{
    // enable dump functions with environment variables
    DumpConfig config;
    DumpType dumpType;
    // 1. enable exception dump with environment variables
    if (DumpConfigConverter::EnableExceptionDumpWithEnv(config, dumpType)) {
        if (SetDumpConfig(dumpType, config) != ADUMP_SUCCESS) {
            IDE_LOGW("Enable exception dump failed. dumpType: %d", dumpType);
        } else {
            isEnvExceptionDump_ = true;
        }
    }
    // 2. 通过环境变量使能Kernel Dfx Dump
    KernelDfxDumper::Instance();
}

void DumpManager::KFCResourceInit()
{
#if !defined(ADUMP_SOC_HOST) || ADUMP_SOC_HOST == 1
    if (isKFCInit_) {
        IDE_LOGD("KFC resources have been initialized on all devices.");
        return;
    }
    std::vector<uint32_t> devList = AdumpDsmi::DrvGetDeviceList();

    for (uint32_t& deviceId : devList) {
        IDE_LOGI("Start to initialize KFC resources on device %u.", deviceId);
        SharedPtr<OperatorPreliminary> opIniter = MakeSharedInstance<OperatorPreliminary>(GetDumpSetting(), deviceId);
        IDE_CTRL_VALUE_FAILED_NODO(
            opIniter != nullptr && opIniter->OperatorInit() == ADUMP_SUCCESS, return,
            "Failed to execute the resource initialization task on device %u.", deviceId);
        DumpManager::operatorMap_.emplace_back(std::move(opIniter));
        IDE_LOGI("KFC executed on the device %u successfully.", deviceId);
    }
#endif
    isKFCInit_ = true;
}

int32_t DumpManager::ExceptionConfig(DumpType dumpType, const DumpConfig& dumpConfig)
{
    if (exceptionDumper_.IsRepeatEnableException(dumpType, dumpConfig)) {
        IDE_LOGW(
            "Exception dump has been enabled, not support enable exception dump[%s] again",
            DumpConfigConverter::DumpTypeToStr(dumpType).c_str());
        return ADUMP_SUCCESS;
    }

    if (dumpType == DumpType::AIC_ERR_DETAIL_DUMP && !CheckCoredumpSupportedPlatform()) {
        IDE_LOGE("Current platform is not support coredump mode.");
        return ADUMP_FAILED;
    }

    IDE_RUN_LOGI(
        "Set %d dump setting, status: %s, dump switch: %llu", dumpType, dumpConfig.dumpStatus.c_str(),
        dumpConfig.dumpSwitch);
    if (exceptionDumper_.ExceptionDumperInit(dumpType, dumpConfig) != ADUMP_SUCCESS) {
        IDE_LOGW("Failed to initialize the exception dump.");
        return ADUMP_SUCCESS;
    }
    dumpSetting_.InitDumpSwitch(dumpConfig.dumpSwitch & DUMP_SWITCH_MASK);
    IDE_CTRL_VALUE_WARN(
        RegsiterExceptionCallback(), return ADUMP_SUCCESS,
        "Failed to register the args exception dump callback function.");
    if (exceptionDumper_.GetCoredumpStatus()) { // 如果启动了coredump模式
        IDE_CTRL_VALUE_FAILED(
            rtRegDeviceStateCallbackEx(COREDUMP_CB_MODULE, &NotifyCoredumpCallback, DEV_CB_POS_BACK) == RT_ERROR_NONE,
            return ADUMP_FAILED, "Failed to register the coredump callback function to rtSetDevice.");
    }
    return ADUMP_SUCCESS;
}

int32_t DumpManager::SetDumpConfig(DumpType dumpType, const DumpConfig& dumpConfig)
{
    std::lock_guard<std::mutex> lk(resourceMtx_);
    if (dumpType == DumpType::EXCEPTION || dumpType == DumpType::ARGS_EXCEPTION ||
        dumpType == DumpType::AIC_ERR_DETAIL_DUMP) {
        return ExceptionConfig(dumpType, dumpConfig);
    }
    auto ret = dumpSetting_.Init(dumpType, dumpConfig);
    if (ret != ADUMP_SUCCESS) {
        return ret;
    }

    if (CheckBinValidation() && (isKFCInit_ == false)) {
        auto kfcBind = std::bind(&DumpManager::KFCResourceInit, this);
        std::thread kfcThread(kfcBind);
        kfcThread.join();
        if (!isKFCInit_) {
            IDE_LOGE("SetDumpConfig failed due to fkc resource initialization error.");
            DumpManager::operatorMap_.clear();
            return ADUMP_FAILED;
        }
    }
    IDE_RUN_LOGI(
        "Set %d dump setting, status: %s, mode: %s, data: %s, dump switch: %llu, path:%s, dump stats:%s.", dumpType,
        dumpConfig.dumpStatus.c_str(), dumpConfig.dumpMode.c_str(), dumpConfig.dumpData.c_str(), dumpConfig.dumpSwitch,
        dumpConfig.dumpPath.c_str(), StrUtils::ToString(dumpConfig.dumpStatsItem).c_str());
    return ADUMP_SUCCESS;
}

int32_t DumpManager::SetDumpConfig(const char* dumpConfigData, size_t dumpConfigSize)
{
    std::lock_guard<std::mutex> lk(resourceMtx2_);
    if ((dumpConfigData == nullptr) || (dumpConfigSize == 0U)) {
        IDE_LOGE("Set dump config failed. Config data is null or empty.");
        return ADUMP_FAILED;
    }
    DumpConfig dumpConfig;
    DumpDfxConfig dumpDfxConfig;
    DumpType dumpType;
    bool needDump = true;
    DumpConfigConverter converter{dumpConfigData, dumpConfigSize};
    int32_t ret = converter.Convert(dumpType, dumpConfig, needDump, dumpDfxConfig);
    if (ret != ADUMP_SUCCESS) {
        IDE_LOGE("Parse dump config from memory[%s] failed.", dumpConfigData);
        return ADUMP_INPUT_FAILED;
    }

    // 开启KernelDataDump
    ret = KernelDfxDumper::Instance().EnableDfxDumper(dumpDfxConfig);
    IDE_CTRL_VALUE_FAILED(ret == ADUMP_SUCCESS, return ret, "Enable kernel dfx dump failed.");

    if (!needDump) {
        return ADUMP_SUCCESS;
    }

    // 已开启exception dump：不支持重复使能，不更新配置缓存，不重复回调注册模块组件
    if (exceptionDumper_.IsRepeatEnableException(dumpType, dumpConfig)) {
        IDE_LOGW(
            "Exception dump has been enabled, not support enable exception dump[%s] again",
            DumpConfigConverter::DumpTypeToStr(dumpType).c_str());
        return ADUMP_SUCCESS;
    }

    (void)dumpConfigInfo_.assign(dumpConfigData, dumpConfigSize);
    IDE_LOGI("Dump config info set: addr=%p, size=%zu", dumpConfigInfo_.data(), dumpConfigInfo_.size());
    ret = SetDumpConfig(dumpType, dumpConfig);
    // 同步触发callback事件
    for (auto& item : enableCallbackFunc_) {
        IDE_LOGI("SetDumpConfig HandleDumpEvent start for module [%zu]", item.first);
        HandleDumpEvent(item.first, DumpEnableAction::ENABLE);
    }
    IDE_CTRL_VALUE_FAILED(ret == ADUMP_SUCCESS, return ret, "Set dump config failed.");
    (void)openedDump_.insert(dumpType);
    return ADUMP_SUCCESS;
}

int32_t DumpManager::UnSetDumpConfig()
{
    std::lock_guard<std::mutex> lk(resourceMtx2_);
    DumpConfig config;
    config.dumpStatus = ADUMP_DUMP_STATUS_SWITCH_OFF;
    config.dumpSwitch = 0;
    for (const auto dumpType : openedDump_) {
        if (IsEnableDump(dumpType)) {
            const auto ret = SetDumpConfig(dumpType, config);
            if (ret != ADUMP_SUCCESS) {
                IDE_LOGE(
                    "[Set][Dump]set dump off failed, dumpType:[%d], errorCode = %d", static_cast<int32_t>(dumpType),
                    ret);
                return ADUMP_FAILED;
            }
            IDE_LOGI("set dump off successfully, dumpType:[%d].", static_cast<int32_t>(dumpType));
        }
    }
    openedDump_.clear();
    // 同步触发callbcak事件
    for (auto& item : disableCallbackFunc_) {
        IDE_LOGI("UnSetDumpConfig start for module [%zu]", item.first);
        HandleDumpEvent(item.first, DumpEnableAction::DISABLE);
    }
    dumpConfigInfo_.clear();
    IDE_LOGI("Dump config info cleared.");

    // 等待所有 dump 操作完成并清理资源
    DumpResourceSafeMap::Instance().waitAndClear();

    return ADUMP_SUCCESS;
}

std::string DumpManager::GetBinName() const
{
    auto it = BIN_NAME_MAP.find(dumpSetting_.GetPlatformType());
    if (it != BIN_NAME_MAP.cend()) {
        return it->second;
    }
    return "";
}

bool DumpManager::CheckBinValidation()
{
    const std::string opName = GetBinName();
    if ((dumpSetting_.GetDumpData().compare(DUMP_STATS_DATA) != 0) || opName.empty()) {
        return false;
    }
    const std::string opPath = LibPath::Instance().GetTargetPath(opName);
    IDE_CTRL_VALUE_FAILED(!opPath.empty(), return false, "Received an empty path for file %s.", opName.c_str());
    bool isExist = FileUtils::IsFileExist(opPath);
    IDE_LOGI("CheckBinValidation result is %s", isExist ? "true" : "false");
    return isExist;
}

bool DumpManager::IsEnableDump(DumpType dumpType)
{
    std::lock_guard<std::mutex> lk(resourceMtx_);
    if (dumpType == DumpType::ARGS_EXCEPTION) {
        return exceptionDumper_.GetArgsExceptionStatus() || exceptionDumper_.GetCoredumpStatus();
    } else if (dumpType == DumpType::OPERATOR) {
        return dumpSetting_.GetDumpStatus();
    } else if (dumpType == DumpType::OP_OVERFLOW) {
        return dumpSetting_.GetDumpDebugStatus();
    } else if (dumpType == DumpType::EXCEPTION) {
        return exceptionDumper_.GetExceptionStatus();
    } else if (dumpType == DumpType::AIC_ERR_DETAIL_DUMP) {
        return exceptionDumper_.GetCoredumpStatus();
    } else {
        IDE_LOGW("Dump type is not support.");
    }

    return false;
}

int32_t DumpManager::DumpOperator(
    const std::string& opType, const std::string& opName, const std::vector<TensorInfo>& tensors, aclrtStream stream)
{
    return DumpOperatorV2(opType, opName, ConvertTensorInfoToDumpTensorV2(tensors), stream);
}

int32_t DumpManager::DumpOperatorV2(
    const std::string& opType, const std::string& opName, const std::vector<TensorInfoV2>& tensors, aclrtStream stream)
{
    std::lock_guard<std::mutex> lk(resourceMtx_);
    if (!dumpSetting_.GetDumpStatus() && !dumpSetting_.GetDumpDebugStatus()) {
        IDE_LOGW("Operator or overflow dump is not enable, can't dump.");
        return ADUMP_SUCCESS;
    }

    rtStreamCaptureStatus status = RT_STREAM_CAPTURE_STATUS_MAX;
    rtModel_t* captureMdl = nullptr;
    int32_t ret = rtStreamGetCaptureInfo(stream, &status, captureMdl);
    if (ret != ACL_SUCCESS) {
        IDE_LOGE("Get stream capture info error: %d", ret);
        return ret;
    }
    IDE_LOGI("Get stream capture info: %d", status);

    std::vector<DumpTensor> inputTensors;
    std::vector<DumpTensor> outputTensors;
    for (const auto& tensorInfo : tensors) {
        if (tensorInfo.tensorAddr == nullptr || tensorInfo.tensorSize == 0) {
            IDE_LOGE("Tensor is nullptr.");
            return ADUMP_FAILED;
        }

        if (tensorInfo.placement != TensorPlacement::kOnDeviceHbm) {
            IDE_LOGW("Tensor of of %s[%s] is on device, skip it.", opName.c_str(), opType.c_str());
            continue;
        }

        if (tensorInfo.type == TensorType::INPUT) {
            inputTensors.emplace_back(tensorInfo);
        } else if (tensorInfo.type == TensorType::OUTPUT) {
            outputTensors.emplace_back(tensorInfo);
        }
    }

    OperatorDumper opDumper(opType, opName);
    ret = opDumper.SetDumpSetting(dumpSetting_)
              .RuntimeStream(stream)
              .InputDumpTensor(inputTensors)
              .OutputDumpTensor(outputTensors)
              .Launch();
    if (ret != ADUMP_SUCCESS) {
        IDE_LOGE("Launch dump operator failed.");
        return ret;
    }
    return ADUMP_SUCCESS;
}

static int32_t DumpTensorPushToDumpQueue(
    void* dataBuf, uint32_t bufLen, const char* fileName, uint64_t offset, uint32_t isLastChunk)
{
    int err;
    uint32_t dataLen = 0;
    IDE_RETURN_IF_CHECK_ASSIGN_32U_ADD(sizeof(DumpChunk), bufLen, dataLen, return IDE_DAEMON_INTERGER_REVERSED_ERROR);
    MsgProto* msg = AdxMsgProto::CreateMsgPacket(IDE_DUMP_REQ, 0, nullptr, dataLen);
    IDE_CTRL_VALUE_FAILED(msg != nullptr, return IDE_DAEMON_MALLOC_ERROR, "create message failed");
    SharedPtr<MsgProto> sendDataMsgPtr(msg, IdeXfree);
    msg = nullptr;
    DumpChunk* data = reinterpret_cast<DumpChunk*>(sendDataMsgPtr->data);
    err = strcpy_s(data->fileName, IDE_MAX_FILE_PATH, fileName);
    IDE_CTRL_VALUE_FAILED(err == EOK, return IDE_DAEMON_INVALID_PATH_ERROR, "copy file name failed");
    data->bufLen = bufLen;
    data->flag = 0;
    data->isLastChunk = isLastChunk;
    data->offset = static_cast<int64_t>(offset);
    IDE_LOGI(
        "dataLen: %u, bufLen: %u, flag: %d, isLastChunk: %u, offset: %ld, fileName: %s", dataLen, data->bufLen,
        data->flag, data->isLastChunk, data->offset, data->fileName);
    err = memcpy_s(data->dataBuf, data->bufLen, dataBuf, bufLen);
    IDE_CTRL_VALUE_FAILED(err == EOK, return IDE_DAEMON_UNKNOW_ERROR, "memcpy_s data buffer failed");
    HostDumpDataInfo dataInfo = {sendDataMsgPtr, dataLen};
    if (!AdxDumpRecord::Instance().RecordDumpDataToQueue(dataInfo)) {
        IDE_LOGW("dump data queue full");
        return ADUMP_FAILED;
    }
    IDE_LOGD("dump data process normal");
    return ADUMP_SUCCESS;
}

static int32_t SerializeAndPushToQueue(
    const std::vector<char>& dumpDataBuffer, size_t totalSize, const std::string& fileName)
{
    if (totalSize > DUMP_SLICE_SIZE) {
        const char* data = dumpDataBuffer.data();
        uint64_t offset = 0;
        while (offset < totalSize) {
            uint32_t chunkSize = static_cast<uint32_t>(
                std::min(static_cast<size_t>(DUMP_SLICE_SIZE), static_cast<size_t>(totalSize - offset)));
            uint32_t isLastChunk = ((offset + chunkSize) >= totalSize) ? 1 : 0;

            int ret =
                DumpTensorPushToDumpQueue((void*)(data + offset), chunkSize, fileName.c_str(), offset, isLastChunk);
            if (ret != ADUMP_SUCCESS) {
                IDE_LOGW("%s push to queue failed at offset %lu, ret: %d", fileName.c_str(), offset, ret);
                return ret;
            }
            offset += chunkSize;
        }
        IDE_LOGI("%s dump data success, size: %zu bytes (chunked)", fileName.c_str(), totalSize);
        return ADUMP_SUCCESS;
    }

    int ret = DumpTensorPushToDumpQueue(
        const_cast<void*>(static_cast<const void*>(dumpDataBuffer.data())), static_cast<uint32_t>(totalSize),
        fileName.c_str(), 0, 1);
    if (ret != ADUMP_SUCCESS) {
        IDE_LOGW("%s push to queue failed, ret: %d", fileName.c_str(), ret);
        return ret;
    }
    IDE_LOGI("%s dump data success, size: %zu bytes", fileName.c_str(), totalSize);
    return ADUMP_SUCCESS;
}

static void DumpTensorToQueue(DumpStreamInfo* dumpInfoPtr)
{
    if (dumpInfoPtr == nullptr) {
        IDE_LOGE("dumpInfoPtr is nullptr");
        return;
    }

    std::string fileName = GenerateDumpFileName(dumpInfoPtr);

    toolkit::dump::DumpData dumpData = BuildDumpDataProto(dumpInfoPtr);

    uint64_t protoSize = dumpData.ByteSizeLong();
    if (protoSize == 0) {
        IDE_LOGE("Protobuf serialize failed, proto size is 0");
        return;
    }

    size_t inputDataSize = CalculateTensorDataSize(dumpInfoPtr->inputTensors);
    size_t outputDataSize = CalculateTensorDataSize(dumpInfoPtr->outputTensors);

    size_t totalSize = sizeof(uint64_t) + protoSize + inputDataSize + outputDataSize;
    std::vector<char> dumpDataBuffer(totalSize);

    uint64_t* sizePtr = reinterpret_cast<uint64_t*>(dumpDataBuffer.data());
    *sizePtr = protoSize;

    if (!dumpData.SerializeToArray(dumpDataBuffer.data() + sizeof(uint64_t), static_cast<int32_t>(protoSize))) {
        IDE_LOGE("Serialize dump data failed, protoSize: %lu", protoSize);
        return;
    }

    char* dataPtr = dumpDataBuffer.data() + sizeof(uint64_t) + protoSize;
    size_t remainingSize = inputDataSize + outputDataSize;

    int ret = CopyTensorDataToBuffer(dumpInfoPtr->inputTensors, dataPtr, remainingSize);
    if (ret != ADUMP_SUCCESS) {
        IDE_LOGE("%s(%s)copy input tensor to buffer failed", 
            dumpInfoPtr->opName.c_str(), dumpInfoPtr->opType.c_str());
        return;
    }
    dataPtr += inputDataSize;
    remainingSize -= inputDataSize;
    ret = CopyTensorDataToBuffer(dumpInfoPtr->outputTensors, dataPtr, remainingSize);
    if (ret != ADUMP_SUCCESS) {
        IDE_LOGE("%s(%s)copy input tensor to buffer failed", 
            dumpInfoPtr->opName.c_str(), dumpInfoPtr->opType.c_str());
        return;
    }

    (void)SerializeAndPushToQueue(dumpDataBuffer, totalSize, fileName);
}

static void DumpDataRecordInCaptureStream(void* fnArgs)
{
    if (fnArgs == nullptr) {
        IDE_LOGE("create dump stream failed");
        return;
    }

    DumpStreamInfo* args = static_cast<DumpStreamInfo*>(fnArgs);
    if (args == nullptr) {
        IDE_LOGE("args is nullptr");
        return;
    }

    IDE_LOGI("%s input tensor dump, size : %d", args->opName.c_str(), args->inputTensors.size());
    IDE_LOGI("%s output tensor dump, size : %d", args->opName.c_str(), args->outputTensors.size());
    DumpTensorToQueue(args);

    DumpResourceSafeMap::Instance().EnqueueCleanup(args->mainStreamKey);
}

static int32_t CollectStreamContextInfo(
    aclrtStream mainStream, const std::string& opName, const std::string& opType, uint32_t& streamId, uint32_t& taskId,
    uint32_t& deviceId, std::string& dumpPath)
{
    aclError ret = aclrtStreamGetId(mainStream, reinterpret_cast<int32_t*>(&streamId));
    IDE_CTRL_VALUE_FAILED(
        (ret == ACL_SUCCESS), return ADUMP_FAILED, "%s(%s) dump data : get main stream id failed, ret: %d",
        opName.c_str(), opType.c_str(), ret);

    ret = aclrtGetThreadLastTaskId(&taskId);
    IDE_CTRL_VALUE_FAILED(
        (ret == ACL_SUCCESS), return ADUMP_FAILED, "%s(%s) dump data : get task id failed, ret: %d", opName.c_str(),
        opType.c_str(), ret);

    int32_t deviceIdTmp = 0;
    ret = aclrtGetDevice(&deviceIdTmp);
    IDE_CTRL_VALUE_FAILED(
        (ret == ACL_SUCCESS), return ADUMP_FAILED, "%s(%s) dump data : get device id failed, ret: %d", opName.c_str(),
        opType.c_str(), ret);
    deviceId = static_cast<uint32_t>(deviceIdTmp);

    dumpPath = DumpManager::Instance().GetDumpSetting().GetDumpPath();
    if (dumpPath.empty()) {
        IDE_LOGE("%s(%s) dump data : get dump path failed", opName.c_str(), opType.c_str());
        return ADUMP_FAILED;
    }
    return ADUMP_SUCCESS;
}

static int32_t SetupAsyncDump(
    std::shared_ptr<DumpStreamInfo> dumpInfoPtr, const std::string& opName, const std::string& opType,
    aclrtStream mainStream)
{
    aclError ret = aclrtRecordEvent(dumpInfoPtr->mainStmEvt, mainStream);
    IDE_CTRL_VALUE_FAILED(
        ret == ACL_SUCCESS, return ADUMP_FAILED, "%s(%s) main stream record event failed, ret: %d", opName.c_str(),
        opType.c_str(), ret);

    ret = aclrtStreamWaitEvent(dumpInfoPtr->stm, dumpInfoPtr->mainStmEvt);
    IDE_CTRL_VALUE_FAILED(
        ret == ACL_SUCCESS, return ADUMP_FAILED, "%s(%s) dump stream wait event failed, ret: %d", opName.c_str(),
        opType.c_str(), ret);

    ret = aclrtLaunchHostFunc(dumpInfoPtr->stm, DumpDataRecordInCaptureStream, (void*)dumpInfoPtr.get());
    IDE_CTRL_VALUE_FAILED(
        ret == ACL_SUCCESS, return ADUMP_FAILED, "%s(%s) launch host function failed, ret: %d", opName.c_str(),
        opType.c_str(), ret);

    ret = aclrtRecordEvent(dumpInfoPtr->dumpStmEvt, dumpInfoPtr->stm);
    IDE_CTRL_VALUE_FAILED(
        ret == ACL_SUCCESS, return ADUMP_FAILED, "%s(%s) dump stream record event failed, ret: %d", opName.c_str(),
        opType.c_str(), ret);

    ret = aclrtStreamWaitEvent(mainStream, dumpInfoPtr->dumpStmEvt);
    IDE_CTRL_VALUE_FAILED(
        ret == ACL_SUCCESS, return ADUMP_FAILED, "%s(%s) main stream wait event failed, ret: %d", opName.c_str(),
        opType.c_str(), ret);
    return ADUMP_SUCCESS;
}

int32_t DumpManager::DumpOpertorWithCapture(
    const std::string& opType, const std::string& opName, const std::vector<DumpTensor>& inputTensors,
    const std::vector<DumpTensor>& outputTensors, aclrtStream mainStream)
{
    if (mainStream == nullptr) {
        IDE_LOGE("mainStream is nullptr.");
        return ADUMP_FAILED;
    }

    uint32_t streamId = 0;
    uint32_t taskId = 0;
    uint32_t deviceId = 0;
    std::string dumpPath;
    int32_t ret = CollectStreamContextInfo(mainStream, opName, opType, streamId, taskId, deviceId, dumpPath);
    if (ret != ADUMP_SUCCESS) {
        return ret;
    }

    uint64_t timestamp = SysUtils::GetTimestamp();
    uint64_t dumpNumber = GetNextDumpNumber();

    std::string mainStreamKey = std::to_string(streamId) + "_" + std::to_string(taskId);
    DumpInfoParams params = {mainStreamKey, inputTensors, outputTensors, opType, opName,
                             streamId,      taskId,       deviceId,      0,      0,
                             timestamp,     dumpNumber,   dumpPath};
    ret = GetDumpInfoFromMap(params);
    std::shared_ptr<DumpStreamInfo> dumpInfoPtr = DumpResourceSafeMap::Instance().get(mainStreamKey);
    if (ret != ADUMP_SUCCESS || dumpInfoPtr == nullptr) {
        IDE_LOGE("%s(%s) get dump info failed.", opName.c_str(), opType.c_str());
        return ADUMP_FAILED;
    }
    IDE_LOGI("%s(%s) dump data : create DumpStreamInfo success", opName.c_str(), opType.c_str());

    ret = SetupAsyncDump(dumpInfoPtr, opName, opType, mainStream);
    if (ret != ADUMP_SUCCESS) {
        return ret;
    }
    IDE_LOGI("%s(%s) dump data : set event, callback function success", opName.c_str(), opType.c_str());

    return ADUMP_SUCCESS;
}

int32_t DumpManager::GetDumpInfoFromMap(const DumpInfoParams& params)
{
    auto it = DumpResourceSafeMap::Instance().get(params.mainStreamKey);
    if (it != nullptr) {
        return ADUMP_SUCCESS;
    }

    DumpStreamInfo* dumpPtr = nullptr;
    int32_t ret = DumpStreamCreate(&dumpPtr);
    std::shared_ptr<DumpStreamInfo> dumpInfo(dumpPtr, DumpStreamFree);
    if (ret != ADUMP_SUCCESS) {
        IDE_LOGE("ceate dump info error, ret : %d", ret);
        return ADUMP_FAILED;
    }
    dumpPtr->mainStreamKey = params.mainStreamKey;
    dumpPtr->opType = params.opType;
    dumpPtr->opName = params.opName;
    dumpPtr->streamId = params.streamId;
    dumpPtr->taskId = params.taskId;
    dumpPtr->deviceId = params.deviceId;
    dumpPtr->contextId = params.contextId;
    dumpPtr->threadId = params.threadId;
    dumpPtr->timestamp = params.timestamp;
    dumpPtr->dumpNumber = params.dumpNumber;
    dumpPtr->dumpPath = params.dumpPath;
    for (const auto& tensorInfo : params.inputTensors) {
        dumpPtr->inputTensors.emplace_back(tensorInfo);
    }

    for (const auto& tensorInfo : params.outputTensors) {
        dumpPtr->outputTensors.emplace_back(tensorInfo);
    }
    DumpResourceSafeMap::Instance().insert(params.mainStreamKey, dumpInfo);
    return ADUMP_SUCCESS;
}

void DumpManager::AddExceptionOp(const OperatorInfo& opInfo)
{
    exceptionDumper_.AddDumpOperator(opInfo);
}

void DumpManager::AddExceptionOpV2(const OperatorInfoV2& opInfo)
{
    exceptionDumper_.AddDumpOperatorV2(opInfo);
}

void DumpManager::ConvertOperatorInfo(const OperatorInfo& opInfo, OperatorInfoV2& operatorInfoV2) const
{
    operatorInfoV2.agingFlag = opInfo.agingFlag;
    operatorInfoV2.taskId = opInfo.taskId;
    operatorInfoV2.streamId = opInfo.streamId;
    operatorInfoV2.deviceId = opInfo.deviceId;
    operatorInfoV2.contextId = opInfo.contextId;
    operatorInfoV2.opType = opInfo.opType;
    operatorInfoV2.opName = opInfo.opName;
    operatorInfoV2.tensorInfos = ConvertTensorInfoToDumpTensorV2(opInfo.tensorInfos);
    operatorInfoV2.deviceInfos = opInfo.deviceInfos;
    operatorInfoV2.additionalInfo = opInfo.additionalInfo;
}

std::vector<TensorInfoV2> DumpManager::ConvertTensorInfoToDumpTensorV2(const std::vector<TensorInfo>& tensorInfos) const
{
    std::vector<TensorInfoV2> tensors;
    tensors.reserve(tensorInfos.size());
    for (const auto& tensorInfo : tensorInfos) {
        TensorInfoV2 tensor = {};
        ConvertTensorInfo(tensorInfo, tensor);
        tensors.emplace_back(tensor);
    }
    return tensors;
}

void DumpManager::ConvertTensorInfo(const TensorInfo& tensorInfo, TensorInfoV2& tensor) const
{
    tensor.dataType = tensorInfo.dataType;
    tensor.format = tensorInfo.format;
    tensor.placement = tensorInfo.placement;
    tensor.tensorAddr = tensorInfo.tensorAddr;
    tensor.tensorSize = tensorInfo.tensorSize;
    tensor.type = tensorInfo.type;
    tensor.addrType = tensorInfo.addrType;
    tensor.argsOffSet = tensorInfo.argsOffSet;
    std::vector<int64_t> shape = tensorInfo.shape;
    for (auto dim : shape) {
        tensor.shape.emplace_back(static_cast<uint64_t>(dim));
    }
    std::vector<int64_t> originShape = tensorInfo.originShape;
    for (auto dim : originShape) {
        tensor.originShape.emplace_back(static_cast<uint64_t>(dim));
    }
}

int32_t DumpManager::DelExceptionOp(uint32_t deviceId, uint32_t streamId)
{
    return exceptionDumper_.DelDumpOperator(deviceId, streamId);
}

int32_t DumpManager::DumpExceptionInfo(const rtExceptionInfo& exception)
{
    return exceptionDumper_.DumpException(exception);
}

uint64_t DumpManager::AdumpGetDumpSwitch()
{
    std::lock_guard<std::mutex> lk(resourceMtx_);
    return dumpSetting_.GetDumpSwitch();
}

bool DumpManager::RegsiterExceptionCallback()
{
    if (!registered_ && rtRegTaskFailCallbackByModule(EXCEPTION_CB_MODULE, ExceptionCallback) == RT_ERROR_NONE) {
        registered_ = true;
    }
    IDE_LOGI("Register exception callback, registered: %d", static_cast<int32_t>(registered_));
    return registered_;
}

DumpSetting DumpManager::GetDumpSetting() const
{
    return dumpSetting_;
}

void DumpManager::ExceptionModeDowngrade()
{
    exceptionDumper_.ExceptionModeDowngrade();
}

int32_t DumpManager::RegisterCallback(uint32_t moduleId, AdumpCallback enableFunc, AdumpCallback disableFunc)
{
    if (enableFunc == nullptr) {
        IDE_LOGE("Register callback failed: enableFunc is null for module %u", moduleId);
        return ADUMP_FAILED;
    }
    if (disableFunc == nullptr) {
        IDE_LOGE("Register callback failed: disableFunc is null for module %u", moduleId);
        return ADUMP_FAILED;
    }
    std::lock_guard<std::mutex> lk(resourceMtx2_);
    enableCallbackFunc_[moduleId] = enableFunc;
    disableCallbackFunc_[moduleId] = disableFunc;
    IDE_LOGI("Registered callback for module %u", moduleId);
    return HandleDumpEvent(moduleId, DumpEnableAction::AUTO);
}

int32_t DumpManager::StartDumpArgs(const std::string& dumpPath)
{
    std::lock_guard<std::mutex> lk(resourceMtx_);
    uint64_t dumpSwitch = dumpSetting_.GetDumpSwitch();
    if ((dumpSwitch & OP_INFO_RECORD_DUMP) == OP_INFO_RECORD_DUMP) {
        IDE_LOGW("Double OpInfoRecord Start Entry");
        return -1;
    }

    Adx::Path path(dumpPath);
    if (path.Empty()) {
        IDE_LOGE("OpInfoRecord path[%s] is empty", dumpPath.c_str());
        return -1;
    }
    if (!path.Exist()) {
        if (!path.CreateDirectory(true)) {
            IDE_LOGE("Create path[%s] failed, strerr: %s", dumpPath.c_str(), strerror(errno));
            return -1;
        }
    }
    if (!path.IsDirectory()) {
        IDE_LOGE("OpInfoRecord path[%s] is not directory!", dumpPath.c_str());
        return -1;
    }

    dumpSwitch |= OP_INFO_RECORD_DUMP;
    dumpSetting_.InitDumpSwitch(dumpSwitch);
    opInfoRecordPath_ = path.GetString();

    for (auto& item : enableCallbackFunc_) {
        item.second(dumpSwitch, dumpConfigInfo_.data(), dumpConfigInfo_.size());
    }
    IDE_RUN_LOGI("OpInfoRecord start success!");
    return 0;
}

int32_t DumpManager::StopDumpArgs()
{
    std::lock_guard<std::mutex> lk(resourceMtx_);
    uint64_t dumpSwitch = dumpSetting_.GetDumpSwitch();
    if ((dumpSwitch & OP_INFO_RECORD_DUMP) != OP_INFO_RECORD_DUMP) {
        return 0;
    }
    IDE_RUN_LOGI("OpInfoRecord Stop Entry!");
    dumpSwitch &= ~OP_INFO_RECORD_DUMP;
    dumpSetting_.InitDumpSwitch(dumpSwitch);
    for (auto& item : disableCallbackFunc_) {
        item.second(dumpSwitch, dumpConfigInfo_.data(), dumpConfigInfo_.size());
    }
    IDE_RUN_LOGI("OpInfoRecord success!");
    return 0;
}

const char* DumpManager::GetExceptionDumpPath()
{
    std::lock_guard<std::mutex> lk(resourceMtx_);
    exceptionDumper_.CreateExtraDumpPath();
    return exceptionDumper_.GetExtraDumpCPath();
}

const char* DumpManager::GetDataDumpPath()
{
    std::lock_guard<std::mutex> lk(resourceMtx_);
    return dumpSetting_.GetDumpCPath();
}

int32_t DumpManager::SaveFile(const char* data, size_t dataLen, const char* fileName, SaveType type)
{
    Adx::Path filePath(opInfoRecordPath_);
    filePath.Concat(fileName);
    Adx::Path dirPath = filePath.ParentPath();
    if (!dirPath.Exist()) {
        if (!dirPath.CreateDirectory(true)) {
            IDE_LOGE("create directory[%s] failed", dirPath.GetCString());
            return -1;
        }
    }
    if (!dirPath.RealPath()) {
        IDE_LOGE("get real path failed, path:%s", dirPath.GetCString());
    }

    std::string canonicalPath = dirPath.GetString() + "/" + filePath.GetFileName();
    int32_t openFlag = 0;
    if (type == SaveType::OVERWRITE) {
        openFlag = O_CREAT | O_WRONLY | O_TRUNC;
    } else {
        openFlag = O_CREAT | O_WRONLY | O_APPEND;
    }
    File file(canonicalPath, openFlag);

    if (file.IsFileOpen() != 0) {
        IDE_LOGE("open file[%s] failed!", fileName);
        return -1;
    }
    int64_t ret = file.Write(data, dataLen);
    IDE_CTRL_VALUE_FAILED(ret >= 0, return -1, "Save file %s failed!", fileName);

    IDE_LOGI("DumpJsonToFile %s success!", fileName);
    return 0;
}

int32_t DumpManager::CallbackEnvExceptionDumpEvent(AdumpCallback callbackFunc)
{
    if (isEnvExceptionDump_) {
        IDE_LOGI("Callback module when exception dump enabled with env.");
        if (exceptionDumper_.GetArgsExceptionStatus() || exceptionDumper_.GetCoredumpStatus()) {
            return callbackFunc(DUMP_SWITCH_L0_MACK, nullptr, 0);
        } else if (exceptionDumper_.GetExceptionStatus()) {
            return callbackFunc(DUMP_SWITCH_L1_MACK, nullptr, 0);
        }
    }
    return ADUMP_SUCCESS;
}

// DUMP 配置变化时，触发dump事件，同步回调用户接口
int32_t DumpManager::HandleDumpEvent(uint32_t moduleId, DumpEnableAction action)
{
    const uint64_t dumpSwitch = dumpSetting_.GetDumpSwitch();
    auto callbackFunc = disableCallbackFunc_[moduleId];
    if (action == DumpEnableAction::ENABLE) {
        callbackFunc = enableCallbackFunc_[moduleId];
        // 回调环境变量开启exception dump
        (void)CallbackEnvExceptionDumpEvent(callbackFunc);
    }
    if (action == DumpEnableAction::AUTO) {
        // 回调环境变量开启exception dump
        (void)CallbackEnvExceptionDumpEvent(enableCallbackFunc_[moduleId]);
        if (dumpConfigInfo_.data() == nullptr || dumpConfigInfo_.size() == 0U) {
            IDE_LOGW("Config data is null or empty. Not trigger HandleDumpEvent.");
            return ADUMP_SUCCESS;
        }
        if (dumpSwitch > 0U) {
            callbackFunc = enableCallbackFunc_[moduleId];
        }
    }

    if (!callbackFunc) {
        IDE_LOGE("No registered callback for module %u", moduleId);
        return ADUMP_FAILED;
    }

    IDE_LOGI("HandleDumpEvent callbackFunc start for module [%zu]", moduleId);
    IDE_LOGI("HandleDumpEvent callbackFunc switch [%" PRIu64 "]", dumpSwitch);
    IDE_LOGI(
        "HandleDumpEvent callbackFunc Dump config info: addr=%p, size=%zu", dumpConfigInfo_.data(),
        dumpConfigInfo_.size());
    int32_t result = callbackFunc(dumpSwitch, dumpConfigInfo_.data(), dumpConfigInfo_.size());
    IDE_LOGI("callbackFunc returned: %d", result);
    return result;
}

#ifdef __ADUMP_LLT
void DumpManager::Reset()
{
    registered_ = false;
    exceptionDumper_.Reset();
}

bool DumpManager::GetKFCInitStatus()
{
    return isKFCInit_;
}

void DumpManager::SetKFCInitStatus(bool status)
{
    isKFCInit_ = status;
}
#endif
} // namespace Adx
