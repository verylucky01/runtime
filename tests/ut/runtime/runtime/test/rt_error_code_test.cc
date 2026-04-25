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
#include "runtime/rt.h"
#include "error_code.h"
#include "errcode_manage.hpp"
#include "runtime.hpp"
#include "rt_log.h"
using namespace testing;
using namespace cce::runtime;
class RtErrorCodeTest : public Test {
protected:
    static void SetUpTestCase()
    {}

    static void TearDownTestCase()
    {}

    virtual void SetUp()
    {}

    virtual void TearDown()
    {}
};

TEST_F(RtErrorCodeTest, GetTsErrModuleType)
{
    EXPECT_EQ(GetTsErrModuleType(TS_SUCCESS), ERR_MODULE_RTS);
}

TEST_F(RtErrorCodeTest, GetTsErrModuleTypeNotFind)
{
    EXPECT_EQ(GetTsErrModuleType(TS_ERROR_RESERVED + 1), ERR_MODULE_RTS);
}

TEST_F(RtErrorCodeTest, GetTsErrCodeDescNotFind)
{
    RecordErrorLog(__FILE__, __LINE__, __FUNCTION__,  "%s", "unknown error");
    RecordLog(DLOG_DEBUG, __FILE__, __LINE__, __FUNCTION__,  "%s", "unknown error");
    EXPECT_EQ(strcmp(GetTsErrCodeDesc(TS_ERROR_RESERVED + 1), "unknown error"), 0);
}

TEST_F(RtErrorCodeTest, GetTsErrDescByRtErr)
{
    RecordErrorLog(__FILE__, __LINE__, __FUNCTION__, nullptr);
    RecordLog(DLOG_DEBUG, __FILE__, __LINE__, __FUNCTION__,  nullptr);
    EXPECT_EQ(strcmp(GetTsErrDescByRtErr(RT_ERROR_NONE), "success"), 0);
}

