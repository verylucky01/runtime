/** 
  * Copyright (c) 2025 Huawei Technologies Co., Ltd. 
  * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
  * CANN Open Software License Agreement Version 2.0 (the "License"). 
  * Please refer to the License for details. You may not use this file except in compliance with the License. 
  * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
  * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 
  * See LICENSE in the root of the software repository for the full text of the License. 
  */ 
 #include <thread> 
 #include "gtest/gtest.h" 
 #include "mockcpp/mockcpp.hpp" 
 
 
 #define private public 
 #define protected public 
 #include "inc/package_worker.h" 
 #undef private 
 #undef protected 
 #include "inc/package_worker_utils.h" 
 
 
 
 
 using namespace tsd; 
 
 
 class PackageWorkerTest : public testing::Test { 
 protected: 
     virtual void SetUp() {} 
 
 
     virtual void TearDown() 
     { 
         GlobalMockObject::verify(); 
     } 
 }; 
 
 
 TEST_F(PackageWorkerTest, StopTest) 
 { 
     std::shared_ptr<PackageWorker> packageWorker = PackageWorker::GetInstance(0, 0); 
     packageWorker->DestroyPackageWorker(); 
     EXPECT_EQ(PackageWorker::workerManager_.size(), 0); 
 } 
 
 TEST_F(PackageWorkerTest, LoadPackageNull) 
 { 
     std::shared_ptr<PackageWorker> packageWorker = PackageWorker::GetInstance(0, 0); 
     const auto ret = packageWorker->LoadPackage(PackageWorkerType::PACKAGE_WORKER_MAX, "", ""); 
     EXPECT_EQ(ret, TSD_INSTANCE_NOT_FOUND); 
 } 

  TEST_F(PackageWorkerTest, UnloadPackageSucc) 
 { 
    PackageWorkerParas paras;
    PackageWorkerFactory &inst = PackageWorkerFactory().GetInstance();
    std::shared_ptr<PackageWorker> packageWorker = PackageWorker::GetInstance(0, 0);
    auto reworker = inst.CreatePackageWorker(PackageWorkerType::PACKAGE_WORKER_AICPU_THREAD, paras);
    MOCKER_CPP(&PackageWorker::GetPackageWorker).stubs().will(returnValue(reworker));
    const auto ret = packageWorker->UnloadPackage(PackageWorkerType::PACKAGE_WORKER_AICPU_THREAD); 
    EXPECT_EQ(ret, TSD_OK); 
 }
