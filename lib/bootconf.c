#include "bootconf.h"
#include "loaders.h"

static char boot_cfg_buffer[512];

// Copy value without line ending chars or leading spaces
static void val_copy(char *dst, const char *src, uint32_t maxlen)
{
	// Trim var name
	while (*src != '=') {
		src++;
	}
	// Move right after the =
	src++;
	// Trim more spaces
	while (*src == ' ' || *src == '\t') {
		src++;
	}

	while (maxlen--) {
		if (*src == '\r' || *src == '\n') {
			break;
		}
		*dst++ = *src++;
	}
	if (maxlen)
		*dst = '\0';
}

/*
  Read boot config file, get active slot config
*/
char bootconf_get_slot(const char *filename)
{
	int	  bytes_read;
	char *line_start;
	char  name = '?';

	bytes_read = read_file(filename, (BYTE *)boot_cfg_buffer);

	if (bytes_read <= 0) {
		error("BOOT: Missing or empty boot.cfg file\r\n");
		return 'R';
	}

	line_start = boot_cfg_buffer;

	while (line_start != NULL && *line_start != '\0' && (line_start - boot_cfg_buffer) <= bytes_read) {
		while (*line_start == ' ')
			line_start++;

		// Commented or empty lines
		if (*line_start == '#' || *line_start == '\n') {
			line_start = strchr(line_start, '\n');
			if (line_start)
				line_start++;
			continue;
		}

		if (strncmp(line_start, "slot", sizeof("slot") - 1) == 0) {
			val_copy(&name, line_start, 1);
			break;
		}
		line_start = strchr(line_start, '\n');
		if (line_start)
			line_start++;
	}
	// Slot name not found, use recovery
	if (name == '?') {
		error("BOOT: slot not found\r\n");
		name = 'R';
	}

	return name;
}

uint8_t bootconf_read_slot_data(const char *filename, slot_t *slot)
{
	int	  bytes_read;
	char *line_start;

	memset(slot, 0, sizeof(slot_t));

	bytes_read = read_file(filename, (BYTE *)boot_cfg_buffer);

	if (bytes_read <= 0) {
		return 1;
	}

	line_start = boot_cfg_buffer;

	while (line_start != NULL && *line_start != '\0' && (line_start - boot_cfg_buffer) <= bytes_read) {
		while (*line_start == ' ')
			line_start++;

		// Commented or empty lines
		if (*line_start == '#' || *line_start == '\n') {
			line_start = strchr(line_start, '\n');
			if (line_start)
				line_start++;
			continue;
		}

		if (strncmp(line_start, "kernel", sizeof("kernel") - 1) == 0) {
			val_copy(slot->kernel_filename, line_start, MAX_FILENAME_SIZE);
		} else if (strncmp(line_start, "dtb", sizeof("dtb") - 1) == 0) {
			val_copy(slot->dtb_filename, line_start, MAX_FILENAME_SIZE);
		} else if (strncmp(line_start, "initrd", sizeof("initrd") - 1) == 0) {
			val_copy(slot->initrd_filename, line_start, MAX_FILENAME_SIZE);
		} else if (strncmp(line_start, "args", sizeof("args") - 1) == 0) {
			val_copy(slot->kernel_cmd, line_start, MAX_CMD_SIZE);
		}

		line_start = strchr(line_start, '\n');
		if (line_start)
			line_start++;
	}

	return 0;
}