/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "driver/ascend_hal.h"
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "securec.h"
#define private public
#define protected public
#include "runtime.hpp"
#include "raw_device.hpp"
#include "thread_local_container.hpp"
#include "rts_snapshot.h"
#include "global_state_manager.hpp"
#include "api_decorator.hpp"
#undef private
#undef protected

using namespace testing;
using namespace cce::runtime;

class SnapshotTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }

    virtual void SetUp()
    {
        rtError_t error = rtSetDevice(0);
        ASSERT_EQ(error, RT_ERROR_NONE);

        device_ = Runtime::Instance()->GetDevice(0, 0);
        ASSERT_NE(device_, nullptr);

        deviceSnapshot_ = new (std::nothrow) DeviceSnapshot(device_);
        ASSERT_NE(deviceSnapshot_, nullptr);

        eventPool_ = new (std::nothrow) EventExpandingPool(device_);
        ASSERT_NE(eventPool_, nullptr);
    }

    virtual void TearDown()
    {
        DELETE_O(deviceSnapshot_);
        DELETE_O(eventPool_);
        rtDeviceReset(0);
        GlobalMockObject::verify();
    }

    Device* device_{nullptr};
    DeviceSnapshot* deviceSnapshot_{nullptr};
    EventExpandingPool* eventPool_{nullptr};
};

TEST_F(SnapshotTest, Lock_Unlock)
{
    rtError_t error = rtSnapShotProcessLock();
    EXPECT_EQ(error, ACL_RT_SUCCESS);
    rtProcessState state;
    error = rtSnapShotProcessGetState(&state);
    EXPECT_EQ(error, ACL_RT_SUCCESS);
    EXPECT_EQ(state, RT_PROCESS_STATE_LOCKED);
    error = rtSnapShotProcessUnlock();
    EXPECT_EQ(error, ACL_RT_SUCCESS);
    error = rtSnapShotProcessGetState(&state);
    EXPECT_EQ(error, ACL_RT_SUCCESS);
    EXPECT_EQ(state, RT_PROCESS_STATE_RUNNING);
}

TEST_F(SnapshotTest, LockFailed)
{
    rtError_t error = rtSnapShotProcessLock();
    EXPECT_EQ(error, ACL_RT_SUCCESS);
    error = rtSnapShotProcessLock();
    EXPECT_EQ(error, ACL_ERROR_SNAPSHOT_LOCK_FAILED);
    error = rtSnapShotProcessUnlock();
    EXPECT_EQ(error, ACL_RT_SUCCESS);
}

TEST_F(SnapshotTest, UnlockFailed)
{
    const rtError_t error = rtSnapShotProcessUnlock();
    EXPECT_EQ(error, ACL_ERROR_SNAPSHOT_UNLOCK_FAILED);
}

TEST_F(SnapshotTest, BackUpFailed)
{
    const rtError_t error = rtSnapShotProcessBackup();
    EXPECT_EQ(error, ACL_ERROR_SNAPSHOT_BACKUP_FAILED);
}

TEST_F(SnapshotTest, RestoreFailed)
{
    const rtError_t error = rtSnapShotProcessRestore();
    EXPECT_EQ(error, ACL_ERROR_SNAPSHOT_RESTORE_FAILED);
}

TEST_F(SnapshotTest, StateToString)
{
    EXPECT_STREQ(GlobalStateManager::StateToString(RT_PROCESS_STATE_BACKED_UP), "BACKED_UP");
    EXPECT_STREQ(GlobalStateManager::StateToString(RT_PROCESS_STATE_MAX), "UNKNOWN");
}

TEST_F(SnapshotTest, ForceUnlocked)
{
    GlobalStateManager::GetInstance().ForceUnlocked();
    rtProcessState state;
    const auto error = rtSnapShotProcessGetState(&state);
    EXPECT_EQ(error, ACL_RT_SUCCESS);
    EXPECT_EQ(state, RT_PROCESS_STATE_RUNNING);
}

TEST_F(SnapshotTest, ForceUnlockedAfterLocked)
{
    rtError_t error = rtSnapShotProcessLock();
    EXPECT_EQ(error, ACL_RT_SUCCESS);
    GlobalStateManager::GetInstance().ForceUnlocked();
    rtProcessState state;
    error = rtSnapShotProcessGetState(&state);
    EXPECT_EQ(error, ACL_RT_SUCCESS);
    EXPECT_EQ(state, RT_PROCESS_STATE_RUNNING);
}

