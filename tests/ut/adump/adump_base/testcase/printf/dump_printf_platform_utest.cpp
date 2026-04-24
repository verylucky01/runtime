/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <gtest/gtest.h>
#include <string.h>
#include <fstream>
#include <functional>
#include <thread>
#include "mockcpp/mockcpp.hpp"
#include "runtime/dev.h"
#include "runtime/rt.h"
#include "acl/error_codes/rt_error_codes.h"
#include "dump_printf_platform.h"

class DumpPrintfPlatformUtest : public testing::Test {
protected:
    virtual void SetUp() {
        setenv("ADX_LLT_SOC_VERSION", "Ascend910B", 1);
    }
    virtual void TearDown() {
        setenv("ADX_LLT_SOC_VERSION", "", 1);
        GlobalMockObject::verify();
    }
};

TEST_F(DumpPrintfPlatformUtest, Test_DumpPrintfPlatform)
{
    EXPECT_EQ(AdxGetCoreTypeIDOffset(), 50U);
    EXPECT_EQ(AdxGetBlockNum(), 75U);
    EXPECT_EQ(AdxEnableSimtDump(0), false);
    EXPECT_EQ(GetStreamSynchronizeTimeout(), 60000U);
}

TEST_F(DumpPrintfPlatformUtest, Test_Ascend950Platform)
{
    setenv("ADX_LLT_SOC_VERSION", "Ascend950PR_9599", 1);
    EXPECT_EQ(AdxIsAscend950(), true);
    EXPECT_EQ(AdxGetCoreTypeIDOffset(), 72U);
    EXPECT_EQ(AdxGetBlockNum(), 108U);
    EXPECT_EQ(AdxEnableSimtDump(1024U * 1024U * 200U), true);
    EXPECT_EQ(GetStreamSynchronizeTimeout(), 60000U * 30U);
}

TEST_F(DumpPrintfPlatformUtest, Test_EnableSimtDumpThreshold)
{
    setenv("ADX_LLT_SOC_VERSION", "Ascend950PR_9599", 1);
    size_t threshold = AdxGetBlockNum() * (1024U * 1024U);
    EXPECT_EQ(AdxEnableSimtDump(threshold - 1), false);
    EXPECT_EQ(AdxEnableSimtDump(threshold + 1), true);
}

TEST_F(DumpPrintfPlatformUtest, Test_AdxIsAscend950_WithNon950Version)
{
    setenv("ADX_LLT_SOC_VERSION", "Ascend910B", 1);
    EXPECT_EQ(AdxIsAscend950(), false);
    EXPECT_EQ(AdxGetCoreTypeIDOffset(), 50U);
    EXPECT_EQ(AdxGetBlockNum(), 75U);
    EXPECT_EQ(GetStreamSynchronizeTimeout(), 60000U);
}

TEST_F(DumpPrintfPlatformUtest, Test_AdxIsAscend950_With310PVersion)
{
    setenv("ADX_LLT_SOC_VERSION", "Ascend310P", 1);
    EXPECT_EQ(AdxIsAscend950(), false);
}

TEST_F(DumpPrintfPlatformUtest, Test_AdxIsAscend950_WithEmptyVersion)
{
    setenv("ADX_LLT_SOC_VERSION", "", 1);
    EXPECT_EQ(AdxIsAscend950(), false);
    EXPECT_EQ(AdxGetCoreTypeIDOffset(), 50U);
    EXPECT_EQ(AdxGetBlockNum(), 75U);
}

TEST_F(DumpPrintfPlatformUtest, Test_EnableSimtDump_Non950Platform)
{
    setenv("ADX_LLT_SOC_VERSION", "Ascend910B", 1);
    EXPECT_EQ(AdxEnableSimtDump(1024U * 1024U * 200U), false);
    EXPECT_EQ(AdxEnableSimtDump(0), false);
}

TEST_F(DumpPrintfPlatformUtest, Test_GetStreamSynchronizeTimeout_Non950)
{
    setenv("ADX_LLT_SOC_VERSION", "Ascend910B", 1);
    EXPECT_EQ(GetStreamSynchronizeTimeout(), 60000U);
}

TEST_F(DumpPrintfPlatformUtest, Test_AdxIsAscend950_RtGetSocVersionFail)
{
    setenv("ADX_LLT_SOC_VERSION", "", 1);
    MOCKER_CPP(&rtGetSocVersion).stubs().will(returnValue(ACL_ERROR_RT_SOC_VERSION));
    EXPECT_EQ(AdxIsAscend950(), false);
}

TEST_F(DumpPrintfPlatformUtest, Test_AdxIsAscend950_PartialMatch)
{
    setenv("ADX_LLT_SOC_VERSION", "Ascend95", 1);
    EXPECT_EQ(AdxIsAscend950(), false);
}

TEST_F(DumpPrintfPlatformUtest, Test_AdxIsAscend950_DifferentPrefix)
{
    setenv("ADX_LLT_SOC_VERSION", "Ascend910", 1);
    EXPECT_EQ(AdxIsAscend950(), false);
}

TEST_F(DumpPrintfPlatformUtest, Test_EnableSimtDump_ExactThreshold)
{
    setenv("ADX_LLT_SOC_VERSION", "Ascend950PR_9599", 1);
    size_t threshold = AdxGetBlockNum() * (1024U * 1024U);
    EXPECT_EQ(AdxEnableSimtDump(threshold), false);
}

TEST_F(DumpPrintfPlatformUtest, Test_EnableSimtDump_LargeWorkspace)
{
    setenv("ADX_LLT_SOC_VERSION", "Ascend950PR_9599", 1);
    EXPECT_EQ(AdxEnableSimtDump(1024U * 1024U * 500U), true);
}

TEST_F(DumpPrintfPlatformUtest, Test_GetCoreTypeIDOffset_MultipleCalls)
{
    setenv("ADX_LLT_SOC_VERSION", "Ascend950PR_9599", 1);
    EXPECT_EQ(AdxGetCoreTypeIDOffset(), 72U);
    EXPECT_EQ(AdxGetCoreTypeIDOffset(), 72U);
}

TEST_F(DumpPrintfPlatformUtest, Test_GetBlockNum_MultipleCalls)
{
    setenv("ADX_LLT_SOC_VERSION", "Ascend950PR_9599", 1);
    EXPECT_EQ(AdxGetBlockNum(), 108U);
    EXPECT_EQ(AdxGetBlockNum(), 108U);
}