TEST_F(RtErrorCodeTest, PrintErrMsgToLog)
{
    std::vector<std::string> values1001 = {"The argument is invalid"};
    PrintErrMsgToLog(ErrorCode::EE1001, "file", 1000, "func", values1001);
 
    std::vector<std::string> values1002 = {"Stream synchronize timeout"};
    PrintErrMsgToLog(ErrorCode::EE1002, "file", 1000, "func", values1002);
 
    std::vector<std::string> values1 = {"rtMemCpy", "0", "size", "[0, 255]"};
    PrintErrMsgToLog(ErrorCode::EE1003, "file", 1000, "func", values1);
 
    std::vector<std::string> values2 = {"rtMemCpy", "src"};
    PrintErrMsgToLog(ErrorCode::EE1004, "file", 1000, "func", values2);
 
    std::vector<std::string> values3 = {"rtMemCpy"};
    PrintErrMsgToLog(ErrorCode::EE1005, "file", 1000, "func", values3);

    std::vector<std::string> values101 = {"set the saturation mode", "only the Inf/NaN mode can be set and the saturation mode"};
    PrintErrMsgToLog(ErrorCode::WE0001, "file", 1000, "func", values101);
 
    std::vector<std::string> values4 = {"rtMemCpy", "d2d"};
    PrintErrMsgToLog(ErrorCode::EE1006, "file", 1000, "func", values4);
 
    std::vector<std::string> values5 = {"10", "repeat bind"};
    PrintErrMsgToLog(ErrorCode::EE1007, "file", 1000, "func", values5);
 
    std::vector<std::string> values6 = {"section invalid"};
    PrintErrMsgToLog(ErrorCode::EE1008, "file", 1000, "func", values6);
 
    std::vector<std::string> values7 = {"10", "model invalid"};
    PrintErrMsgToLog(ErrorCode::EE1009, "file", 1000, "func", values7);
 
    std::vector<std::string> values8 = {"rtModelExecute", "stream"};
    PrintErrMsgToLog(ErrorCode::EE1010, "file", 1000, "func", values8);
 
    std::vector<std::string> values1011 = {"rtMemCpy", "0", "size", "size is not 0"};
    PrintErrMsgToLog(ErrorCode::EE1011, "file", 1000, "func", values1011);
    
    std::vector<std::string> values1012 = {"NotifyWait", "0", "current deviceId", "The current device cannot deliver Notify Wait"};
    PrintErrMsgToLog(ErrorCode::EE1012, "file", 1000, "func", values1012);

    std::vector<std::string> values1013 = {"100"};
    PrintErrMsgToLog(ErrorCode::EE1013, "file", 1000, "func", values1013);

    std::vector<std::string> values1014 = {"The ELF section header address in the operator binary ELF file header cannot be empty"};
    PrintErrMsgToLog(ErrorCode::EE1014, "file", 1000, "func", values1014);

    std::vector<std::string> values1015 = {"rtsIpcMemImportByKey", "The driver interface halShmemInfoGet does not exist."};
    PrintErrMsgToLog(ErrorCode::EE1015, "file", 1000, "func", values1015);

    std::vector<std::string> values1016 = {"MemCopySync", "Other threads of the current context are in the capture state"};
    PrintErrMsgToLog(ErrorCode::EE1016, "file", 1000, "func", values1016);

    std::vector<std::string> values1017 = {"rtMemCpy", "size", "size is not 0"};
    PrintErrMsgToLog(ErrorCode::EE1017, "file", 1000, "func", values1017);

    std::vector<std::string> values1018 = {"aclrtSetLabel", "Before setting the label using aclrtSetLabel, you need to call aclrtCreateLabelList to create a label list"};
    PrintErrMsgToLog(ErrorCode::EE1018, "file", 1000, "func", values1018);

    std::vector<std::string> values1019 = {"AddTaskToList", "stream task public buffer is full"};
    PrintErrMsgToLog(ErrorCode::EE1019, "file", 1000, "func", values1019);

    std::vector<std::string> values1020 = {"rtGetSocVersion", "memcpy_s", "1", "count is greater than dest_max",
        "src=0x1, dest=0x2, dest_max=10, count=11."};
    PrintErrMsgToLog(ErrorCode::EE1020, "file", 1000, "func", values1020);

    std::vector<std::string> values9 = {"1, 2, 2", "SetVisible", "not repeat"};
    PrintErrMsgToLog(ErrorCode::EE2002, "file", 1000, "func", values9);

    PrintErrMsgToLog(ErrorCode::EE_NO_ERROR, "file", 1000, "func", values9);
    PrintErrMsgToLog(ErrorCode::EE_NO_ERROR, "file", 1000, "func", {});
    ProcessErrorCodeImpl(ErrorCode::EE_NO_ERROR,  "file", 1000, "func", std::move(values9));
}

TEST_F(RtErrorCodeTest, RePortErrCode)
{
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1001, "The argument is invalid");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1002, "Stream synchronize timeout");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1003, "rtMemCpy", "0", "size", "[0, 255]");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1004, "rtMemCpy", "src");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1005, "rtMemCpy");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1006, "rtMemCpy", "d2d");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1007, "10", "repeat bind");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1008, "malloc", "section invalid");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1009, "10", "model invalid");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1010, "rtModelExecute", "stream");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1011, "rtMemCpy", "0", "size", "size is not 0");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1012, "NotifyWait", "0", "current deviceId", "The current device cannot deliver Notify Wait");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1013, "100");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1014, "The ELF section header address in the operator binary ELF file header cannot be empty");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1015, "rtsIpcMemImportByKey", "The driver interface halShmemInfoGet does not exist.");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1016, "MemCopySync", "Other threads of the current context are in the capture state");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1017, "rtMemCpy", "size", "size is not 0");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1018, "aclrtSetLabel", "Before setting the label using aclrtSetLabel, you need to call aclrtCreateLabelList to create a label list");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1019, "AddTaskToList", "stream task public buffer is full");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE1020, "rtGetSocVersion", "memcpy_s", "1", "count is greater than dest_max", "src=0x1, dest=0x2, dest_max=10, count=11.");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::EE2002, "1, 2, 2", "SetVisible", "not repeat");
    RT_LOG_OUTER_MSG_IMPL(ErrorCode::WE0001, "set the saturation mode", "only the Inf/NaN mode can be set and the saturation mode");
}

