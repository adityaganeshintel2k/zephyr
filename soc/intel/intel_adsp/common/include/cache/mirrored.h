/*
 * Copyright 2021 Intel Corporation
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_SOC_INTEL_ADSP_CACHE_MIRRORED_H_
#define ZEPHYR_SOC_INTEL_ADSP_CACHE_MIRRORED_H_

#include <zephyr/toolchain.h>
#include <zephyr/sys/util.h>
#include <zephyr/debug/sparse.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Test if a pointer is in cached region.
 *
 * Some hardware may map the same physical memory twice
 * so that it can be seen in both (incoherent) cached mappings
 * and a coherent "shared" area. This tests if a particular
 * pointer is within the cached, coherent area.
 *
 * @param ptr Pointer
 *
 * @retval True if pointer is in cached region.
 * @retval False if pointer is not in cached region.
 */
static ALWAYS_INLINE bool soc_cache_is_ptr_cached(void *ptr)
{
	size_t addr = (size_t) ptr;

	return (addr >> 29) == CONFIG_INTEL_ADSP_CACHED_REGION;
}

/**
 * @brief Test if a pointer is in un-cached region.
 *
 * Some hardware may map the same physical memory twice
 * so that it can be seen in both (incoherent) cached mappings
 * and a coherent "shared" area. This tests if a particular
 * pointer is within the un-cached, incoherent area.
 *
 * @param ptr Pointer
 *
 * @retval True if pointer is not in cached region.
 * @retval False if pointer is in cached region.
 */
static ALWAYS_INLINE bool soc_cache_is_ptr_uncached(void *ptr)
{
	size_t addr = (size_t) ptr;

	return (addr >> 29) == CONFIG_INTEL_ADSP_UNCACHED_REGION;
}

static ALWAYS_INLINE uint32_t z_xtrpoflip(uint32_t addr, uint32_t rto, uint32_t rfrom)
{
	/* The math here is all compile-time: when the two regions
	 * differ by a power of two, we can convert between them by
	 * setting or clearing just one bit.  Otherwise it needs two
	 * operations.
	 */
	uint32_t rxor = (rto ^ rfrom) << 29;

	rto <<= 29;
	if (Z_IS_POW2(rxor)) {
		if ((rxor & rto) == 0) {
			return addr & ~rxor;
		} else {
			return addr | rxor;
		}
	} else {
		return (addr & ~(7U << 29)) | rto;
	}
}

/**
 * @brief Return cached pointer to a RAM address
 *
 * The Xtensa coherence architecture maps addressable RAM twice, in
 * two different 512MB regions whose L1 cache settings can be
 * controlled independently.  So for any given pointer, it is possible
 * to convert it to and from a cached version.
 *
 * This function takes a pointer to any addressable object (either in
 * cacheable memory or not) and returns a pointer that can be used to
 * refer to the same memory through the L1 data cache.  Data read
 * through the resulting pointer will reflect locally cached values on
 * the current CPU if they exist, and writes will go first into the
 * cache and be written back later.
 *
 * @see arch_uncached_ptr()
 *
 * @param ptr A pointer to a valid C object
 * @return A pointer to the same object via the L1 dcache
 */
static ALWAYS_INLINE void __sparse_cache *soc_cache_cached_ptr(void *ptr)
{
	return (__sparse_force void __sparse_cache *)z_xtrpoflip((uint32_t) ptr,
						CONFIG_INTEL_ADSP_CACHED_REGION,
						CONFIG_INTEL_ADSP_UNCACHED_REGION);
}

/**
 * @brief Return uncached pointer to a RAM address
 *
 * The Xtensa coherence architecture maps addressable RAM twice, in
 * two different 512MB regions whose L1 cache settings can be
 * controlled independently.  So for any given pointer, it is possible
 * to convert it to and from a cached version.
 *
 * This function takes a pointer to any addressable object (either in
 * cacheable memory or not) and returns a pointer that can be used to
 * refer to the same memory while bypassing the L1 data cache.  Data
 * in the L1 cache will not be inspected nor modified by the access.
 *
 * @see arch_cached_ptr()
 *
 * @param ptr A pointer to a valid C object
 * @return A pointer to the same object bypassing the L1 dcache
 */
static ALWAYS_INLINE void *soc_cache_uncached_ptr(void __sparse_cache *ptr)
{
	return (void *)z_xtrpoflip((__sparse_force uint32_t)ptr,
				   CONFIG_INTEL_ADSP_UNCACHED_REGION,
				   CONFIG_INTEL_ADSP_CACHED_REGION);
}

/**
 * @see sys_cache_is_mem_coherent
 */
static ALWAYS_INLINE bool soc_cache_is_mem_coherent(void *ptr)
{
	size_t addr = (size_t) ptr;

	return (addr >> 29) == CONFIG_INTEL_ADSP_UNCACHED_REGION;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ZEPHYR_SOC_INTEL_ADSP_CACHE_MIRRORED_H_ */