TEST_F(SnapshotTest, GetStateNull)
{
    rtError_t error = rtSnapShotProcessGetState(NULL);
    EXPECT_EQ(error, ACL_ERROR_RT_PARAM_INVALID);
}

static uint32_t rtSnapShotCallBackUt(int32_t devId, void *args)
{
    std::cout << "snap shot call back" << std::endl;
    return 0U;
}

static uint32_t rtSnapShotCallBackUtFailed(int32_t devId, void *args)
{
    std::cout << "snap shot call back failed" << std::endl;
    return 1U;
}

TEST_F(SnapshotTest, Chip_Support)
{
    Runtime *rtInstance = (Runtime *)Runtime::Instance();
    rtChipType_t oldChipType = rtInstance->GetChipType();
    rtInstance->SetChipType(CHIP_CLOUD);
    GlobalContainer::SetRtChipType(CHIP_CLOUD);

    rtError_t error = rtSnapShotProcessLock();
    EXPECT_EQ(error, ACL_ERROR_RT_FEATURE_NOT_SUPPORT);
    rtProcessState state;
    error = rtSnapShotProcessGetState(&state);
    EXPECT_EQ(error, ACL_ERROR_RT_FEATURE_NOT_SUPPORT);
    error = rtSnapShotProcessUnlock();
    EXPECT_EQ(error, ACL_ERROR_RT_FEATURE_NOT_SUPPORT);
    error = rtSnapShotProcessBackup();
    EXPECT_EQ(error, ACL_ERROR_RT_FEATURE_NOT_SUPPORT);
    error = rtSnapShotProcessRestore();
    EXPECT_EQ(error, ACL_ERROR_RT_FEATURE_NOT_SUPPORT);
    error = rtSnapShotCallbackRegister(RT_SNAPSHOT_LOCK_PRE, rtSnapShotCallBackUt, nullptr);
    EXPECT_EQ(error, ACL_ERROR_RT_FEATURE_NOT_SUPPORT);
    error = rtSnapShotCallbackUnregister(RT_SNAPSHOT_LOCK_PRE, rtSnapShotCallBackUt);
    EXPECT_EQ(error, ACL_ERROR_RT_FEATURE_NOT_SUPPORT);

    rtInstance->SetChipType(oldChipType);
    GlobalContainer::SetRtChipType(oldChipType);
    GlobalMockObject::verify();
}

TEST_F(SnapshotTest, SnapShotCallbackUnregisterFailed)
{
    const auto error = rtSnapShotCallbackUnregister(RT_SNAPSHOT_LOCK_PRE, rtSnapShotCallBackUt);
    EXPECT_EQ(error, ACL_ERROR_RT_PARAM_INVALID);
}

TEST_F(SnapshotTest, SnapShotCallbackRegister)
{
    rtError_t error = rtSnapShotCallbackRegister(RT_SNAPSHOT_LOCK_PRE, rtSnapShotCallBackUt, nullptr);
    EXPECT_EQ(error, ACL_RT_SUCCESS);
    error = rtSnapShotCallbackRegister(RT_SNAPSHOT_LOCK_PRE, rtSnapShotCallBackUt, nullptr);
    EXPECT_EQ(error, ACL_ERROR_SNAPSHOT_REGISTER_CALLBACK_FAILED);
    error = rtSnapShotCallbackUnregister(RT_SNAPSHOT_LOCK_PRE, rtSnapShotCallBackUt);
    EXPECT_EQ(error, ACL_RT_SUCCESS);
    error = rtSnapShotCallbackUnregister(RT_SNAPSHOT_LOCK_PRE, rtSnapShotCallBackUt);
    EXPECT_EQ(error, ACL_ERROR_RT_PARAM_INVALID);
}

