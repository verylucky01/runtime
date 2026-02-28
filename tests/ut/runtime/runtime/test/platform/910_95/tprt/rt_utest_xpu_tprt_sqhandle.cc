/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#define private public
#include "tprt_sqhandle.hpp"
#undef private

using namespace cce::tprt;

class TprtSqHandleTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "TprtSqHandleTest SetUP" << std::endl;
        std::cout << "TprtSqHandleTest start" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "TprtSqHandleTest end" << std::endl;
    }

    virtual void SetUp()
    {}

    virtual void TearDown()
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TprtSqHandleTest, create_TprtSqHandle_Success_01)
{
    TprtSqHandle *sqHdl = new TprtSqHandle(0, 0);
    EXPECT_EQ(sqHdl->sqState_, TPRT_SQ_STATE_IS_RUNNING);
    DELETE_O(sqHdl);
}

TEST_F(TprtSqHandleTest, GetTaskTimeout_TaskSpecificTimeout)
{
    TprtSqHandle *sqHdl = new TprtSqHandle(0, 0);

    TprtSqe_t headTask = {};
    headTask.aicpuSqe.timeout = 1500000U; 

    uint32_t timeout = sqHdl->GetTaskTimeout(&headTask);
    EXPECT_EQ(timeout, 2U);

    DELETE_O(sqHdl);
}


TEST_F(TprtSqHandleTest, SetTimeoutWaitInfo_Success)
{
    TprtSqHandle *sqHandle = new TprtSqHandle(0, 0);
    sqHandle->sqHead_ = 0U;
    sqHandle->sqTail_ = 1U;
    TprtSqe_t headTask = {};
    headTask.commonSqe.sqeHeader.taskSn = 1;
    headTask.aicpuSqe.timeout = 5000000U; 
    sqHandle->sqQueue_[sqHandle->sqHead_] = headTask;

    sqHandle->SetTimeoutWaitInfo();

    EXPECT_TRUE(sqHandle->waitInfo_.isNeedProcess);
    EXPECT_EQ(sqHandle->waitInfo_.waitTaskSn, 1);
    EXPECT_EQ(sqHandle->waitInfo_.timeout, 5U); 

    DELETE_O(sqHandle);
}