/*
 * Copyright (c) 2026 Intel Corporation. All rights reserved.
 *
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

#include <memory/smmu.h>

/**
 * Address of SMMU_PTU_ADDR_CTRL register.
 *
 * Page Table Update Address and Control Register.
 * Used to specify which PTE to read/write.
 */
#define SMMU_PTU_ADDR_CTRL (SMMU_BASE + 0x10000 + 0x4)

/**
 * SMMU_PTU_ADDR_CTRL GO bit.
 *
 * Set to 1 to start hardware operation.
 * Cleared by hardware after operation is done.
 */
#define SMMU_PTU_ADDR_CTRL_GO BIT(0)

/**
 * SMMU_PTU_ADDR_CTRL CMD bit.
 *
 * 0 to read from PTE into SMMU_PTU_RD_DATA register.
 * 1 to write SMMU_PTU_WR_DATA register into PTE.
 */
#define SMMU_PTU_ADDR_CTRL_CMD BIT(1)

/**
 * SMMU_PTU_ADDR_CTRL PGIDX bit mask.
 *
 * Used to specify the index of PTE to read/write.
 */
#define SMMU_PTU_ADDR_CTRL_PGIDX_MASK GENMASK(25, 12)

/**
 * SMMU_PTU_ADDR_CTRL PGIDX bit shift.
 *
 * Used to specify the index of PTE to read/write.
 */
#define SMMU_PTU_ADDR_CTRL_PGIDX_SHIFT 12

/**
 * Address of SMMU_PTU_RD_DATA register.
 *
 * Page Table Update Read Data register.
 * Contains data read from a PTE.
 */
#define SMMU_PTU_RD_DATA (SMMU_BASE + 0x10000 + 0x8)

/**
 * SMMU_PTU_RD_DATA Present bit.
 *
 * 1 if page is present in local SRAM.
 * 0 if page is not present in local SRAM.
 */
#define SMMU_PTU_RD_DATA_PRESENT BIT(0)

/**
 * SMMU_PTU_RD_DATA Bypass bit.
 *
 * 1 if page is bypassed to IMR.
 * 0 if page is not bypassed to IMR.
 */
#define SMMU_PTU_RD_DATA_BYPASS BIT(1)

/**
 * Mask to SMMU_PTU_RD_DATA to determine page status.
 */
#define SMMU_PTU_RD_DATA_MAPPED_MASK (SMMU_PTU_RD_DATA_BYPASS | SMMU_PTU_RD_DATA_PRESENT)

/**
 * Address of SMMU_PTU_WR_DATA register.
 *
 * Page Table Update Write Data register.
 * Contains data to be written to a PTE.
 */
#define SMMU_PTU_WR_DATA (SMMU_BASE + 0x10000 + 0xC)

/**
 * SMMU_PTU_WR_DATA Present bit.
 *
 * 1 if page is present in local SRAM.
 * 0 if page is not present in local SRAM.
 */
#define SMMU_PTU_WR_DATA_PRESENT BIT(0)

/**
 * SMMU_PTU_WR_DATA Bypass bit.
 *
 * 1 if page is bypassed to IMR.
 * 0 if page is not bypassed to IMR.
 */
#define SMMU_PTU_WR_DATA_BYPASS BIT(1)

/**
 * SMMU_PTU_WR_DATA Phyical Memory bit mask.
 *
 * Physical address for translation.
 */
#if SMMU_PAGE_SIZE == 4096
#define SMMU_PTU_WR_DATA_PMEM_MASK GENMASK(22, 12)
#elif SMMU_PAGE_SIZE == 8192
#define SMMU_PTU_WR_DATA_PMEM_MASK GENMASK(22, 13)
#endif

/**
 * Address of SMMU_PAGE_IN_CONFIG register.
 *
 * Page In Config register.
 * Contains information to start page-in operation.
 *
 * Lower 16-bit contains the starting page index to page-in.
 * Upper 16-bit contains the number of pages to page-in.
 */