TEST_F(SnapshotTest, SnapShotCallbackFailed)
{
    int32_t devId = 0;
    rtError_t error = rtSetDevice(devId);
    EXPECT_EQ(error, ACL_RT_SUCCESS);

    error = rtSnapShotCallbackRegister(RT_SNAPSHOT_LOCK_PRE, rtSnapShotCallBackUtFailed, nullptr);
    EXPECT_EQ(error, ACL_RT_SUCCESS);

    error = rtSnapShotProcessLock();
    EXPECT_EQ(error, ACL_ERROR_SNAPSHOT_CALLBACK_FAILED);
    
    error = rtSnapShotCallbackUnregister(RT_SNAPSHOT_LOCK_PRE, rtSnapShotCallBackUtFailed);
    EXPECT_EQ(error, ACL_RT_SUCCESS);

    error = rtDeviceReset(devId);
    EXPECT_EQ(error, ACL_RT_SUCCESS);
}

TEST_F(SnapshotTest, SnapShotCallbackRegisterLock)
{
    int32_t devId = 0;
    rtError_t error = rtSetDevice(devId);
    EXPECT_EQ(error, ACL_RT_SUCCESS);

    error = rtSnapShotCallbackRegister(RT_SNAPSHOT_LOCK_PRE, rtSnapShotCallBackUt, nullptr);
    EXPECT_EQ(error, ACL_RT_SUCCESS);

    error = rtSnapShotProcessLock();
    EXPECT_EQ(error, ACL_RT_SUCCESS);

    MOCKER(ContextManage::SnapShotProcessBackup).stubs().will(returnValue(RT_ERROR_NONE));
    MOCKER(ContextManage::SnapShotProcessRestore).stubs().will(returnValue(RT_ERROR_NONE));

    error = rtSnapShotProcessBackup();
    EXPECT_EQ(error, ACL_RT_SUCCESS);

    error = rtSnapShotProcessRestore();
    EXPECT_EQ(error, ACL_RT_SUCCESS);

    error = rtSnapShotProcessUnlock();
    EXPECT_EQ(error, ACL_RT_SUCCESS);

    error = rtSnapShotCallbackUnregister(RT_SNAPSHOT_LOCK_PRE, rtSnapShotCallBackUt);
    EXPECT_EQ(error, ACL_RT_SUCCESS);

    error = rtDeviceReset(devId);
    EXPECT_EQ(error, ACL_RT_SUCCESS);
}

TEST_F(SnapshotTest, SnapShotCallbackRegisterLockApiDecorator)
{
    Api *oldApi_= const_cast<Api *>(Runtime::runtime_->api_);
    ApiDecorator apiDecorator(oldApi_);

    int32_t devId = 0;
    rtError_t error = rtSetDevice(devId);
    EXPECT_EQ(error, ACL_RT_SUCCESS);

    error = apiDecorator.SnapShotCallbackRegister(RT_SNAPSHOT_LOCK_PRE, rtSnapShotCallBackUt, nullptr);
    EXPECT_EQ(error, ACL_RT_SUCCESS);

    error = apiDecorator.SnapShotProcessLock();
    EXPECT_EQ(error, ACL_RT_SUCCESS);

    error = apiDecorator.SnapShotProcessUnlock();
    EXPECT_EQ(error, ACL_RT_SUCCESS);

    error = apiDecorator.SnapShotCallbackUnregister(RT_SNAPSHOT_LOCK_PRE, rtSnapShotCallBackUt);
    EXPECT_EQ(error, ACL_RT_SUCCESS);

    error = rtDeviceReset(devId);
    EXPECT_EQ(error, ACL_RT_SUCCESS);
}

TEST_F(SnapshotTest, StreamTaskClean)
{
    int32_t devId = 0;
    rtError_t error = rtSetDevice(devId);
    EXPECT_EQ(error, ACL_RT_SUCCESS);

    rtStream_t stream;
    error = rtStreamCreateWithFlags(&stream, 0U, RT_STREAM_PERSISTENT);
    EXPECT_EQ(error, ACL_RT_SUCCESS);

    Stream* str = static_cast<Stream*>(stream);
    str->bindFlag_.Set(true);
    error = str->StreamTaskClean();
    EXPECT_EQ(error, ACL_RT_SUCCESS);

    error = rtStreamDestroy(stream);
    EXPECT_EQ(error, ACL_RT_SUCCESS);

    error = rtDeviceReset(devId);
    EXPECT_EQ(error, ACL_RT_SUCCESS);
}

