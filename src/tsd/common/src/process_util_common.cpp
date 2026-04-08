/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "inc/process_util_common.h"
#include <fstream>
#include <vector>
#ifdef TSD_HOST_LIB
#include <openssl/sha.h>
#endif
#include <fstream>
#include <sstream>
#include <iomanip>

namespace {
    constexpr const int32_t HX_PRINT_POS = 2;
}
namespace tsd {
std::string ProcessUtilCommon::CalFileSha256HashValue(const std::string &filePath)
{
    std::ifstream curFile(filePath, std::ios::binary);
    if (!curFile) {
        TSD_RUN_WARN("open file:%s not success, reason:%s", filePath.c_str(), SafeStrerror().c_str());
        return "";
    }

    std::stringstream fileBuffer;
    fileBuffer << curFile.rdbuf();
    std::string fileBinaryValue = fileBuffer.str();
    unsigned char hashValue[SHA256_DIGEST_LENGTH];
#ifndef tsd_UT
    SHA256(PtrToPtr<const char, const unsigned char>(fileBinaryValue.c_str()), fileBinaryValue.size(), hashValue);
#endif
    std::stringstream sha256Str;
    sha256Str << std::hex << std::setfill('0');
    for (auto i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sha256Str << std::setw(HX_PRINT_POS) << static_cast<int32_t>(hashValue[i]);
    }
    curFile.close();
    return sha256Str.str();
}
}  // namespace tsd