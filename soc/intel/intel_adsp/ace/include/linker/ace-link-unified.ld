/*
 * Copyright (c) 2022, 2026 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Linker command/script file
 *
 * Linker script for the intel_apl_adsp platform with unified memory layout
 */

OUTPUT_ARCH(xtensa)

#include <zephyr/devicetree.h>
#include <xtensa/config/core-isa.h>
#include <zephyr/linker/sections.h>
#include <adsp_memory.h>

#include <zephyr/linker/linker-defs.h>
#include <zephyr/linker/linker-tool.h>

ENTRY(rom_entry);

/* intlist.ld needs an IDT_LIST memory region */
#define IDT_BASE 0xe0000000
#define IDT_SIZE 0x2000

/* rimage module sections are C struct data, and thus flagged ALLOC.
 * The xcc linker demands they be in a declared memory region even if
 * the enclosing output section is (NOLOAD).  Put them here.
 */
#define NOLOAD_BASE 0x20000
#define NOLOAD_SIZE 0x100000

#ifdef XCHAL_MPU_ALIGN
#define _ALIGN_SIZE XCHAL_MPU_ALIGN
#else
#define _ALIGN_SIZE 4096
#endif /* XCHAL_MPU_ALIGN */

/* Even if CONFIG_MPU is not enabled, we still want things aligned to
 * satisfy alignment required for rimage.
 */
#define MPU_SEGMENT_ALIGN . = ALIGN(_ALIGN_SIZE);
#define HDR_MPU_SEGMENT_ALIGN ALIGN(_ALIGN_SIZE)
#define LAST_RAM_ALIGN MPU_SEGMENT_ALIGN

#define RAMABLE_REGION ram
#define ROMABLE_REGION ram

MEMORY {
  vectors :
    org = VECBASE_RESET_PADDR_SRAM,
    len = VECTOR_TBL_SIZE
  boot_loader :
    org = BOOT_LDR_BASE,
    len = BOOT_LDR_SIZE
  ram :
    org = RAM_BASE,
    len = RAM_SIZE
#ifdef CONFIG_GEN_ISR_TABLES
  IDT_LIST :
    org = IDT_BASE,
    len = IDT_SIZE
#endif
  lpram :
    org = LP_SRAM_BASE,
    len = LP_SRAM_SIZE
  noload :
    org = NOLOAD_BASE,
    len = NOLOAD_SIZE
}

