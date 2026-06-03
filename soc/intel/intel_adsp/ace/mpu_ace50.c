/*
 * Copyright (c) 2024, 2026 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include <xtensa/config/core-isa.h>

#include <zephyr/arch/xtensa/mpu.h>
#include <zephyr/linker/linker-defs.h>
#include <zephyr/sys/util.h>

#include "include/memory/unified.h"

extern char _heap_start[];
extern char _heap_end[];

extern char _cached_start[];
extern char _cached_end[];

/**
 * MPU memory type for cached local virtual address space.
 *
 * Cacheable, non-sharable, interruptible, L1 write-back cached.
 */
#define MEM_TYPE_LOCAL_VM_CACHED 0xF7

/**
 * MPU memory type for uncached local virtual address space.
 *
 * Cacheable, non-sharable, interruptible, L1 uncached.
 */
#define MEM_TYPE_LOCAL_VM_UNCACHED 0x17

/**
 * MPU memory type for alias of local virtual address space.
 *
 * Non-cacheable, sharable, interruptible, L1 uncached.
 */
#define MEM_TYPE_LOCAL_VM_ALIAS 0x1E

/**
 * MPU memory type for LP-SRAM.
 *
 * Cacheable, non-sharable, interruptible, L1 write-back cached.
 */
#define MEM_TYPE_LPSRAM 0xF7

/**
 * MPU memory type for LP-SRAM alias.
 *
 * Non-cacheable, sharable, interruptible, L1 uncached.
 */
#define MEM_TYPE_LPSRAM_ALIAS 0x1E

/**
 * MPU memory type for device MMIO.
 *
 * Device, sharable, not interruptible, not cached, not bufferable.
 */
#define MEM_TYPE_DEVICE 0x6

/**
 * MPU memory type for unmapped memory.
 *
 * Same as the background map.
 */
#define MEM_TYPE_UNMAPPED 0x6

/*
 * By default, the MPU background map has XTENSA_MPU_ACCESS_P_RW_U_NA permission
 * and memory type of MEM_TYPE_DEVICE (0x6) to cover the entire memory space.
 * For those memory regions with the same permission and memory type, there is
 * no need to define them. Having those memory regions explicitly defined will
 * take up valuable hardware MPU slots.
 *
 * Change this to #define to force those memory regions to be explicitly defined.
 */
#undef EXTRA_FG_RANGES

extern char __boot_loader_start[];
extern char __boot_loader_end[];

const struct xtensa_mpu_range xtensa_mpu_ranges[] = {
#ifdef EXTRA_FG_RANGES
	/* Device MMIO */
	{
		.start = 0x0,
		.end = 0x200000,
		.access_rights = XTENSA_MPU_ACCESS_P_RW_U_NA,
	},
#endif /* EXTRA_FG_RANGES */

	/* LP-SRAM */
	{
		.start = (uintptr_t)LP_SRAM_BASE,
		.end = (uintptr_t)(LP_SRAM_BASE + LP_SRAM_SIZE),
		.access_rights = XTENSA_MPU_ACCESS_P_RW_U_NA,
	},

	/* Cover all HPSRAM area at this point. The finer permissions below. */
	{
		.start = (uintptr_t)L2_SRAM_BASE,
		.end = (uintptr_t)(L2_SRAM_BASE + L2_SRAM_SIZE),
		.access_rights = XTENSA_MPU_ACCESS_P_RW_U_NA,
	},

	/* Boot loader */
	{
		.start = (uintptr_t)__boot_loader_start,
		.end = (uintptr_t)__boot_loader_end,
		.access_rights = XTENSA_MPU_ACCESS_P_RX_U_RX,
	},

	/* Region for vector handlers */
	{
		.start = (uintptr_t)VECBASE_RESET_PADDR_SRAM,
		.end = (uintptr_t)(VECBASE_RESET_PADDR_SRAM + VECTOR_TBL_SIZE),
		.access_rights = XTENSA_MPU_ACCESS_P_RX_U_RX,
	},

#if K_HEAP_MEM_POOL_SIZE > 0
	/* System heap memory */
	{
		.start = (uintptr_t)_heap_start,
		.end = (uintptr_t)_heap_end,
		.access_rights = XTENSA_MPU_ACCESS_P_RW_U_NA,
	},
#endif

	/* text segment */
	{
		.start = (uintptr_t)__text_region_start,
		.end = (uintptr_t)__text_region_end,
		.access_rights = XTENSA_MPU_ACCESS_P_RX_U_RX,
	},

	/* rodata segment */
	{
		.start = (uintptr_t)__rodata_region_start,
		.end = (uintptr_t)__rodata_region_end,
		.access_rights = XTENSA_MPU_ACCESS_P_RO_U_RO,
	},

#ifdef CONFIG_KERNEL_COHERENCE
	/* cached segment */
	{
		.start = (uintptr_t)_cached_start,
		.end = (uintptr_t)_cached_end,
		.access_rights = XTENSA_MPU_ACCESS_P_RW_U_NA,
	},
#endif /* CONFIG_KERNEL_COHERENCE */
};

const unsigned int xtensa_mpu_ranges_num = ARRAY_SIZE(xtensa_mpu_ranges);

const struct xtensa_mpu_mem_type_region xtensa_mpu_mem_type_ranges[] = {
	/* Device MMIO */
	{
		.start = 0x0,
		.end = 0x200000,
		.memory_type = MEM_TYPE_DEVICE,
	},

	/* Unmapped memory */
	{
		.start = 0x200000,
		.end = (uintptr_t)LP_SRAM_BASE,
		.memory_type = MEM_TYPE_UNMAPPED,
	},

	/* LP-SRAM */
	{
		.start = 0x40000000,
		.end = 0x48000000,
		.memory_type = MEM_TYPE_LPSRAM,
	},

	/* LP-SRAM alias */
	{
		.start = 0x48000000,
		.end = 0x50000000,
		.memory_type = MEM_TYPE_LPSRAM_ALIAS,
	},

	/* Local virtual address space */
	{
		.start = 0x50000000,
		.end = 0x60000000,
		.memory_type = MEM_TYPE_LOCAL_VM_UNCACHED,
	},

	/* boot loader, windows vector and text segment */
	{
		.start = (uintptr_t)__boot_loader_start,
		.end = (uintptr_t)__text_region_end,
		.memory_type = MEM_TYPE_LOCAL_VM_CACHED,
	},

	/* rodata segment */
	{
		.start = (uintptr_t)__rodata_region_start,
		.end = (uintptr_t)__rodata_region_end,
		.memory_type = MEM_TYPE_LOCAL_VM_CACHED,
	},

#ifdef CONFIG_KERNEL_COHERENCE
	/* cached segment */
	{
		.start = (uintptr_t)_cached_start,
		.end = (uintptr_t)_cached_end,
		.memory_type = MEM_TYPE_LOCAL_VM_CACHED,
	},
#endif /* CONFIG_KERNEL_COHERENCE */

	/* Local virtual address space alias */
	{
		.start = 0x60000000,
		.end = 0x70000000,
		.memory_type = MEM_TYPE_LOCAL_VM_ALIAS,
	},

	/* Unmapped memory */
	{
		.start = 0x70000000,
		.end = 0xFFFFFFFF,
		.memory_type = MEM_TYPE_UNMAPPED,
	},
};

const unsigned int xtensa_mpu_mem_type_ranges_num = ARRAY_SIZE(xtensa_mpu_mem_type_ranges);
