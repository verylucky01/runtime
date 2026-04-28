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
#include <csignal>
#include <atomic>
#include <unistd.h>
#include <mockcpp/mockcpp.hpp>
#include "prof_acl_mgr.h"

using namespace Msprofiler::Api;
using ProfSignalHandler = void (*)(int);

class SigintHandlerUtest : public testing::Test {
protected:
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

static void CustomerSigHandler(int signum)
{
    (void)signum;
}

static std::atomic<bool> g_customSigHandlerCalled{false};
static void CustomerSigHandlerWithFlag(int /*signum*/)
{
    g_customSigHandlerCalled = true;
}

TEST_F(SigintHandlerUtest, RegisterSignalHandlerTwice)
{
    signal(SIGINT, CustomerSigHandler);

    auto *mgr = ProfAclMgr::instance();
    mgr->isReady_ = true;

    mgr->Init();
    ProfSignalHandler firstInstalled = signal(SIGINT, SIG_IGN);
    EXPECT_NE(firstInstalled, CustomerSigHandler);
    EXPECT_NE(firstInstalled, SIG_DFL);
    signal(SIGINT, firstInstalled);

    mgr->Init();
    ProfSignalHandler secondInstalled = signal(SIGINT, SIG_IGN);
    EXPECT_EQ(firstInstalled, secondInstalled);
    signal(SIGINT, secondInstalled);

    mgr->UnInit();
    ProfSignalHandler restored = signal(SIGINT, CustomerSigHandler);
    EXPECT_EQ(restored, CustomerSigHandler);
}

TEST_F(SigintHandlerUtest, SigintWatcherThreadCallsMsprofFinalize)
{
    g_customSigHandlerCalled = false;
    signal(SIGINT, CustomerSigHandlerWithFlag);

    MOCKER_CPP(&ProfAclMgr::MsprofFinalizeHandle)
        .stubs()
        .will(returnValue(0));

    auto *mgr = ProfAclMgr::instance();
    mgr->isReady_ = true;
    mgr->Init();

    raise(SIGINT);

    int waitMs = 0;
    while (!g_customSigHandlerCalled && waitMs < 200) {
        usleep(1000);
        waitMs++;
    }
    EXPECT_TRUE(g_customSigHandlerCalled) << "CustomerSigHandlerWithFlag was not called within 200ms";

    mgr->UnInit();
    signal(SIGINT, SIG_DFL);
}

TEST_F(SigintHandlerUtest, UnregisterSigalHandlerRestoresOldHandler)
{
    signal(SIGINT, CustomerSigHandler);

    auto *mgr = ProfAclMgr::instance();
    mgr->isReady_ = true;
    mgr->Init();
    mgr->UnInit();

    ProfSignalHandler restored = signal(SIGINT, CustomerSigHandler);
    EXPECT_EQ(restored, CustomerSigHandler) << "SIGINT handler should be restored to CustomerSigHandler";
}

TEST_F(SigintHandlerUtest, SigintWatcherThreadExitsOnUnregister)
{
    signal(SIGINT, CustomerSigHandler);

    auto *mgr = ProfAclMgr::instance();
    mgr->isReady_ = true;
    mgr->Init();
    mgr->UnInit();

    SUCCEED();
    signal(SIGINT, SIG_DFL);
}

TEST_F(SigintHandlerUtest, UnregisterSignalHandlerRestoresSIGIGN)
{
    // 验证：当初始信号处理器是 SIG_IGN 时，
    // 注册 ProfAclMgr 信号处理器后，注销时能正确恢复为 SIG_IGN

    signal(SIGINT, SIG_IGN);

    auto *mgr = ProfAclMgr::instance();
    mgr->isReady_ = true;
    mgr->Init();

    mgr->UnInit();

    ProfSignalHandler restored = signal(SIGINT, SIG_IGN);
    EXPECT_EQ(restored, SIG_IGN) << "SIGINT handler should be restored to SIG_IGN";
}

TEST_F(SigintHandlerUtest, UnregisterSignalHandlerRestoresSIGDFL)
{
    // 验证：当初始信号处理器是 SIG_DFL 时，
    // 注册 ProfAclMgr 信号处理器后，注销时能正确恢复为 SIG_DFL

    signal(SIGINT, SIG_DFL);

    auto *mgr = ProfAclMgr::instance();
    mgr->isReady_ = true;
    mgr->Init();

    mgr->UnInit();

    ProfSignalHandler restored = signal(SIGINT, SIG_DFL);
    EXPECT_EQ(restored, SIG_DFL) << "SIGINT handler should be restored to SIG_DFL";
}
