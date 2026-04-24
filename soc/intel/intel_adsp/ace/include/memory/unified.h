/*
 * Copyright (c) 2024, 2026 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_SOC_INTEL_ADSP_MEMORY_UNIFIED_H_
#define ZEPHYR_SOC_INTEL_ADSP_MEMORY_UNIFIED_H_

#include <zephyr/devicetree.h>
#include <zephyr/toolchain.h>
#include <mem_window.h>

#define L2_SRAM_BASE (DT_REG_ADDR(DT_NODELABEL(sram0)))
#define L2_SRAM_SIZE (DT_REG_SIZE(DT_NODELABEL(sram0)))

#define L2_VIRTUAL_SRAM_BASE (DT_REG_ADDR(DT_NODELABEL(sram0virtual)))
#define L2_VIRTUAL_SRAM_SIZE (DT_REG_SIZE(DT_NODELABEL(sram0virtual)))

#define L2_VIRTUAL_SRAM_ALIAS_BASE (DT_REG_ADDR(DT_NODELABEL(sram0virtualalias)))
#define L2_VIRTUAL_SRAM_ALIAS_SIZE (DT_REG_SIZE(DT_NODELABEL(sram0virtualalias)))

#define LP_SRAM_BASE (DT_REG_ADDR(DT_NODELABEL(sram1)))
#define LP_SRAM_SIZE (DT_REG_SIZE(DT_NODELABEL(sram1)))

#define LP_SRAM_ALIAS_BASE (DT_REG_ADDR(DT_NODELABEL(sram1alias)))
#define LP_SRAM_ALIAS_SIZE (DT_REG_SIZE(DT_NODELABEL(sram1alias)))

#define ROM_JUMP_ADDR (LP_SRAM_BASE + 0x10)

#define BOOT_LDR_BASE (L2_SRAM_BASE + CONFIG_HP_SRAM_RESERVE)
#define BOOT_LDR_SIZE (0x1000)

/** Base address of all the vectors defined in SRAM */
#define VECBASE_RESET_PADDR_SRAM (BOOT_LDR_BASE + BOOT_LDR_SIZE)

/** Size of vector table */
#define VECTOR_TBL_SIZE 0x1000

/* Linker-usable RAM region */
#define RAM_BASE (L2_SRAM_BASE + CONFIG_HP_SRAM_RESERVE + BOOT_LDR_SIZE + VECTOR_TBL_SIZE)
#define RAM_SIZE (L2_SRAM_SIZE - CONFIG_HP_SRAM_RESERVE - BOOT_LDR_SIZE - VECTOR_TBL_SIZE)

/* L3 region (IMR), located in host memory */
#define L3_MEM_BASE_ADDR (DT_REG_ADDR(DT_NODELABEL(imr)))
#define L3_MEM_SIZE      (DT_REG_SIZE(DT_NODELABEL(imr)))
#define L3_MEM_PAGE_SIZE (DT_PROP(DT_NODELABEL(imr), block_size))

/* The rimage tool produces two blob addresses we need to find: one is
 * our bootloader code block which starts at its entry point, the
 * other is the "manifest" containing the HP-SRAM data to unpack.
 * There's no ability to change this offset, it's a magic number
 * from rimage we simply need to honor.
 */
#define IMR_BOOT_LDR_MANIFEST_OFFSET (0x62000)
#define IMR_BOOT_LDR_MANIFEST_BASE   (L2_SRAM_BASE + IMR_BOOT_LDR_MANIFEST_OFFSET)

/* Used to specify firmware entry point for ROM to jump in the rimage
 * module BRNGUP.
 */
#define IMR_BOOT_LDR_TEXT_ENTRY_BASE BOOT_LDR_BASE

/** Entry point for BRNGUP module in SOF manifest */
#define SOF_MAN_MOD_BRNGUP_ENTRY IMR_BOOT_LDR_TEXT_ENTRY_BASE

/** Entry point for BASEFW module in SOF manifest */
#define SOF_MAN_MOD_BASEFW_ENTRY RAM_BASE

#define ADSP_L1_CACHE_PREFCTL_VALUE 0x1038

/* L1 init */
#define ADSP_L1CC_ADDR     (DT_REG_ADDR(DT_NODELABEL(l1ccap)))
#define ADSP_CxL1CCAP_ADDR (DT_REG_ADDR(DT_NODELABEL(l1ccap)))
#define ADSP_CxL1CCFG_ADDR (DT_REG_ADDR(DT_NODELABEL(l1ccfg)))
#define ADSP_CxL1PCFG_ADDR (DT_REG_ADDR(DT_NODELABEL(l1pcfg)))

#if (!defined(_ASMLANGUAGE) && !defined(__ASSEMBLER__))

#define ADSP_CxL1CCAP_REG (*(volatile uint32_t *)(ADSP_CxL1CCAP_ADDR))
#define ADSP_CxL1CCFG_REG (*(volatile uint32_t *)(ADSP_CxL1CCFG_ADDR))
#define ADSP_CxL1PCFG_REG (*(volatile uint32_t *)(ADSP_CxL1PCFG_ADDR))