#define SMMU_PAGE_IN_CONFIG (SMMU_BASE + 0x10000 + 0x14)

/**
 * Construct the data to write to SMMU_PAGE_IN_CONFIG
 *
 * @param page_idx Starting page index to do page-in op.
 * @param page_nums Number of pages to page-in (zero-based, 0 = 1 page).
 */
#define SMMU_PAGE_IN(page_idx, page_nums) ((page_idx & 0xFFFF) | (page_nums << 16))

/**
 * Address of SMMU_PAGE_CMD_CTRL register.
 *
 * Page Command and Control Register.
 * Contains how and which paging operations to perform.
 */
#define SMMU_PAGE_CMD_CTRL (SMMU_BASE + 0x10000 + 0x1C)

/**
 * SMMU_PAGE_CMD_CTRL page-in bit.
 *
 * Set to 1 to start page-in operation.
 * Hardware clears to 0 once the page-in operation completes.
 */
#define SMMU_PAGE_CMD_CTRL_PAGE_IN BIT(0)

/**
 * SMMU_PAGE_CMD_CTRL paging ops mask.
 *
 * If any bit is set to 1, paging operation is in progress.
 */
#define SMMU_PAGE_CMD_CTRL_PAGING_MASK 0x7

/**
 * Address of SMMU_PAGE_STATUS register.
 *
 * Page Status registers.
 * Contains status of paging operations.
 */
#define SMMU_PAGE_STATUS (SMMU_BASE + 0x10000 + 0x20)

/**
 * SMMU_PAGE_STATUS Page-In Status mask.
 *
 * 0: page-in was successful.
 * 1: page-in failed due to incorrect page attributes.
 * 2: page-in failed due to DMA error.
 */
#define SMMU_PAGE_STATUS_PAGE_IN_STATUS_MASK (0x3)

/**
 * SMMU_PAGE_CMD_CTRL DMA APB Direction Enable bit mask.
 *
 * Opens the APB path so firmware can program the paging DMAC registers.
 * Must be disabled again once programming is done.
 */
#define SMMU_PAGE_CMD_CTRL_DMA_APB_DIR_EN GENMASK(5, 4)

/** DMAC instance used by the SMMU for page-in operations. */
#define SMMU_PAGE_IN_DMAC 0

/** DMAC instance used by the SMMU for page-out operations. */
#define SMMU_PAGE_OUT_DMAC 1

/** Channel used for paging in each DMAC instance, CH1 in the hardware registers. */
#define SMMU_PAGE_DMAC_CH 0

/** Width in bytes of a single SMMU paging DMA transfer. */
#define SMMU_DMA_DATA_WIDTH 64

/** Base address of the DW AXI DMAC instance @p x used by the SMMU. */
#define SMMU_DMAC_BASE(x) (SMMU_BASE + 0x14000 + ((x) * 0x4000))

/** DMAC configuration register (64-bit). */
#define SMMU_DMAC_CFGREG(x) (SMMU_DMAC_BASE(x) + 0x10)

/** DMAC_CFGREG DMAC enable bit. */
#define SMMU_DMAC_CFGREG_DMAC_EN BIT64(0)

/** DMAC_CFGREG interrupt enable bit. */
#define SMMU_DMAC_CFGREG_INT_EN BIT64(1)

/** Base address of channel @p y of DMAC instance @p x. */
#define SMMU_DMAC_CH_BASE(x, y) (SMMU_DMAC_BASE(x) + (((y) + 1) * 0x100))

/** Channel block transfer size register (64-bit). */
#define SMMU_DMAC_CH_BLOCK_TS(x, y) (SMMU_DMAC_CH_BASE(x, y) + 0x10)

/** Channel control register (64-bit). */
#define SMMU_DMAC_CH_CTL(x, y) (SMMU_DMAC_CH_BASE(x, y) + 0x18)

/** Channel configuration register (64-bit). */
#define SMMU_DMAC_CH_CFG(x, y) (SMMU_DMAC_CH_BASE(x, y) + 0x20)