TEST_F(RtErrorCodeTest, CheckErrCodeParams)
{
    auto names = GetParamNames(ErrorCode::EE1001);
    EXPECT_EQ(names, (std::vector<std::string>{"extend_info"}));
    names = GetParamNames(ErrorCode::EE1002);
    EXPECT_EQ(names, (std::vector<std::string>{"extend_info"}));
    names = GetParamNames(ErrorCode::EE1003);
    EXPECT_EQ(names, (std::vector<std::string>{"func", "value", "param", "expect"}));
    names = GetParamNames(ErrorCode::EE1004);
    EXPECT_EQ(names, (std::vector<std::string>{"func", "param"}));
    names = GetParamNames(ErrorCode::EE1005);
    EXPECT_EQ(names, (std::vector<std::string>{"func"}));
    names = GetParamNames(ErrorCode::EE1006);
    EXPECT_EQ(names, (std::vector<std::string>{"func", "type"}));
    names = GetParamNames(ErrorCode::EE1007);
    EXPECT_EQ(names, (std::vector<std::string>{"id", "reason"}));
    names = GetParamNames(ErrorCode::EE1008);
    EXPECT_EQ(names, (std::vector<std::string>{"reason"}));
    names = GetParamNames(ErrorCode::EE1009);
    EXPECT_EQ(names, (std::vector<std::string>{"id", "reason"}));
    names = GetParamNames(ErrorCode::EE1010);
    EXPECT_EQ(names, (std::vector<std::string>{"func", "object"}));
    names = GetParamNames(ErrorCode::EE1011);
    EXPECT_EQ(names, (std::vector<std::string>{"func", "value", "param", "reason"}));
    names = GetParamNames(ErrorCode::EE1012);
    EXPECT_EQ(names, (std::vector<std::string>{"func", "value", "param", "reason"}));
    names = GetParamNames(ErrorCode::EE1013);
    EXPECT_EQ(names, (std::vector<std::string>{"buf_size"}));
    names = GetParamNames(ErrorCode::EE1014);
    EXPECT_EQ(names, (std::vector<std::string>{"reason"}));
    names = GetParamNames(ErrorCode::EE1015);
    EXPECT_EQ(names, (std::vector<std::string>{"func", "reason"}));
    names = GetParamNames(ErrorCode::EE1016);
    EXPECT_EQ(names, (std::vector<std::string>{"func", "reason"}));
    names = GetParamNames(ErrorCode::EE1017);
    EXPECT_EQ(names, (std::vector<std::string>{"func", "param", "reason"}));
    names = GetParamNames(ErrorCode::EE1018);
    EXPECT_EQ(names, (std::vector<std::string>{"func", "reason"}));
    names = GetParamNames(ErrorCode::EE1019);
    EXPECT_EQ(names, (std::vector<std::string>{"func", "reason"}));
    names = GetParamNames(ErrorCode::EE1020);
    EXPECT_EQ(names, (std::vector<std::string>{"func1", "func2", "ret_code", "reason", "extend_info"}));
    names = GetParamNames(ErrorCode::EE2002);
    EXPECT_EQ(names, (std::vector<std::string>{"value", "env", "expect"}));
    names = GetParamNames(ErrorCode::EE_NO_ERROR);
    EXPECT_EQ(names, (std::vector<std::string>{}));
    names = GetParamNames(ErrorCode::WE0001);
    EXPECT_EQ(names, (std::vector<std::string>{"function", "type"}));
    names = GetParamNames(ErrorCode::EE1012);
    EXPECT_EQ(names, (std::vector<std::string>{"func", "value", "param", "reason"}));
}