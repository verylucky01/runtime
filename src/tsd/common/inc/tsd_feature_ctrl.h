/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef COMMON_TSD_FEATURE_CTRL_H
#define COMMON_TSD_FEATURE_CTRL_H

#include <cstdint>
#include "inc/log.h"
#include "inc/internal_api.h"
namespace tsd {
// min number of vDeviceId
constexpr const uint32_t VDEVICE_MIN_CPU_NUM = 32U;
// max number of vDeviceId
constexpr const uint32_t VDEVICE_MAX_CPU_NUM = 64U;
const std::string AOSCORE_PREFIX = "sea_";
const std::string AOSCORE_DIR_PREFIX = "/usr/utils/rootfs";

class FeatureCtrl {
public:
    static inline bool IsAosCore()
    {
#ifdef _AOSCORE_
        return true;
#else
        return false;
#endif
    }

    static inline bool IsVfMode(const uint32_t deviceId, const uint32_t vfId)
    {
        if ((IsVfModeCheckedByDeviceId(deviceId)) || (vfId > 0)) {
            return true;
        } else {
            return false;
        }
    }

    static inline bool IsVfModeCheckedByDeviceId(const uint32_t deviceId)
    {
        if ((deviceId >= VDEVICE_MIN_CPU_NUM) && (deviceId < VDEVICE_MAX_CPU_NUM)) {
            return true;
        } else {
            return false;
        }
    }

    static inline bool IsTinyRuntime()
    {
#ifdef TINY_RUNTIME
        return true;
#else
        return false;
#endif
    }

    static inline bool IsHeterogeneousProduct()
    {
#ifdef TSD_HELPER
        return true;
#else
        return false;
#endif
    }

private:
    FeatureCtrl() = default;
    ~FeatureCtrl() = default;

    FeatureCtrl(FeatureCtrl const&) = delete;
    FeatureCtrl& operator=(FeatureCtrl const&) = delete;
    FeatureCtrl(FeatureCtrl&&) = delete;
    FeatureCtrl& operator=(FeatureCtrl&&) = delete;
};

} // namespace tsd

#endif // COMMON_TSD_FEATURE_CTRL_H