/** Channel interrupt signal enable register (64-bit). */
#define SMMU_DMAC_CH_INTSIGNAL_EN(x, y) (SMMU_DMAC_CH_BASE(x, y) + 0x90)

/** Channel interrupt clear register (64-bit). */
#define SMMU_DMAC_CH_INTCLEAR(x, y) (SMMU_DMAC_CH_BASE(x, y) + 0x98)

/** CHx_CTL transfer width encoding for a 512-bit (64 byte) interface. */
#define SMMU_DMAC_CH_CTL_TR_WIDTH_64B 6

/** CHx_CTL burst transaction length. Hardware truncates this to its 2-bit maximum. */
#define SMMU_DMAC_CH_CTL_MSIZE_MAX 15

/* CHx_CTL fields. */
#define SMMU_DMAC_CH_CTL_SRC_TR_WIDTH(x)        FIELD_PREP(GENMASK64(10, 8), x)
#define SMMU_DMAC_CH_CTL_DST_TR_WIDTH(x)        FIELD_PREP(GENMASK64(13, 11), x)
#define SMMU_DMAC_CH_CTL_SRC_MSIZE(x)           FIELD_PREP(GENMASK64(17, 14), x)
#define SMMU_DMAC_CH_CTL_DST_MSIZE(x)           FIELD_PREP(GENMASK64(21, 18), x)
#define SMMU_DMAC_CH_CTL_NONPOSTED_LASTWRITE_EN BIT64(30)
#define SMMU_DMAC_CH_CTL_ARLEN_EN               BIT64(38)
#define SMMU_DMAC_CH_CTL_ARLEN(x)               FIELD_PREP(GENMASK64(46, 39), x)
#define SMMU_DMAC_CH_CTL_AWLEN_EN               BIT64(47)
#define SMMU_DMAC_CH_CTL_AWLEN(x)               FIELD_PREP(GENMASK64(55, 48), x)

/** CHx_CFG outstanding request limit, maximum supported by the hardware. */
#define SMMU_DMAC_CH_CFG_OSR_LMT_MAX 15

/* CHx_CFG fields. */
#define SMMU_DMAC_CH_CFG_SRC_OSR_LMT(x) FIELD_PREP(GENMASK64(58, 55), x)
#define SMMU_DMAC_CH_CFG_DST_OSR_LMT(x) FIELD_PREP(GENMASK64(62, 59), x)

/*
 * AXI burst lengths for the paging channels. Read bursts longer than a single
 * transfer do not work on this hardware, so ARLEN stays at zero on both.
 */
#define SMMU_PAGE_IN_DMAC_AWLEN  0
#define SMMU_PAGE_IN_DMAC_ARLEN  0
#define SMMU_PAGE_OUT_DMAC_AWLEN 1
#define SMMU_PAGE_OUT_DMAC_ARLEN 0

/* CHx_INTSIGNAL_ENABLEREG bits for transfer completion and AXI error reporting. */
#define SMMU_DMAC_CH_INT_DMA_TFR_DONE BIT64(1)
#define SMMU_DMAC_CH_INT_SRC_DEC_ERR  BIT64(5)
#define SMMU_DMAC_CH_INT_DST_DEC_ERR  BIT64(6)
#define SMMU_DMAC_CH_INT_SRC_SLV_ERR  BIT64(7)
#define SMMU_DMAC_CH_INT_DST_SLV_ERR  BIT64(8)

enum smmu_page_map_state {
	/** Page is not mapped */
	SMMU_PAGE_UNMAPPED = 0,

	/** Page is mapped into local physical memory */
	SMMU_PAGE_MAPPED = 1,

	/** Page is bypassed to IMR */
	SMMU_PAGE_BYPASSED = 2
};

/* SMMU page size can only be 4KB or 8KB */
BUILD_ASSERT((SMMU_PAGE_SIZE == 4096) || (SMMU_PAGE_SIZE == 8192), "SMMU page size invalid");