SECTIONS {

  .boot_loader : {
    __boot_loader_start = .;

    /* FIXME: previous ACE versions put some boot code in IMR.
     * Since memory mapping has changed. Place those here for now...
     * ... so we can boot the system.
     */
    KEEP (*(.boot_entry.text))
    KEEP (*(.imr.z_boot_asm_entry.literal))
    KEEP (*(.imr.z_boot_asm_entry))
    *(.imr .imr.*)

    MPU_SEGMENT_ALIGN

    __boot_loader_end = .;
  } >boot_loader

/* Auto-generated vector linkage, to "vectors" memory region */
#include <xtensa_vectors.ld>
  >vectors

  .text : {
    __text_region_start = .;
    _image_ram_start = .;

    *(.iram1 .iram1.*)
    *(.entry.text)
    *(.init.literal)
    *(.iram0.text)

    KEEP(*(.init))
    KEEP(*(.lps_vector))
    *(.stub .gnu.warning .gnu.linkonce.literal.* .gnu.linkonce.t.*.literal .gnu.linkonce.t.*)
    MPU_SEGMENT_ALIGN
    *(.literal .text)
    *(.literal.* .text.*)
    *(.fini.literal)
    KEEP(*(.fini))
    *(.gnu.version)

#include <snippets-text-sections.ld>

#include <zephyr/linker/kobject-text.ld>

    MPU_SEGMENT_ALIGN
    __text_region_end = .;
    __text_region_size = __text_region_end - __text_region_start;
  } >ram

  .rodata : HDR_MPU_SEGMENT_ALIGN
  {
  __rodata_region_start = .;
    *(.rodata)
    *(.rodata.*)
    *(.gnu.linkonce.r.*)
    *(.rodata1)

    . = ALIGN(4);
    #include <snippets-rodata.ld>
    #include <zephyr/linker/kobject-rom.ld>

    __XT_EXCEPTION_TABLE__ = .;
    KEEP (*(.xt_except_table))
    KEEP (*(.gcc_except_table .gcc_except_table.*))
    *(.gnu.linkonce.e.*)
    *(.gnu.version_r)
    KEEP (*(.eh_frame))
    KEEP (*crtbegin.o(.ctors))
    KEEP (*crtbegin.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    __XT_EXCEPTION_DESCS__ = .;
    *(.xt_except_desc)
    *(.gnu.linkonce.h.*)
    __XT_EXCEPTION_DESCS_END__ = .;
    *(.xt_except_desc_end)
    *(.dynamic)
    *(.gnu.version_d)

    MPU_SEGMENT_ALIGN
  } >ram

  .module_init : HDR_MPU_SEGMENT_ALIGN {
    _module_init_start = .;
    *(*.initcall)
    _module_init_end = .;
  } >ram

#include <zephyr/linker/common-rom.ld>

/* Located in generated directory. This file is populated by calling
 * zephyr_linker_sources(ROM_SECTIONS ...). Useful for grouping iterable RO structs.
 */
#include <snippets-rom-sections.ld>

#include <zephyr/linker/thread-local-storage.ld>

  MPU_SEGMENT_ALIGN

  __rodata_region_end = .;
  __rodata_region_size = __rodata_region_end - __rodata_region_start;

#ifdef CONFIG_KERNEL_COHERENCE
  /* This section is cached.  By default it contains only declared
   * thread stacks, but applications can put symbols here too.
   */
  .cached : HDR_MPU_SEGMENT_ALIGN {
     _cached_start = .;

    *(.cached .cached.*)

#ifdef CONFIG_USERSPACE
    z_user_stacks_start = .;
    *(.user_stacks .user_stacks.*)
    z_user_stacks_end = .;
#endif

    MPU_SEGMENT_ALIGN

    _cached_end = .;
  } >ram
#endif /* CONFIG_KERNEL_COHERENCE */

  .noinit (NOLOAD) : HDR_MPU_SEGMENT_ALIGN {
    __data_start = .;

    *(.noinit)
    *(.noinit.*)

#if defined(CONFIG_USERSPACE) && !defined(CONFIG_KERNEL_COHERENCE)
    z_user_stacks_start = .;
    *(.user_stacks .user_stacks.*)
    z_user_stacks_end = .;
#endif /* CONFIG_USERSPACE && !CONFIG_KERNEL_COHERENCE */

/* Located in generated directory. This file is populated by the
 * zephyr_linker_sources() CMake function.
 */
#include <snippets-noinit.ld>

  } >ram

  .data : HDR_MPU_SEGMENT_ALIGN {
    /* FIXME: previous ACE versions put some boot code in IMR.
     * Since memory mapping has changed. Place those here for now...
     * ... so we can boot the system.
     */
    *(.imrdata .imrdata.*)

    *(.data)
    *(.data.*)
    *(.gnu.linkonce.d.*)
    KEEP(*(.gnu.linkonce.d.*personality*))
    *(.data1)
    *(.sdata)
    *(.sdata.*)
    *(.gnu.linkonce.s.*)
    *(.sdata2)
    *(.sdata2.*)
    *(.gnu.linkonce.s2.*)
    KEEP(*(.jcr))
    _trace_ctx_start = ABSOLUTE(.);
    *(.trace_ctx)
    _trace_ctx_end = ABSOLUTE(.);
    *(.gna_model)

    MPU_SEGMENT_ALIGN

    __data_end = .;
  } >ram

#include <snippets-ram-sections.ld>

/* Located in generated directory. This file is populated by the
 * zephyr_linker_sources() Cmake function.
 */
#include <snippets-data-sections.ld>

#include <zephyr/linker/common-ram.ld>

#include <zephyr/linker/cplusplus-ram.ld>

  .fw_ready : {
    KEEP(*(".fw_ready"));
    KEEP (*(.fw_ready_metadata))
  } > ram

#ifdef CONFIG_USERSPACE
  /* APP SHARED MEMORY REGION */

#define SMEM_PARTITION_ALIGN(size) MPU_SEGMENT_ALIGN
#define APP_SHARED_ALIGN MPU_SEGMENT_ALIGN
#define EMPTY_APP_SHARED_ALIGN APP_SHARED_ALIGN

#include <app_smem.ld>

  _app_smem_size = _app_smem_end - _app_smem_start;
  _app_smem_num_words = _app_smem_size >> 2;
  _app_smem_rom_start = LOADADDR(_APP_SMEM_SECTION_NAME);

#endif /* CONFIG_USERSPACE */

  .tm_clone_table : {
    *(.tm_clone_table)
  } >ram

#include <snippets-sections.ld>

/* Must be last in RAM */
#include <zephyr/linker/kobject-data.ld>

  /* Rimage requires page alignment */
  .bss (NOLOAD) : HDR_MPU_SEGMENT_ALIGN
  {
    __bss_start = .;

#if defined(CONFIG_ZTEST) && defined(CONFIG_SIMULATOR_XTENSA)
    /* For some weird unknown reasons, the simulator really do not
     * like these cpuhold_* variables to be tightly packed together.
     * This results in cpuhold_spawned not being updated, and we
     * will be stuck in the while loop for it to be set.
     * Workaround by explicitly aligning these variables.
     */
    . = ALIGN(16);
    *:ztest.c.obj(.bss.cpuhold_sem)
    . = ALIGN(16);
    *:ztest.c.obj(.bss.cpuhold_active)
    . = ALIGN(16);
    *:ztest.c.obj(.bss.cpuhold_spawned)
    . = ALIGN(16);
    *:ztest.c.obj(.bss.cpuhold_pool_items)
#endif /* CONFIG_ZTEST && CONFIG_SIMULATOR_XTENSA */

    *(.dynsbss)
    *(.sbss)
    *(.sbss.*)
    *(.gnu.linkonce.sb.*)
    *(.scommon)
    *(.sbss2)
    *(.sbss2.*)
    *(.gnu.linkonce.sb2.*)
    *(.dynbss)
    *(.bss)
    *(.bss.*)
    *(.gnu.linkonce.b.*)
    *(COMMON)
    MPU_SEGMENT_ALIGN
    __bss_end = .;
  } >ram

#include <zephyr/linker/ram-end.ld>

  MPU_SEGMENT_ALIGN

  /* Heap start and end markers. Used to reserve system heap memory. */
  .heap_mem (NOLOAD) :
  {
    _shared_heap_start = .;
    *(.shared_heap_mem)
    _shared_heap_end = .;
    _heap_start = .;
    *(.heap_mem)
  } >ram

  _end = ALIGN(8);

  .unused_ram_start_marker (NOLOAD) :
  {
    . = ALIGN(4096);
    _unused_ram_start_marker = .;
    *(.unused_ram_start_marker)
    *(.unused_ram_start_marker.*)
  } >ram

  /* Heap start and end markers. Used with libc malloc code. */
  . = L2_SRAM_BASE + L2_SRAM_SIZE;
  _heap_end = .;
  _heap_sentry = .;

  /* dma buffers */
  .lpbuf (NOLOAD): {
    _dma_buf_start = .;
    *(.dma_buffers)
    _dma_buf_end = .;
  } >lpram

  /* Non-loadable sections below. */

  /* rimage module manifest headers */
  .module.boot : { KEEP(*(.module.boot)) } >noload
  .module.cold : { KEEP(*(.module.cold)) } >noload
  .module.main : { KEEP(*(.module.main)) } >noload

 .static_uuid_entries : {
    *(*.static_uuids)
  } >noload

  .static_log_entries : {
    *(*.static_log*)
  } >noload

  /* This is the "extended manifest" data (mostly versioning stuff)
   * emitted by SOF and inspected by the kernel driver.  It doesn't
   * appear directly in the image, but rimage will parse and repack
   * this into the output file header, so requires this be present
   * even if empty.  Alignment and padding to 16 bytes is required,
   * otherwise rimage will complain about the size being wrong (which
   * sounds like a struct should be declared packed somewhere...)
   */
  .fw_metadata : ALIGN(16) {
    KEEP (*(.fw_metadata))
    . = ALIGN(16);
  } >noload

#include <zephyr/linker/debug-sections.ld>

  /DISCARD/ : { *(.note.GNU-stack) }

  .xtensa.info 0 : { *(.xtensa.info) }
  .xt.insn 0 : {
    KEEP (*(.xt.insn))
    KEEP (*(.gnu.linkonce.x.*))
  }
  .xt.prop 0 : {
    KEEP (*(.xt.prop))
    KEEP (*(.xt.prop.*))
    KEEP (*(.gnu.linkonce.prop.*))
  }
  .xt.lit 0 : {
    KEEP (*(.xt.lit))
    KEEP (*(.xt.lit.*))
    KEEP (*(.gnu.linkonce.p.*))
  }
  .xt.profile_range 0 : {
    KEEP (*(.xt.profile_range))
    KEEP (*(.gnu.linkonce.profile_range.*))
  }
  .xt.profile_ranges 0 : {
    KEEP (*(.xt.profile_ranges))
    KEEP (*(.gnu.linkonce.xt.profile_ranges.*))
  }
  .xt.profile_files 0 : {
    KEEP (*(.xt.profile_files))
    KEEP (*(.gnu.linkonce.xt.profile_files.*))
  }

#ifdef CONFIG_GEN_ISR_TABLES
#include <zephyr/linker/intlist.ld>
#endif

#include <zephyr/linker/llext-sections.ld>

}
