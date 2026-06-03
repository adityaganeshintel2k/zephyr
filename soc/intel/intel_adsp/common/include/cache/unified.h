/*
 * Copyright 2021, 2026 Intel Corporation
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_SOC_INTEL_ADSP_CACHE_UNIFIED_H_
#define ZEPHYR_SOC_INTEL_ADSP_CACHE_UNIFIED_H_

#include <zephyr/toolchain.h>
#include <zephyr/sys/util.h>
#include <zephyr/debug/sparse.h>

#include <zephyr/arch/xtensa/mpu.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Bit in memory type saying if it is cached in local cache. */
#define XTENSA_MPU_MEMORY_TYPE_CACHED_BIT BIT(7)

/**
 * @brief Test if a pointer is in cached region.
 *
 * Find out if a pointer is cached via the MPU memory type ranges array.
 *
 * @param ptr Pointer
 *
 * @retval True if pointer is in cached region.
 * @retval False if pointer is not in cached region.
 */
static ALWAYS_INLINE bool soc_cache_is_ptr_cached(void *ptr)
{
#ifdef CONFIG_KERNEL_COHERENCE
	bool cached = false;

	extern const char _cached_start[];
	extern const char _cached_end[];

	/* Cached section in linker is always considered cache. So it's a quick check. */
	if (((uintptr_t)ptr >= (uintptr_t)_cached_start) &&
	    ((uintptr_t)ptr < (uintptr_t)_cached_end)) {
		return true;
	}

	/* Need to go through the whole list as it is possible that later
	 * entry in the array overrides previous entries.
	 */
	for (unsigned int i = 0; i < xtensa_mpu_mem_type_ranges_num; i++) {
		const struct xtensa_mpu_mem_type_region *region = &xtensa_mpu_mem_type_ranges[i];

		if (((uintptr_t)ptr >= region->start) && ((uintptr_t)ptr < region->end)) {
			cached = region->memory_type & XTENSA_MPU_MEMORY_TYPE_CACHED_BIT;
		}
	}

	return cached;
#else  /* CONFIG_KERNEL_COHERENCE */
	return true;
#endif /* CONFIG_KERNEL_COHERENCE */
}

/**
 * @brief Test if a pointer is in un-cached region.
 *
 * Find out if a pointer is uncached via the MPU memory type ranges array.
 *
 * @param ptr Pointer
 *
 * @retval True if pointer is not in cached region.
 * @retval False if pointer is in cached region.
 */
static ALWAYS_INLINE bool soc_cache_is_ptr_uncached(void *ptr)
{
	return !soc_cache_is_ptr_cached(ptr);
}

/**
 * @brief Return cached pointer to a RAM address
 *
 * The unified memory model does not have different addresses for cached and
 * uncached regions as in mirrored memory model. So we simply return the same
 * address.
 *
 * @see arch_uncached_ptr()
 *
 * @param ptr A pointer to a valid C object
 * @return A pointer to the same object via the L1 dcache
 */
static ALWAYS_INLINE void *soc_cache_cached_ptr(void *ptr)
{
	return ptr;
}

/**
 * @brief Return uncached pointer to a RAM address
 *
 * The unified memory model does not have different addresses for cached and
 * uncached regions as in mirrored memory model. So we simply return the same
 * address.
 *
 * @see arch_cached_ptr()
 *
 * @param ptr A pointer to a valid C object
 * @return A pointer to the same object bypassing the L1 dcache
 */
static ALWAYS_INLINE void *soc_cache_uncached_ptr(void __sparse_cache *ptr)
{
	return ptr;
}

/**
 * @see sys_cache_is_mem_coherent
 */
static ALWAYS_INLINE bool soc_cache_is_mem_coherent(void *ptr)
{
#ifdef CONFIG_KERNEL_COHERENCE
	return soc_cache_is_ptr_uncached(ptr);
#else
	return true;
#endif /* CONFIG_KERNEL_COHERENCE */
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ZEPHYR_SOC_INTEL_ADSP_CACHE_UNIFIED_H_ */