TEST_F(SnapshotTest, AddArgPcieBar_Single) {
    void* testAddr = reinterpret_cast<void*>(0x1000);
    size_t testSize = 4096;

    deviceSnapshot_->AddArgPcieBar(testAddr, testSize);

    const auto& barInfos = deviceSnapshot_->GetArgPcieBarInfos();
    EXPECT_EQ(barInfos.size(), 1U);
    EXPECT_EQ(barInfos[0].first, testAddr);
    EXPECT_EQ(barInfos[0].second, testSize);
}

TEST_F(SnapshotTest, AddArgPcieBar_Multiple) {
    std::vector<std::pair<void*, size_t>> testData = {
        {reinterpret_cast<void*>(0x1000), 4096},
        {reinterpret_cast<void*>(0x2000), 8192},
        {reinterpret_cast<void*>(0x3000), 16384}
    };

    for (const auto& data : testData) {
        deviceSnapshot_->AddArgPcieBar(data.first, data.second);
    }

    const auto& barInfos = deviceSnapshot_->GetArgPcieBarInfos();
    EXPECT_EQ(barInfos.size(), testData.size());

    for (size_t i = 0; i < testData.size(); ++i) {
        EXPECT_EQ(barInfos[i].first, testData[i].first);
        EXPECT_EQ(barInfos[i].second, testData[i].second);
    }
}

TEST_F(SnapshotTest, GetArgPcieBarInfos_Empty) {
    const auto& barInfos = deviceSnapshot_->GetArgPcieBarInfos();
    EXPECT_EQ(barInfos.size(), 0U);
}

TEST_F(SnapshotTest, ResetBufferForEvent_Normal) {
    void* eventAddr = nullptr;
    int32_t eventId = 0;
    rtError_t ret = eventPool_->AllocAndInsertEvent(&eventAddr, &eventId);
    ASSERT_EQ(ret, RT_ERROR_NONE);
    ASSERT_NE(eventAddr, nullptr);

    ret = eventPool_->ResetBufferForEvent();
    EXPECT_EQ(ret, RT_ERROR_NONE);
}

TEST_F(SnapshotTest, ResetBufferForEvent_MultiplePools) {
    void* eventAddr = nullptr;
    int32_t eventId = 0;

    for (int i = 0; i < 4 * 1024 * 1024; ++i) {
        rtError_t ret = eventPool_->AllocAndInsertEvent(&eventAddr, &eventId);
        ASSERT_EQ(ret, RT_ERROR_NONE);
    }

    uint16_t poolIndex = eventPool_->GetPoolIndex();
    EXPECT_GT(poolIndex, 0U);

    rtError_t ret = eventPool_->ResetBufferForEvent();
    EXPECT_EQ(ret, RT_ERROR_NONE);
}

TEST_F(SnapshotTest, ResetBufferForEvent_MemsetBuffersFailed) {
    void* eventAddr = nullptr;
    int32_t eventId = 0;
    rtError_t ret = eventPool_->AllocAndInsertEvent(&eventAddr, &eventId);
    ASSERT_EQ(ret, RT_ERROR_NONE);

    NpuDriver *drv = dynamic_cast<NpuDriver *>(device_->Driver_());
    EXPECT_NE(drv, nullptr);
    MOCKER_CPP_VIRTUAL(drv, &NpuDriver::MemSetSync).stubs().will(returnValue(RT_ERROR_DRV_NOT_SUPPORT));

    ret = eventPool_->ResetBufferForEvent();
    EXPECT_NE(ret, RT_ERROR_NONE);
}

TEST_F(SnapshotTest, GetPoolIndex_Initial) {
    uint16_t poolIndex = eventPool_->GetPoolIndex();
    EXPECT_EQ(poolIndex, 0U);
}

TEST_F(SnapshotTest, GetPoolIndex_AfterAlloc) {
    void* eventAddr = nullptr;
    int32_t eventId = 0;

    rtError_t ret = eventPool_->AllocAndInsertEvent(&eventAddr, &eventId);
    ASSERT_EQ(ret, RT_ERROR_NONE);

    uint16_t poolIndex = eventPool_->GetPoolIndex();
    EXPECT_GE(poolIndex, 0U);
}