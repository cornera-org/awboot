/*
 * Copyright (C) 2012 Microchip Technology Inc. and its subsidiaries
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef __FDT_H__
#define __FDT_H__

/* see linux document: ./Documentation/devicetree/booting-without-of.txt */
#define OF_DT_MAGIC 0xd00dfeed

typedef struct {
	unsigned int magic_number;
	unsigned int total_size;
	unsigned int offset_dt_struct;
	unsigned int offset_dt_strings;
	unsigned int offset_reserve_map;
	unsigned int format_version;
	unsigned int last_compatible_version;

	/* version 2 field */
	unsigned int mach_id;
	/* version 3 field */
	unsigned int dt_strings_len;
	/* version 17 field */
	unsigned int dt_struct_len;
} boot_param_header_t;

unsigned int fdt_get_total_size(void *blob);
int			 fdt_check_blob_valid(void *blob);
int			 fdt_update_bootargs(void *blob, const char *bootargs);
int			 fdt_update_initrd(void *blob, uint32_t start, uint32_t end);
int			 fdt_update_memory(void *blob, unsigned int mem_bank, unsigned int mem_size);
#endif /* #ifndef __FDT_H__ */
