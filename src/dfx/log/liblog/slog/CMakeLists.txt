# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

add_library(slog_headers INTERFACE)
target_include_directories(slog_headers INTERFACE
    $<BUILD_INTERFACE:${TOOLAHCIN_LOG_DIR}/inc>
    $<BUILD_INTERFACE:${TOOLAHCIN_LOG_DIR}/inc/toolchain>
    $<BUILD_INTERFACE:${TOOLAHCIN_LOG_DIR}/../../../pkg_inc>
    $<BUILD_INTERFACE:${TOOLAHCIN_LOG_DIR}/../../../pkg_inc/base>
    $<INSTALL_INTERFACE:include>
    $<INSTALL_INTERFACE:include/slog>
    $<INSTALL_INTERFACE:include/slog/toolchain>
)


message("Variable value1: ${PRODUCT_SIDE}")
message("Variable value2: ${PRODUCT}")

if (${PRODUCT_SIDE} STREQUAL "device")
    set(liblog_type "device")
if (PRODUCT)
if (${PRODUCT} STREQUAL "pegasus")
    set(liblog_type "mdc")
endif()
endif()

if (${liblog_type} STREQUAL "mdc")
############################# [mdc] #########################################
message("Variable value a")
add_subdirectory(plog_device_mdc)
else ()
############################# [device] #######################################
message("Variable value b")
add_subdirectory(plog_device)
endif ()
############################# [ host ] ########################################
elseif (${PRODUCT_SIDE} STREQUAL "host")
message("Variable value c")
add_subdirectory(plog)
add_subdirectory(plog_host)
endif ()

message("Variable value3: ${liblog_type}")
