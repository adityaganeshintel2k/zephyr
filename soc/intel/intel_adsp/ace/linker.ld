/* Copyright (c) 2022 Intel Corporation
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/linker/devicetree_regions.h>

#if defined(CONFIG_INTEL_ADSP_MEMORY_IS_MIRRORED)
#include <linker/ace-link-mirrored.ld>
#elif defined(CONFIG_INTEL_ADSP_MEMORY_IS_UNIFIED)
#include <linker/ace-link-unified.ld>
#endif

MEMORY
{
    LINKER_DT_REGIONS()
}

SECTIONS
{
    LINKER_DT_SECTIONS()
}