/**
 * Busy wait until PTE read/write command has completed.
 */
__imr static void wait_smmu_rw_idle(void)
{
	uint32_t ctrl_reg;

	/* Wait for GO bit to be cleared. */
	while (true) {
		__asm__("nop");

		ctrl_reg = sys_read32(SMMU_PTU_ADDR_CTRL);

		if ((ctrl_reg & SMMU_PTU_ADDR_CTRL_GO) == 0) {
			break;
		}
	}
}

/**
 * Read a PTE from SMMU.
 *
 * @param virt_idx Virtual page index.
 */
__imr static uint32_t read_pte(const uint32_t virt_idx)
{
	uint32_t ctrl_reg;

	wait_smmu_rw_idle();

	/* Indicate intention to read from PTE. */
	ctrl_reg = ((virt_idx << SMMU_PTU_ADDR_CTRL_PGIDX_SHIFT) & SMMU_PTU_ADDR_CTRL_PGIDX_MASK);

	/* Write to CTRL register without GO bit as required by hardware. */
	sys_write32(ctrl_reg, SMMU_PTU_ADDR_CTRL);

	/* Write GO bit so hardware can go do its magic. */
	ctrl_reg |= SMMU_PTU_ADDR_CTRL_GO;
	sys_write32(ctrl_reg, SMMU_PTU_ADDR_CTRL);

	wait_smmu_rw_idle();

	return sys_read32(SMMU_PTU_RD_DATA);
}

/**
 * Write a PTE to SMMU.
 *
 * @param virt_idx Virtual page index.
 * @param pte PTE to be written.
 */
__imr static void write_pte(const uint32_t virt_idx, const uint32_t pte)
{
	uint32_t ctrl_reg;

	wait_smmu_rw_idle();

	/* Indicate intention to write to PTE. */
	ctrl_reg = SMMU_PTU_ADDR_CTRL_CMD |
		   ((virt_idx << SMMU_PTU_ADDR_CTRL_PGIDX_SHIFT) & SMMU_PTU_ADDR_CTRL_PGIDX_MASK);

	/* Write to both register without GO bit as required by hardware. */
	sys_write32(ctrl_reg, SMMU_PTU_ADDR_CTRL);
	sys_write32(pte, SMMU_PTU_WR_DATA);

	/* Write GO bit so hardware can go do its magic. */
	ctrl_reg |= SMMU_PTU_ADDR_CTRL_GO;
	sys_write32(ctrl_reg, SMMU_PTU_ADDR_CTRL);

	wait_smmu_rw_idle();
}

/**
 * Busy wait until any paging operation has completed.
 */
__imr static void wait_smmu_paging_idle(void)
{
	uint32_t cmd_reg;

	/* Wait for all paging bits to be cleared. */
	while (true) {
		__asm__("nop");

		cmd_reg = sys_read32(SMMU_PAGE_CMD_CTRL);

		if ((cmd_reg & SMMU_PAGE_CMD_CTRL_PAGING_MASK) == 0) {
			break;
		}
	}
}

/**
 * Check if the virtual address has already been mapped.
 *
 * Since we are only checking usable memory, mapped memory usually means
 * that the region has "bypassed" to the IMR. An unmapped memory page is
 * not mapped to local SRAM or "bypassed" to IMR.
 *
 * @param virt_addr Virtual address to check
 *
 * @retval true Virtual address has been mapped by ROM.
 * @retval false Virtual address has NOT been mapped by ROM.
 */
__imr enum smmu_page_map_state get_page_mapped_state(const uintptr_t virt_addr)
{
	uint32_t rd_reg;

	const uint32_t virt_idx = (virt_addr - L2_SRAM_BASE) / SMMU_PAGE_SIZE;

	rd_reg = read_pte(virt_idx);
	rd_reg &= SMMU_PTU_RD_DATA_MAPPED_MASK;

	if (rd_reg == SMMU_PTU_RD_DATA_PRESENT) {
		return SMMU_PAGE_MAPPED;
	} else if (rd_reg == SMMU_PTU_RD_DATA_BYPASS) {
		return SMMU_PAGE_BYPASSED;
	}

	return SMMU_PAGE_UNMAPPED;
}

