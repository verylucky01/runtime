# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

include(${ADUMP_CMAKE_DIR}/adump_base/adump_server_base.cmake)

################### libadump_server.a begin ############################
set(adumpServerHeaderList
    ${adumpServerBaseHeaderList}
)

set(adumpServerSrcList
    ${adumpServerBaseSrcList}
)

add_library(adump_server STATIC
    ${adumpServerSrcList}
)

add_dependencies(adump_server adumpHostProto)

target_include_directories(adump_server PRIVATE
    ${adumpServerHeaderList}
)

target_compile_options(adump_server PRIVATE
    ${adumpServerBaseCompileOptions}
)

target_compile_definitions(adump_server PRIVATE
    ${adumpServerBaseCompileDefinitions}
)

target_link_libraries(adump_server
    ${adumpServerBaseLinkLibraries}
)
################### libadump_server.a end ############################

################ libadump_server_stub.a begin ########################
set(adumpServerStubSrcList
    ${ADUMP_ADUMP_DIR}/host/adx_datadump_server_stub.cpp
)

set(adumpServerStubHeaderList
    ${adumpServerStubHeaderList}
)

add_library(adump_server_stub STATIC
    ${adumpServerStubSrcList}
)

target_include_directories(adump_server_stub PRIVATE
    ${adumpServerStubHeaderList}
)

target_compile_options(adump_server_stub PRIVATE
    ${adumpServerStubCompileOptions}
)

target_compile_definitions(adump_server_stub PRIVATE
    ${adumpServerStubCompileDefinitions}
)

target_link_libraries(adump_server_stub PRIVATE
    $<BUILD_INTERFACE:intf_pub>
)
################### libadump_server_stub.a end ######################