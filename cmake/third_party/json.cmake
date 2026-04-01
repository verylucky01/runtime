# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
if (ENABLE_OPEN_SRC)

    include(ExternalProject)

    set(JSON_SRC_DIR ${CANN_3RD_LIB_PATH}/json) 
    if (NOT EXISTS "${JSON_SRC_DIR}")
        set(JSON_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/json")
        set(JSON_INCLUDE "${CMAKE_BINARY_DIR}/json/include")
        set(JSON_SOURCE "${CMAKE_BINARY_DIR}/json/include/nlohmann")
        if (EXISTS "${CANN_3RD_LIB_PATH}/include.zip")
            message("json use local include.zip")
            set(REQ_URL "${CANN_3RD_LIB_PATH}/include.zip")
        else()
            message("json not use cache.")
            set(REQ_URL "https://gitcode.com/cann-src-third-party/json/releases/download/v3.11.3/include.zip")
        endif()
        include(ExternalProject)
        ExternalProject_Add(third_party_json
            URL ${REQ_URL}
            TLS_VERIFY OFF
            DOWNLOAD_DIR ${JSON_DOWNLOAD_DIR}
            SOURCE_DIR ${JSON_DOWNLOAD_DIR}
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND ""
            UPDATE_COMMAND ""
        )
    else()
        message("json use cache.")
        set(JSON_INCLUDE "${CANN_3RD_LIB_PATH}/json/include")
        set(JSON_SOURCE "${CANN_3RD_LIB_PATH}/json/include/nlohmann")
    endif()

    add_library(json INTERFACE)
    add_dependencies(json third_party_json)
    target_include_directories(json INTERFACE ${JSON_INCLUDE})
    target_compile_definitions(json INTERFACE
        nlohmann=ascend_nlohmann  # 如果需要命名空间重映射
    )
endif()