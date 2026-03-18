/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "securec.h"
#define private public
#define protected public
#include "runtime.hpp"
#include "model.hpp"
#include "raw_device.hpp"
#include "module.hpp"
#include "notify.hpp"
#include "event.hpp"
#include "task_info.hpp"
#include "task_info.h"
#include "ffts_task.h"
#include "device/device_error_proc.hpp"
#include "program.hpp"
#include "uma_arg_loader.hpp"
#include "npu_driver.hpp"
#include "ctrl_res_pool.hpp"
#include "stream_sqcq_manage.hpp"
#include "davinci_kernel_task.h"
#include "profiler.hpp"
#include "rdma_task.h"
#include "capture_model_utils.hpp"
#include "thread_local_container.hpp"
#undef private
#undef protected

using namespace testing;
using namespace cce::runtime;

class CloudV2CaptureModelUpdateTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {}

    static void TearDownTestCase()
    {}

    virtual void SetUp()
    {
        auto ret = rtSetDevice(0);
        EXPECT_EQ(ret, RT_ERROR_NONE);
        MOCKER(CheckCaptureModelSupportSoftwareSq).stubs().will(returnValue(RT_ERROR_NONE));
        Device* device = ((Runtime*)Runtime::Instance())->DeviceRetain(0, 0);
        MOCKER_CPP_VIRTUAL(device, &Device::CheckFeatureSupport).stubs().will(returnValue(true));

        for (uint32_t i = 0; i < sizeof(binary_) / sizeof(uint32_t); i++) {
            binary_[i] = i;
        }

        rtDevBinary_t devBin;
        devBin.magic = RT_DEV_BINARY_MAGIC_PLAIN;
        devBin.version = 1;
        devBin.length = sizeof(binary_);
        devBin.data = binary_;
        ret = rtDevBinaryRegister(&devBin, &binHandle_);
        EXPECT_EQ(ret, RT_ERROR_NONE);
        ret = rtFunctionRegister(binHandle_, &function_, "foo", NULL, 0);
        EXPECT_EQ(ret, RT_ERROR_NONE);
        for (uint32_t i = 0; i < sizeof(binary_) / sizeof(uint32_t); i++) {
            binary_[i] = i;
        }
    }

    virtual void TearDown()
    {
        auto ret = rtDevBinaryUnRegister(binHandle_);
        EXPECT_EQ(ret, RT_ERROR_NONE);
        rtDeviceReset(0);
        GlobalMockObject::verify();
    }

public:
    void* binHandle_;
    char function_;
    uint32_t binary_[32];
};

TEST_F(CloudV2CaptureModelUpdateTest, rtModelTaskGetParams_Success)
{
    rtContext_t ctx;
    rtError_t ret = rtCtxCreate(&ctx, 0, 0);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    rtStream_t stream;
    ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtStreamBeginCapture(stream, RT_STREAM_CAPTURE_MODE_GLOBAL);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    MOCKER(memcpy_s).stubs().will(returnValue(NULL));

    void* args[] = {&ret, NULL};
    ret = rtKernelLaunch(&function_, 1, (void*)args, sizeof(args), NULL, stream);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    rtModel_t model;
    ret = rtStreamEndCapture(stream, &model);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    uint32_t numStreams = 1;
    rtStream_t inputStreams[numStreams];
    ret = rtModelGetStreams(model, inputStreams, &numStreams);
    EXPECT_EQ(ret, RT_ERROR_NONE);
    EXPECT_EQ(numStreams, 1);

    uint32_t numTask = 2;
    rtTask_t inputTasks[numTask];
    ret = rtStreamGetTasks(inputStreams[0], inputTasks, &numTask);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    rtTaskParams params;
    ret = rtModelTaskGetParams(inputTasks[0], &params);
    EXPECT_EQ(ret, RT_ERROR_NONE);
    EXPECT_EQ(params.type, RT_TASK_KERNEL);

    ret = rtModelDestroy(model);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtStreamDestroy(stream);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtCtxDestroy(ctx);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    GlobalMockObject::verify();
}

TEST_F(CloudV2CaptureModelUpdateTest, rtModelTaskSetParams_Success)
{
    rtContext_t ctx;
    rtError_t ret = rtCtxCreate(&ctx, 0, 0);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    rtStream_t stream;
    ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtStreamBeginCapture(stream, RT_STREAM_CAPTURE_MODE_GLOBAL);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    MOCKER(memcpy_s).stubs().will(returnValue(NULL));

    void* args[] = {&ret, NULL};
    ret = rtKernelLaunch(&function_, 1, (void*)args, sizeof(args), NULL, stream);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    rtModel_t model;
    ret = rtStreamEndCapture(stream, &model);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    uint32_t numStreams = 1;
    rtStream_t inputStreams[numStreams];
    ret = rtModelGetStreams(model, inputStreams, &numStreams);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    uint32_t numTask = 1;
    rtTask_t inputTasks[numTask];
    ret = rtStreamGetTasks(inputStreams[0], inputTasks, &numTask);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    rtTaskParams params;
    memset_s(&params, sizeof(rtTaskParams), 0, sizeof(rtTaskParams));
    ret = rtModelTaskGetParams(inputTasks[0], &params);
    EXPECT_EQ(ret, RT_ERROR_NONE);
    EXPECT_EQ(params.type, RT_TASK_KERNEL);

    ret = rtModelTaskSetParams(inputTasks[0], &params);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtModelDestroy(model);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtStreamDestroy(stream);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtCtxDestroy(ctx);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    GlobalMockObject::verify();
}

