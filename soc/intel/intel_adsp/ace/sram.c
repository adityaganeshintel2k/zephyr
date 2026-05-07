/* Copyright (c) 2021 Intel Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdint.h>

#include <zephyr/devicetree.h>
#include <soc_util.h>
#include <zephyr/cache.h>
#include <adsp_shim.h>
#include <adsp_memory.h>
#include <cpu_init.h>

#ifdef CONFIG_INTEL_ADSP_SOC_HAS_SMMU
#include <memory/smmu.h>
#endif /* CONFIG_INTEL_ADSP_SOC_HAS_SMMU */

__imr void hp_sram_init(uint32_t memory_size)
{
	ARG_UNUSED(memory_size);

	uint32_t hpsram_ebb_quantity = ace_hpsram_get_bank_count();
	uint32_t idx;

	for (idx = 0; idx < hpsram_ebb_quantity; ++idx) {
		HPSRAM_REGS(idx)->HSxPGCTL = 0;
		HPSRAM_REGS(idx)->HSxRMCTL = IS_ENABLED(CONFIG_SRAM_RETENTION_MODE);
	}
	for (idx = 0; idx < hpsram_ebb_quantity; ++idx) {
		while (HPSRAM_REGS(idx)->HSxPGISTS != 0) {
		}
	}

#ifdef CONFIG_INTEL_ADSP_SOC_HAS_SMMU
	/* If there is SMMU, the HPSRAM address space contains pages mapped to IMR
	 * where the ROM copies the firmware. In addition, ROM does not map the whole
	 * local SRAM space. Therefore, we cannot blindly zero out memory which
	 * would wipe everything. So we need to pull in those IMR mapped pages into
	 * local HPSRAM, and also maps the remaining memory into local HPSRAM.
	 */
	smmu_boot_mem_map();
#else  /* CONFIG_INTEL_ADSP_SOC_HAS_SMMU */
	bbzero((void *)L2_SRAM_BASE, L2_SRAM_SIZE);
#endif /* CONFIG_INTEL_ADSP_SOC_HAS_SMMU */
}

__imr void lp_sram_init(void)
{
	uint32_t lpsram_ebb_quantity = ace_lpsram_get_bank_count();
	uint32_t idx;

	for (idx = 0; idx < lpsram_ebb_quantity; ++idx) {
		LPSRAM_REGS(idx)->USxPGCTL = 0;
		LPSRAM_REGS(idx)->USxRMCTL = IS_ENABLED(CONFIG_SRAM_RETENTION_MODE);
	}
	for (idx = 0; idx < lpsram_ebb_quantity; ++idx) {
		while (LPSRAM_REGS(idx)->USxPGISTS != 0) {
		}
	}

	bbzero((void *)LP_SRAM_BASE, LP_SRAM_SIZE);
}