/**
 * Map local physical memory into virtual address space.
 *
 * @param[in] virt_addr Virtual address of the page to be mapped.
 */
__imr static void map_local(const uintptr_t virt_addr)
{
	const uint32_t virt_idx = (virt_addr - L2_SRAM_BASE) / SMMU_PAGE_SIZE;

	/* Page present on local SRAM. */
	const uint32_t pte = (virt_addr & SMMU_PTU_WR_DATA_PMEM_MASK) | SMMU_PTU_WR_DATA_PRESENT;

	write_pte(virt_idx, pte);
}

/**
 * Copy page from IMR into local memory.
 *
 * This utilizes the SMMU hardware to transfer page from IMR into local SRAM.
 *
 * @param[in] virt_addr Virtual address of the page to be copied.
 */
__imr static void page_in(const uintptr_t virt_addr)
{
	uint32_t page_in_cfg;
	uint32_t page_cmd;
	uint32_t pte;

	const uint32_t virt_idx = (virt_addr - L2_SRAM_BASE) / SMMU_PAGE_SIZE;

	/*
	 * Apparently ROM does not write the physical address to PTE.
	 * So we need to do that or else paging below would map to the first page
	 * in local SRAM and writes to it.
	 */
	pte = (virt_addr & SMMU_PTU_WR_DATA_PMEM_MASK) | SMMU_PTU_WR_DATA_BYPASS;
	write_pte(virt_idx, pte);

	/* Start page-in ops to copy from IMR to local SRAM. */
	wait_smmu_paging_idle();

	/* Note... number of pages is zero based: 0 = 1 page. */
	page_in_cfg = SMMU_PAGE_IN(virt_idx, 0);
	page_cmd = SMMU_PAGE_CMD_CTRL_PAGE_IN;

	sys_write32(page_in_cfg, SMMU_PAGE_IN_CONFIG);
	sys_write32(page_cmd, SMMU_PAGE_CMD_CTRL);

	wait_smmu_paging_idle();
}

/**
 * Enable a DMA Controller instance and configure its paging channel.
 *
 * @param dmac DMAC instance index.
 * @param block_ts Block transfer size matching the active page size.
 * @param awlen AXI write burst length.
 * @param arlen AXI read burst length.
 */
__imr static void cfg_dmac_inst(const uint32_t dmac, const uint32_t block_ts,
				const uint32_t awlen, const uint32_t arlen)
{
	uint64_t ctl;
	uint64_t cfg;

	sys_write64(SMMU_DMAC_CFGREG_DMAC_EN | SMMU_DMAC_CFGREG_INT_EN, SMMU_DMAC_CFGREG(dmac));

	ctl = SMMU_DMAC_CH_CTL_SRC_TR_WIDTH(SMMU_DMAC_CH_CTL_TR_WIDTH_64B) |
	      SMMU_DMAC_CH_CTL_DST_TR_WIDTH(SMMU_DMAC_CH_CTL_TR_WIDTH_64B) |
	      SMMU_DMAC_CH_CTL_SRC_MSIZE(SMMU_DMAC_CH_CTL_MSIZE_MAX) |
	      SMMU_DMAC_CH_CTL_DST_MSIZE(SMMU_DMAC_CH_CTL_MSIZE_MAX) |
	      SMMU_DMAC_CH_CTL_NONPOSTED_LASTWRITE_EN | SMMU_DMAC_CH_CTL_ARLEN_EN |
	      SMMU_DMAC_CH_CTL_ARLEN(arlen) | SMMU_DMAC_CH_CTL_AWLEN_EN |
	      SMMU_DMAC_CH_CTL_AWLEN(awlen);
	sys_write64(ctl, SMMU_DMAC_CH_CTL(dmac, SMMU_PAGE_DMAC_CH));

	sys_write64(block_ts, SMMU_DMAC_CH_BLOCK_TS(dmac, SMMU_PAGE_DMAC_CH));

	/* Fields left out below are zeroed, which selects memory-to-memory transfers. */
	cfg = SMMU_DMAC_CH_CFG_SRC_OSR_LMT(SMMU_DMAC_CH_CFG_OSR_LMT_MAX) |
	      SMMU_DMAC_CH_CFG_DST_OSR_LMT(SMMU_DMAC_CH_CFG_OSR_LMT_MAX);
	sys_write64(cfg, SMMU_DMAC_CH_CFG(dmac, SMMU_PAGE_DMAC_CH));

	sys_write64(SMMU_DMAC_CH_INT_DMA_TFR_DONE | SMMU_DMAC_CH_INT_SRC_DEC_ERR |
			    SMMU_DMAC_CH_INT_DST_DEC_ERR | SMMU_DMAC_CH_INT_SRC_SLV_ERR |
			    SMMU_DMAC_CH_INT_DST_SLV_ERR,
		    SMMU_DMAC_CH_INTSIGNAL_EN(dmac, SMMU_PAGE_DMAC_CH));

	sys_write64(0, SMMU_DMAC_CH_INTCLEAR(dmac, SMMU_PAGE_DMAC_CH));
}

