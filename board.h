#ifndef __BOARD_H__
#define __BOARD_H__

#include "dram.h"
#include "sunxi_spi.h"
#include "sunxi_usart.h"
#include "sunxi_sdhci.h"

#define MB(x) (x * 1024 * 1024)

#define CONFIG_BOOT_CMD			 "console=ttyS3,115200 earlycon"
#define CONFIG_BOOT_INITRD_START 0x43100000
#define CONFIG_BOOT_INITRD_END	 0x44000000

// #define CONFIG_BOOT_SPINAND
// #define CONFIG_BOOT_SDCARD
// #define CONFIG_BOOT_MMC

#define CONFIG_FATFS_CACHE_SIZE 36 // (unit: 512B sectors, multiples of 8 to match FAT's 4KB)
// #define CONFIG_SDMMC_SPEED_TEST_SIZE 1024 // (unit: 512B sectors)

#define CONFIG_CPU_FREQ 1200000000

// #define CONFIG_ENABLE_CPU_FREQ_DUMP

#define CONFIG_KERNEL_LOAD_ADDR	   (SDRAM_BASE + MB(32))
#define CONFIG_DTB_LOAD_ADDR	   (SDRAM_BASE + MB(48))
#define CONFIG_INITRAMFS_LOAD_ADDR (SDRAM_BASE + MB(49))

// 128KB erase sectors, 2KB pages, so place them starting from 2nd sector
#define CONFIG_SPINAND_DTB_ADDR	   (128 * 2048)
#define CONFIG_SPINAND_KERNEL_ADDR (256 * 2048)

typedef struct {
	const char *dtb_filename;
	const char *kernel_filename;
	const char *kernel_cmd;
	const char *initrd_filename;
	uint32_t	initrd_start;
	uint32_t	initrd_end;
} slot_t;

extern dram_para_t	 ddr_param;
extern sunxi_usart_t usart_dbg, usart_mgmt;
extern sunxi_spi_t	 sunxi_spi0;

extern slot_t slots[3];

void board_init(void);
void board_set_led(uint8_t num, uint8_t on);

#endif
