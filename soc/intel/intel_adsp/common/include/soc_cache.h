/*
 * Copyright 2021 Intel Corporation
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_SOC_INTEL_ADSP_CACHE_H_
#define ZEPHYR_SOC_INTEL_ADSP_CACHE_H_

#include <zephyr/toolchain.h>

/* We can reuse most of the Xtensa cache functions. */
#include <zephyr/arch/xtensa/cache.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_DCACHE)

#define cache_data_enable arch_dcache_enable
#define cache_data_disable arch_dcache_disable
#define cache_data_flush_all arch_dcache_flush_all
#define cache_data_invd_all arch_dcache_invd_all
#define cache_data_flush_and_invd_all arch_dcache_flush_and_invd_all
#define cache_data_flush_range(addr, size) arch_dcache_flush_range(addr, size)
#define cache_data_invd_range(addr, size) arch_dcache_invd_range(addr, size)
#define cache_data_flush_and_invd_range(addr, size) arch_dcache_flush_and_invd_range(addr, size)
#define cache_data_line_size_get arch_dcache_line_size_get

#endif /* CONFIG_DCACHE */

#if defined(CONFIG_ICACHE)

#define cache_instr_enable arch_icache_enable
#define cache_instr_disable arch_icache_disable
#define cache_instr_flush_all arch_icache_flush_all
#define cache_instr_invd_all arch_icache_invd_all
#define cache_instr_flush_and_invd_all arch_icache_flush_and_invd_all
#define cache_instr_flush_range(addr, size) arch_icache_flush_range(addr, size)
#define cache_instr_invd_range(addr, size) arch_icache_invd_range(addr, size)
#define cache_instr_flush_and_invd_range(addr, size) arch_icache_flush_and_invd_range(addr, size)
#define cache_instr_line_size_get arch_icache_line_size_get

#endif /* CONFIG_ICACHE */

/**
 * @brief Initialize SoC cache.
 *
 * ROM has already initialized the cache so we don't have to do anything here.
 */
static ALWAYS_INLINE void soc_cache_init(void)
{
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#if defined(CONFIG_INTEL_ADSP_MEMORY_IS_MIRRORED)
#include "cache/mirrored.h"
#else
static ALWAYS_INLINE bool soc_cache_is_ptr_cached(void *ptr)
{
	ARG_UNUSED(ptr);

	return false;
}

static ALWAYS_INLINE bool soc_cache_is_ptr_uncached(void *ptr)
{
	ARG_UNUSED(ptr);

	return false;
}

static ALWAYS_INLINE void *soc_cache_cached_ptr(void *ptr)
{
	ARG_UNUSED(ptr);

	return ptr;
}

static ALWAYS_INLINE void *soc_cache_uncached_ptr(void *ptr)
{
	ARG_UNUSED(ptr);

	return ptr;
}

static ALWAYS_INLINE bool soc_cache_is_mem_coherent(void *ptr)
{
	ARG_UNUSED(ptr);

	return false;
}
#endif

#ifdef cache_is_ptr_cached
#undef cache_is_ptr_cached
#endif
#define cache_is_ptr_cached(ptr) soc_cache_is_ptr_cached(ptr)

#ifdef cache_is_ptr_uncached
#undef cache_is_ptr_uncached
#endif
#define cache_is_ptr_uncached(ptr) soc_cache_is_ptr_uncached(ptr)

#ifdef cache_cached_ptr
#undef cache_cached_ptr
#endif
#define cache_cached_ptr(ptr) soc_cache_cached_ptr(ptr)

#ifdef cache_uncached_ptr
#undef cache_uncached_ptr
#endif
#define cache_uncached_ptr(ptr) soc_cache_uncached_ptr(ptr)

#ifdef cache_is_mem_coherent
#undef cache_is_mem_coherent
#endif
#define cache_is_mem_coherent(ptr) soc_cache_is_mem_coherent(ptr)

#endif /* ZEPHYR_SOC_INTEL_ADSP_CACHE_H_ */
