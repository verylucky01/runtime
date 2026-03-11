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

#include "inc/tsd_path_mgr.h"

using namespace tsd;
using namespace std;

class PathMgrTest : public testing::Test {
protected:
  virtual void SetUp()
  {
      cout << "Before PathMgrTest()" << endl;
  }

  virtual void TearDown()
  {
      cout << "After PathMgrTest" << endl;
      GlobalMockObject::verify();
  }
};

TEST_F(PathMgrTest, BuildCustAicpuRootPath_Test)
{
  EXPECT_EQ(TsdPathMgr::BuildCustAicpuRootPath("0"), "/home/CustAiCpuUser");
  EXPECT_EQ(TsdPathMgr::BuildCustAicpuRootPath("1"), "/home/CustAiCpuUser1");
}