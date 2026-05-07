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