/**
 * Configure the DMA Controllers used by the SMMU for paging operations.
 *
 * The SMMU drives page-in and page-out through two dedicated DW AXI DMAC
 * instances that have to be programmed here, so the behaviour does not depend
 * on earlier boot stages.
 */
__imr static void configure_dmacs(void)
{
	/* The DMACs block transfer size has to match the SMMU page size. */
	const uint32_t block_ts = (SMMU_PAGE_SIZE / SMMU_DMA_DATA_WIDTH) - 1;

	/* Open the APB path so the DMAC registers become writable. */
	sys_write32(SMMU_PAGE_CMD_CTRL_DMA_APB_DIR_EN, SMMU_PAGE_CMD_CTRL);

	cfg_dmac_inst(SMMU_PAGE_IN_DMAC, block_ts, SMMU_PAGE_IN_DMAC_AWLEN,
		      SMMU_PAGE_IN_DMAC_ARLEN);
	cfg_dmac_inst(SMMU_PAGE_OUT_DMAC, block_ts, SMMU_PAGE_OUT_DMAC_AWLEN,
		      SMMU_PAGE_OUT_DMAC_ARLEN);

	/* The APB path has to be closed again before issuing paging commands. */
	sys_write32(0, SMMU_PAGE_CMD_CTRL);
}

__imr void smmu_boot_mem_map(void)
{
	/*
	 * With SMMU memory mapping:
	 *   () The boot code is inside the virtual memory space overlapping SRAM range.
	 *      This usually some pages before the vector handlers.
	 *   () ROM also occupies some early memory space.
	 *
	 * Realistically, we only have control of memory space starting with the vector handlers
	 * till the end of SRAM. So we need to map these into SRAM memory to boot the firmware.
	 *
	 * Note that the ROM has already copied the firmware into memory but those pages are mapped
	 * to IMR.
	 *
	 * Note: must sync with linker script on location and size.
	 */

	uintptr_t virt_start = VECBASE_RESET_PADDR_SRAM;
	uintptr_t virt_end = RAM_BASE + RAM_SIZE;

	configure_dmacs();

	for (uintptr_t addr = virt_start; addr < virt_end; addr += SMMU_PAGE_SIZE) {
		enum smmu_page_map_state page_state = get_page_mapped_state(addr);

		if (page_state == SMMU_PAGE_UNMAPPED) {
			map_local(addr);
			bbzero((void *)addr, SMMU_PAGE_SIZE);
		} else if (page_state == SMMU_PAGE_BYPASSED) {
			page_in(addr);
		}
	}
}