TEST_F(CloudV2CaptureModelUpdateTest, rtModelUpdate_Success)
{
    rtContext_t ctx;
    rtError_t ret = rtCtxCreate(&ctx, 0, 0);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    rtStream_t stream;
    ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtStreamBeginCapture(stream, RT_STREAM_CAPTURE_MODE_GLOBAL);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    MOCKER(memcpy_s).stubs().will(returnValue(NULL));

    void* args[] = {&ret, NULL};
    ret = rtKernelLaunch(&function_, 1, (void*)args, sizeof(args), NULL, stream);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    rtModel_t model;
    ret = rtStreamEndCapture(stream, &model);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    uint32_t numStreams = 1;
    rtStream_t inputStreams[numStreams];
    ret = rtModelGetStreams(model, inputStreams, &numStreams);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    uint32_t numTask = 1;
    rtTask_t inputTasks[numTask];
    ret = rtStreamGetTasks(inputStreams[0], inputTasks, &numTask);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    rtTaskParams params;
    memset_s(&params, sizeof(rtTaskParams), 0, sizeof(rtTaskParams));
    ret = rtModelTaskGetParams(inputTasks[0], &params);
    EXPECT_EQ(ret, RT_ERROR_NONE);
    EXPECT_EQ(params.type, RT_TASK_KERNEL);

    ret = rtModelTaskSetParams(inputTasks[0], &params);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtModelUpdate(model);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtModelDestroy(model);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtStreamDestroy(stream);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtCtxDestroy(ctx);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    GlobalMockObject::verify();
}

TEST_F(CloudV2CaptureModelUpdateTest, rtModelTaskDisable_Success)
{
    rtContext_t ctx;
    rtError_t ret = rtCtxCreate(&ctx, 0, 0);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    rtStream_t stream;
    ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtStreamBeginCapture(stream, RT_STREAM_CAPTURE_MODE_GLOBAL);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    MOCKER(memcpy_s).stubs().will(returnValue(NULL));

    void* args[] = {&ret, NULL};
    ret = rtKernelLaunch(&function_, 1, (void*)args, sizeof(args), NULL, stream);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    rtModel_t model;
    ret = rtStreamEndCapture(stream, &model);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    uint32_t numStreams = 1;
    rtStream_t inputStreams[numStreams];
    ret = rtModelGetStreams(model, inputStreams, &numStreams);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    uint32_t numTask = 1;
    rtTask_t inputTasks[numTask];
    ret = rtStreamGetTasks(inputStreams[0], inputTasks, &numTask);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    rtTaskParams params;
    memset_s(&params, sizeof(rtTaskParams), 0, sizeof(rtTaskParams));
    ret = rtModelTaskGetParams(inputTasks[0], &params);
    EXPECT_EQ(ret, RT_ERROR_NONE);
    EXPECT_EQ(params.type, RT_TASK_KERNEL);

    ret = rtModelTaskDisable(inputTasks[0]);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtModelUpdate(model);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtModelDestroy(model);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtStreamDestroy(stream);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtCtxDestroy(ctx);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    GlobalMockObject::verify();
}

TEST_F(CloudV2CaptureModelUpdateTest, rtModelTaskDefault_Success)
{
    rtContext_t ctx;
    rtError_t ret = rtCtxCreate(&ctx, 0, 0);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    rtStream_t stream;
    ret = rtStreamCreate(&stream, 0);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtStreamBeginCapture(stream, RT_STREAM_CAPTURE_MODE_GLOBAL);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    MOCKER(memcpy_s).stubs().will(returnValue(NULL));

    void* args[] = {&ret, NULL};
    ret = rtKernelLaunch(&function_, 1, (void*)args, sizeof(args), NULL, stream);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtKernelLaunch(&function_, 1, (void*)args, sizeof(args), NULL, stream);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    rtModel_t model;
    ret = rtStreamEndCapture(stream, &model);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    uint32_t numStreams = 1;
    rtStream_t inputStreams[numStreams];
    ret = rtModelGetStreams(model, inputStreams, &numStreams);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    uint32_t numTask = 2;
    rtTask_t inputTasks[numTask];
    ret = rtStreamGetTasks(inputStreams[0], inputTasks, &numTask);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    rtTaskParams params;
    memset_s(&params, sizeof(rtTaskParams), 0, sizeof(rtTaskParams));
    ret = rtModelTaskGetParams(inputTasks[0], &params);
    EXPECT_EQ(ret, RT_ERROR_NONE);
    EXPECT_EQ(params.type, RT_TASK_KERNEL);

    ret = rtModelTaskDisable(inputTasks[0]);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtModelUpdate(model);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtModelDestroy(model);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtStreamDestroy(stream);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    ret = rtCtxDestroy(ctx);
    EXPECT_EQ(ret, RT_ERROR_NONE);

    GlobalMockObject::verify();
}