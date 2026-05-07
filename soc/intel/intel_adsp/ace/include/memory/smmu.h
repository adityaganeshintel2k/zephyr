/*
 * Copyright (c) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_SOC_INTEL_ADSP_MEMORY_SMMU_H_
#define ZEPHYR_SOC_INTEL_ADSP_MEMORY_SMMU_H_

#include <zephyr/devicetree.h>

/** Base address of SMMU registers. */
#define SMMU_BASE DT_REG_ADDR(DT_NODELABEL(smmu))

/** SMMU Page Size. Hardware defined. */
#define SMMU_PAGE_SIZE DT_PROP(DT_NODELABEL(smmu), page_size)

/**
 * Map memory at boot.
 *
 * This maps local physical memory to virtual memory space
 * for pages that haven't been mapped by ROM.
 */
void smmu_boot_mem_map(void);

#endif /* ZEPHYR_SOC_INTEL_ADSP_MEMORY_SMMU_H_ */
