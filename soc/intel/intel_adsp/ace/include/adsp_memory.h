/*
 * Copyright (c) 2024 Intel Corporation
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_SOC_INTEL_ADSP_MEMORY_H_
#define ZEPHYR_SOC_INTEL_ADSP_MEMORY_H_

#if defined(CONFIG_INTEL_ADSP_MEMORY_IS_MIRRORED)
#include "memory/mirrored.h"
#elif defined(CONFIG_INTEL_ADSP_MEMORY_IS_UNIFIED)
#include "memory/unified.h"
#endif

#endif /* ZEPHYR_SOC_INTEL_ADSP_MEMORY_H_ */
