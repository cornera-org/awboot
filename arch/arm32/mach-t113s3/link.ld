/**
 * \file
 *
 * \brief Linker script for T113-S3 internal SRAM
 *
 * based on: Linker script for running in internal FLASH on the SAMD21G17D
 * Copyright (c) 2018 Microchip Technology Inc.
 *
 * \asf_license_start
 *
 * \page License
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the Licence at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * \asf_license_stop
 *
 */

/* This file gets parsed by the preprocessor */

#ifndef PSCI_REGION_ALIGN
#define PSCI_REGION_ALIGN 0x20
#endif

#ifndef PSCI_STACK_SIZE
#define PSCI_STACK_SIZE 0x200
#endif

#ifndef PSCI_MON_STACK_SIZE
#define PSCI_MON_STACK_SIZE 0x100
#endif
OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
SEARCH_DIR(.)

/* Memory Spaces Definitions */
MEMORY
{
    ram (rwx) : ORIGIN = __RAM_BASE, LENGTH = 96K /* A1 + DSP0 IRAM + DSP0 DRAM0. 128K on boot mode, 96K on FEL mode */
    ddr (rwx) : ORIGIN = 0x40000000, LENGTH = 128M
}

PHDRS
{
    text PT_LOAD FLAGS(0x5);
    data PT_LOAD FLAGS(0x6);
}

/* The stack size used by the application. NOTE: you need to adjust according to your application. */
STACK_SIZE = 0x2800; /* 5KB */

ENTRY(reset)

/* Section Definitions */
SECTIONS
{
    .text :
    {
    . = ALIGN(4);
    PROVIDE(__spl_start = .);
    KEEP(*(.boot_header))
    *(.text .text.*)
    . = ALIGN(PSCI_REGION_ALIGN);
    KEEP(*(.psci_vectors))
    . = ALIGN(PSCI_REGION_ALIGN);
    KEEP(*(.psci_text))
    . = ALIGN(PSCI_REGION_ALIGN);
    KEEP(*(.psci_tramp))
    . = ALIGN(PSCI_REGION_ALIGN);
    KEEP(*(.psci_mon))
    . = ALIGN(4);
    } > ram :text

    . = ALIGN(4);

    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > ram :text

    .data :
    {
        . = ALIGN(4);
        *(.data .data.*)
        *(.sdata .sdata.*)
        KEEP(*(.psci_data))
        KEEP(*(.psci_rodata))
        . = ALIGN(4);
    } > ram :data

    .noinit (NOLOAD) :
    {
        . = ALIGN(4);
        KEEP(*(.noinit))
        . = ALIGN(4);
    } > ram :data

    /* .bss section which is used for uninitialized data */
    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = . ;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = . ;
    } > ram :data

    .stack (NOLOAD):
    {
        . = ALIGN(8);
    /* SRV stack section */
        __stack_srv_start = .;
        . += STACK_SIZE;
        __stack_srv_end = .;
    } > ram :data

    .psci_stack (NOLOAD):
    {
        . = ALIGN(16);
        __psci_stack_base = .;
        . += PSCI_STACK_SIZE;
        __psci_stack_top = .;
        . = ALIGN(16);
    } > ram :data

    .psci_mon_stack (NOLOAD):
    {
        . = ALIGN(16);
        __psci_mon_stack_base = .;
        . += PSCI_MON_STACK_SIZE;
        __psci_mon_stack_top = .;
        . = ALIGN(16);
    } > ram :data

    . = ALIGN(4);
    _end = . ;
    PROVIDE(__spl_end = _end);
    PROVIDE(__spl_size = __spl_end - __spl_start);
}
