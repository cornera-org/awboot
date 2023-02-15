#include "common.h"
#include "fdt.h"
#include "sunxi_clk.h"
#include "sunxi_wdg.h"
#include "sdmmc.h"
#include "arm32.h"
#include "debug.h"
#include "board.h"
#include "barrier.h"
#include "bootconf.h"
#include "loaders.h"

image_info_t image;

uint8_t		  uartbuf[40];
char		  cmd_line[128];
static slot_t slot;

static int boot_image_setup(unsigned char *addr, unsigned int *entry)
{
	linux_zimage_header_t *zimage_header = (linux_zimage_header_t *)addr;

	if (zimage_header->magic == LINUX_ZIMAGE_MAGIC) {
		*entry = ((unsigned int)addr + zimage_header->start);
		return 0;
	}

	error("unsupported kernel image\r\n");

	return -1;
}

int main(void)
{
	unsigned int entry_point = 0;
	uint32_t	 memory_size;
	uint32_t	 wait	   = 0;
	char		 slot_name = 'R';
	uint8_t		 slot_num  = 0;
	const char  *slot_filename;
	uint8_t		 btn_led_val = false;
	board_init();
	sunxi_clk_init();

	message("\r\n");
	info("AWBoot r%u starting...\r\n", (u32)BUILD_REVISION);

	memory_size = sunxi_dram_init();

	void (*kernel_entry)(int zero, int arch, unsigned int params);

#ifdef CONFIG_ENABLE_CPU_FREQ_DUMP
	sunxi_clk_dump();
#endif

	// Check if we should be running
	if (!board_get_power_on()) {
		info("Waiting for power off...");
		board_set_status(0);
		while (1) { // wait for poweroff or watchdog
		};
	} else {
		// Indicate to MCU that we are running
		board_set_status(1);
	}

	// Wait for recovery or FEL mode
	// After 10s, mgmt MCU will reset CPU in FEL mode so we have to wait indefinitely
	if (board_get_button()) {
		info("Button pressed, waiting 10 seconds...\r\n");
	};
	while (board_get_button()) {
		mdelay(245);
		wait += 250;
		// Blink button
		btn_led_val = !btn_led_val;
		board_set_led(LED_BUTTON, btn_led_val);
		message("*");
		if (wait > 10000) {
			message("\r\n");
			info("Release button to enter FEL mode\r\n");
			board_set_led(LED_BUTTON, 0);
			board_set_status(0);
			while (1) {
			};
		}
	};

	// Give enough time to load files
	sunxi_wdg_set(10);

	memset(&image, 0, sizeof(image_info_t));

	image.dtb_dest	  = (u8 *)CONFIG_DTB_LOAD_ADDR;
	image.kernel_dest = (u8 *)CONFIG_KERNEL_LOAD_ADDR;
	image.initrd_dest = (u8 *)CONFIG_INITRAMFS_LOAD_ADDR;

// Normal media boot
#if defined(CONFIG_BOOT_SDCARD) || defined(CONFIG_BOOT_MMC)

	if (sunxi_sdhci_init(&sdhci0) != 0) {
		fatal("SMHC: %s controller init failed\r\n", sdhci0.name);
	} else {
		info("SMHC: %s controller v%x initialized\r\n", sdhci0.name, sdhci0.reg->vers);
	}
	if (sdmmc_init(&card0, &sdhci0) != 0) {
#ifdef CONFIG_BOOT_SPINAND
		warning("SMHC: init failed, trying SPI\r\n");
		goto _spi;
#else
		fatal("SMHC: init failed\r\n");
	}
#endif

		if (mount_sdmmc() != 0) {
			fatal("SMHC: card mount failed\r\n");
		};

		if (wait >= 3000) {
			info("BOOT: forced recovery boot\r\n");
			slot_name = 'R';
		} else {
			slot_name = bootconf_get_slot(CONFIG_CONF_FILENAME);
		}

		// Convert to num for backup registers
		switch (slot_name) {
			case 'A':
				slot_num	  = 1;
				slot_filename = "A.cfg";
				break;
			case 'B':
				slot_num	  = 2;
				slot_filename = "B.cfg";
				break;
			case 'C':
				slot_num	  = 3;
				slot_filename = "C.cfg";
				break;
			case 'D':
				slot_num	  = 4;
				slot_filename = "D.cfg";
				break;
			case 'R':
			default:
				slot_num	  = 0;
				slot_filename = "R.cfg";
		}

		if (RTC_BKP_REG(slot_num) >= 2) {
			warning("BOOT: recovery boot after %u failures on slot %c\r\n", RTC_BKP_REG(slot_num), slot_name);
			slot_num	  = 0;
			slot_name	  = 'R';
			slot_filename = "R.cfg";
		} else {
			info("BOOT: standard boot on slot %c, failed %u times\r\n", slot_name, RTC_BKP_REG(slot_num));
		}

		if (bootconf_read_slot_data(slot_filename, &slot) != 0) {
			error("BOOT: failed to read slot config\r\n");
		}

		image.initrd_size = 0; // Set by load_sdmmc()

		strcpy(cmd_line, slot.kernel_cmd);
		image.filename		  = slot.kernel_filename;
		image.of_filename	  = slot.dtb_filename;
		image.initrd_filename = slot.initrd_filename;

#elif defined(CONFIG_BOOT_SPINAND)
	// Static slot configs for SPI
	image.initrd_size = 0; // disabled
	strcpy(cmd_line, CONFIG_DEFAULT_BOOT_CMD);

#else // 100% Fel boot
	info("BOOT: FEL mode\r\n");

	// This value is copied via xfel
	image.initrd_size = *(uint32_t *)(0x45000000);

	strcpy(cmd_line, CONFIG_DEFAULT_BOOT_CMD);
#endif

#if defined(CONFIG_BOOT_SDCARD) || defined(CONFIG_BOOT_MMC)
#ifdef CONFIG_BOOT_SPINAND
		if (load_sdmmc(&image) != 0) {
			unmount_sdmmc();
			warning("SMHC: loading failed, trying SPI\r\n");
		} else {
			unmount_sdmmc();
			goto _boot;
		}
#else
	if (load_sdmmc(&image) != 0) {
		fatal("SMHC: card load failed\r\n");
	}
	unmount_sdmmc();
#endif // CONFIG_SPI_NAND
#endif

#ifdef CONFIG_BOOT_SPINAND
	_spi:
		dma_init();
		dma_test();
		debug("SPI: init\r\n");
		if (sunxi_spi_init(&sunxi_spi0) != 0) {
			fatal("SPI: init failed\r\n");
		}

		if (load_spi_nand(&sunxi_spi0, &image) != 0) {
			fatal("SPI-NAND: loading failed\r\n");
		}

		sunxi_spi_disable(&sunxi_spi0);

#endif // CONFIG_SPI_NAND

	_boot:
		// The kernel will reset WDG
		sunxi_wdg_set(3);

		// Check if we should still be running
		if (!board_get_power_on()) {
			info("Waiting for power off...");
			board_set_status(0);
			while (1) { // wait for poweroff or watchdog
			};
		}

		if (boot_image_setup((unsigned char *)image.kernel_dest, &entry_point) != 0) {
			fatal("boot setup failed\r\n");
		}

		if (strlen(cmd_line) > 0) {
			debug("BOOT: args %s\r\n", cmd_line);
			if (fdt_update_bootargs(image.dtb_dest, cmd_line)) {
				error("BOOT: Failed to set boot args\r\n");
			}
		}

		if (fdt_update_memory(image.dtb_dest, SDRAM_BASE, memory_size)) {
			error("BOOT: Failed to set memory size\r\n");
		} else {
			debug("BOOT: Set memory size to 0x%x\r\n", memory_size);
		}

		if (image.initrd_dest) {
			if (fdt_update_initrd(image.dtb_dest, (uint32_t)image.initrd_dest,
								  (uint32_t)(image.initrd_dest + image.initrd_size))) {
				error("BOOT: Failed to set initrd address\r\n");
			} else {
				debug("BOOT: Set initrd to 0x%x-0x%x\r\n", image.initrd_dest, image.initrd_dest + image.initrd_size);
			}
		}

#if defined(CONFIG_BOOT_SDCARD) || defined(CONFIG_BOOT_MMC)
		// Increase boot count for this slot
		// It will be set to zero from Linux once boot is validated
		RTC_BKP_REG(slot_num) += 1;
#endif

		info("booting linux...\r\n");
		board_set_led(LED_BOARD, 0);
		board_set_led(LED_BUTTON, 1);

		arm32_mmu_disable();
		arm32_dcache_disable();
		arm32_icache_disable();
		arm32_interrupt_disable();

		kernel_entry = (void (*)(int, int, unsigned int))entry_point;
		kernel_entry(0, ~0, (unsigned int)image.dtb_dest);

		return 0;
	}
