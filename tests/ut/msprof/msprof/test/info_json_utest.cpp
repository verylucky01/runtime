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
#include "info_json.h"
#include <cstdio>
#include <fstream>
#include "ai_drv_dev_api.h"
#include "config/config.h"
#include "config_manager.h"
#include "errno/error_code.h"
#include "msprof_dlog.h"
#include "prof_manager.h"
#include "securec.h"
#include "utils/utils.h"
#include "platform/platform.h"
#include "task_relationship_mgr.h"
#include "json/json.h"

using namespace analysis::dvvp::common::error;
using namespace analysis::dvvp::common::utils;
using namespace analysis::dvvp::common::config;
using namespace Analysis::Dvvp::Common::Platform;
using namespace Analysis::Dvvp::Common::Config;
using namespace analysis::dvvp::host;
using namespace Dvvp::Collect::Platform;

class INFO_JSON_TEST: public testing::Test {
protected:
    virtual void SetUp() {
    }
    virtual void TearDown() {
    }
};

TEST(INFO_JSON_TEST, GetHwtsFreq) {
    GlobalMockObject::verify();
    MOCKER_CPP(&Analysis::Dvvp::Common::Config::ConfigManager::GetPlatformType)
        .stubs()
        .will(returnValue(PlatformType::CHIP_CLOUD_V3));
    InfoJson infoJson("1", "0", 1);
    std::string freq = "1005";
    EXPECT_EQ("1000", infoJson.GetHwtsFreq(freq));
    freq = "1000.1";
    EXPECT_EQ("1000.1", infoJson.GetHwtsFreq(freq));
}
