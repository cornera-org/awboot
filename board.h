#ifndef __BOARD_H__
#define __BOARD_H__

#include "dram.h"
#include "sunxi_spi.h"
#include "sunxi_usart.h"
#include "sunxi_sdhci.h"

#define RTC_BKP_REG(n) *((uint32_t *)((0x07090100) + (n * 4)))

#define MB(x) (x * 1024 * 1024)

#define CONFIG_KERNEL_LOAD_ADDR	   (SDRAM_BASE + MB(32))
#define CONFIG_DTB_LOAD_ADDR	   (SDRAM_BASE + MB(48))
#define CONFIG_INITRAMFS_LOAD_ADDR (SDRAM_BASE + MB(49))
#define CONFIG_INITRAMFS_MAX_SIZE  MB(25)

#define CONFIG_CONF_FILENAME	"boot.cfg"
#define CONFIG_DEFAULT_BOOT_CMD "console=ttyS3,115200 earlycon"
#define CONFIG_BOOT_MAX_TRIES	2

// #define CONFIG_BOOT_SPINAND
// #define CONFIG_BOOT_SDCARD
#define CONFIG_BOOT_MMC

#define CONFIG_FATFS_CACHE_SIZE 36 // (unit: 512B sectors, multiples of 8 to match FAT's 4KB)
// #define CONFIG_SDMMC_SPEED_TEST_SIZE 1024 // (unit: 512B sectors)

#define CONFIG_CPU_FREQ 1200000000

// #define CONFIG_ENABLE_CPU_FREQ_DUMP

// 128KB erase sectors, 2KB pages, so place them starting from 2nd sector
#define CONFIG_SPINAND_DTB_ADDR	   (128 * 2048)
#define CONFIG_SPINAND_KERNEL_ADDR (256 * 2048)

#define LED_BOARD  1
#define LED_BUTTON 2

extern dram_para_t	 ddr_param;
extern sunxi_usart_t usart_dbg;
extern sunxi_spi_t	 sunxi_spi0;

extern sdhci_t sdhci0;
extern sdhci_t sdhci2;

void board_init(void);
void board_set_led(uint8_t num, uint8_t on);
bool board_get_button(void);
void board_set_status(bool on);
bool board_get_power_on(void);

#endif
