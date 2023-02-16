#ifndef __BOOTCONF_H__
#define __BOOTCONF_H__

#include "board.h"
#include "ff.h"

typedef struct {
	char	 dtb_filename[MAX_FILENAME_SIZE];
	char	 kernel_filename[MAX_FILENAME_SIZE];
	char	 initrd_filename[MAX_FILENAME_SIZE];
	char	 kernel_cmd[MAX_CMD_SIZE];
	uint32_t initrd_start;
	uint32_t initrd_end;
} slot_t;

char	bootconf_get_slot(const char *filename);
bool	bootconf_is_slot_state_good(const char *filename);
uint8_t bootconf_load_slot_data(const char *filename, slot_t *slot);

#endif