#endif /* (!defined(_ASMLANGUAGE) && !defined(__ASSEMBLER__)) */

/* The number of set associative cache way supported on L1 Data Cache */
#define ADSP_CxL1CCAP_DCMWC ((ADSP_CxL1CCAP_REG >> 16) & 7)
/* The number of set associative cache way supported on L1 Instruction Cache */
#define ADSP_CxL1CCAP_ICMWC ((ADSP_CxL1CCAP_REG >> 20) & 7)

#ifndef _LINKER
/* L2 Local Memory Management */

/* These registers are for the L2 memory control and status. */
#define DFL2MM_REG (DT_REG_ADDR(DT_NODELABEL(hsbcap)))

struct ace_l2mm {
	uint32_t l2mcap;
	uint32_t l2mpat;
	uint32_t l2mecap;
	uint32_t l2mecs;
	uint32_t l2hsbpmptr;
	uint32_t l2usbpmptr;
	uint32_t l2usbmrpptr;
	uint32_t l2ucmrpptr;
	uint32_t l2ucmrpdptr;
};

#define ACE_L2MM ((volatile struct ace_l2mm *)DFL2MM_REG)

/* DfL2MCAP */
struct ace_l2mcap {
	uint32_t l2hss: 8;
	uint32_t l2uss: 4;
	uint32_t l2hsbs: 4;
	uint32_t l2hs2s: 8;
	uint32_t l2usbs: 5;
	uint32_t l2se: 1;
	uint32_t el2se: 1;
	uint32_t rsvd32: 1;
};

#define ACE_L2MCAP ((volatile struct ace_l2mcap *)DFL2MM_REG)

static ALWAYS_INLINE uint32_t ace_hpsram_get_bank_count(void)
{
	return ACE_L2MCAP->l2hss;
}

static ALWAYS_INLINE uint32_t ace_lpsram_get_bank_count(void)
{
	return ACE_L2MCAP->l2uss;
}

struct ace_hpsram_regs {
	/** @brief power gating control */
	uint8_t HSxPGCTL;
	/** @brief retention mode control */
	uint8_t HSxRMCTL;
	uint8_t reserved[2];
	/** @brief power gating status */
	uint8_t HSxPGISTS;
	uint8_t reserved1[3];
};

struct ace_lpsram_regs {
	/** @brief power gating control */
	uint8_t USxPGCTL;
	/** @brief retention mode control */
	uint8_t USxRMCTL;
	uint8_t reserved[2];
	/** @brief power gating status */
	uint8_t USxPGISTS;
	uint8_t reserved1[3];
};

/* IMR memory info */
#define HFIMRIA1_REG (DT_REG_ADDR(DT_NODELABEL(imria1)))
#define HFIMRIS1_REG (DT_REG_ADDR(DT_NODELABEL(imria1)) + 0x8)

/* IMR Info Address 1 */
struct ace_hfimria1 {
	/** @brief Physical base address of the IMR memory region */
	uint64_t addr;
};

/* IMR Info Size 1 */
struct ace_hfimris1 {
	/** @brief IMR used */
	uint64_t iu: 1;
	/** @brief Root Space */
	uint64_t rs: 3;
	uint64_t rsvd31: 28;
	/** @brief IMR size in KB */
	uint64_t sz: 32;
};

#define ACE_HFIMRA1 ((volatile struct ace_hfimria1 *)HFIMRIA1_REG)
#define ACE_HFIMRS1 ((volatile struct ace_hfimris1 *)HFIMRIS1_REG)

static ALWAYS_INLINE uint32_t ace_imr_used(void)
{
	return ACE_HFIMRS1->iu;
}

static ALWAYS_INLINE uint32_t ace_imr_get_mem_size(void)
{
	return ACE_HFIMRS1->sz * 1024;
}
#endif

/* These registers are for the L2 HP SRAM bank power management control and status.*/
#define L2_HSBPM_BASE (DT_REG_ADDR(DT_NODELABEL(hsbpm)))
#define L2_HSBPM_SIZE (DT_REG_SIZE(DT_NODELABEL(hsbpm)))

#define HPSRAM_REGS(block_idx)                                                                     \
	((volatile struct ace_hpsram_regs *const)(L2_HSBPM_BASE + L2_HSBPM_SIZE * (block_idx)))

/* These registers are for the L2 LP SRAM bank power management control and status.*/
#define L2_LSBPM_BASE (DT_REG_ADDR(DT_NODELABEL(lsbpm)))
#define L2_LSBPM_SIZE (DT_REG_SIZE(DT_NODELABEL(lsbpm)))

#define LPSRAM_REGS(block_idx)                                                                     \
	((volatile struct ace_lpsram_regs *const)(L2_LSBPM_BASE + L2_LSBPM_SIZE * (block_idx)))

#endif /* ZEPHYR_SOC_INTEL_ADSP_MEMORY_UNIFIED_H_ */
