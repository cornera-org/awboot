#include "common.h"
#include "dram.h"
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
#include "psci.h"
#if CONFIG_BOOT_SPINAND
#include "sunxi_dma.h"
#endif

static bool range_in_sdram(uint32_t start, uint32_t size)
{
	const uint64_t base	 = (uint64_t)SDRAM_BASE;
	const uint64_t top	 = (uint64_t)SDRAM_TOP;
	const uint64_t start64 = (uint64_t)start;
	const uint64_t end	 = start64 + (uint64_t)size;

	return (start64 >= base) && (end <= top) && (end >= start64);
}

#if !CONFIG_BOOT_SDCARD && !CONFIG_BOOT_MMC && !CONFIG_BOOT_SPINAND
static inline uint32_t fel_mailbox_read(uint32_t addr)
{
	return *((volatile uint32_t *)addr);
}

static bool addr_in_sdram(uint32_t addr)
{
	const uint64_t base = (uint64_t)SDRAM_BASE;
	const uint64_t top	= (uint64_t)SDRAM_TOP;
	const uint64_t pos	= (uint64_t)addr;

	return (pos >= base) && (pos < top);
}

static void apply_fel_mailboxes(image_info_t *img)
{
	const uint32_t kernel_addr	 = fel_mailbox_read(CONFIG_MAIL_KERNEL_ADDR_ADDR);
	const uint32_t dtb_addr	  = fel_mailbox_read(CONFIG_MAIL_DTB_ADDR_ADDR);
	const uint32_t initrd_size	 = fel_mailbox_read(CONFIG_MAIL_INITRD_SIZE_ADDR);
	const uint32_t initrd_start   = fel_mailbox_read(CONFIG_MAIL_INITRD_START_ADDR);
	uint64_t	initrd_end_64 = (uint64_t)initrd_start + (uint64_t)initrd_size;

	if (kernel_addr != 0U) {
		if (!addr_in_sdram(kernel_addr)) {
			fatal("FEL: kernel addr 0x%08" PRIx32 " outside SDRAM\r\n", kernel_addr);
		}
		img->kernel_dest = (u8 *)kernel_addr;
	}

	if (dtb_addr != 0U) {
		if (!addr_in_sdram(dtb_addr)) {
			fatal("FEL: dtb addr 0x%08" PRIx32 " outside SDRAM\r\n", dtb_addr);
		}
		img->dtb_dest = (u8 *)dtb_addr;
	}

	if (initrd_size != 0U) {
		if (initrd_size > CONFIG_INITRAMFS_MAX_SIZE) {
			fatal("FEL: initrd size %" PRIu32 " exceeds max %" PRIu32 "\r\n",
			      initrd_size, (uint32_t)CONFIG_INITRAMFS_MAX_SIZE);
		}
		if (initrd_start == 0U) {
			fatal("FEL: initrd start missing\r\n");
		}
		if ((CONFIG_INITRD_ALIGNMENT != 0U) &&
			((initrd_start & (CONFIG_INITRD_ALIGNMENT - 1U)) != 0U)) {
			warning("FEL: initrd start 0x%08" PRIx32 " not %u-byte aligned\r\n",
				initrd_start, CONFIG_INITRD_ALIGNMENT);
		}
		if (!range_in_sdram(initrd_start, initrd_size)) {
			fatal("FEL: initrd 0x%08" PRIx32 "-0x%08" PRIx32 " outside SDRAM\r\n",
			      initrd_start, (uint32_t)initrd_end_64);
		}
		if ((dtb_addr != 0U) && (initrd_end_64 > (uint64_t)dtb_addr)) {
			fatal("FEL: initrd overlaps DTB (initrd end 0x%08" PRIx32 ", dtb @ 0x%08" PRIx32 ")\r\n",
			      (uint32_t)initrd_end_64, dtb_addr);
		}
		img->initrd_dest = (u8 *)initrd_start;
		img->initrd_size = initrd_size;
	}

	info("FEL: kernel@0x%08" PRIx32 " dtb@0x%08" PRIx32 " initrd@0x%08" PRIx32 " (%" PRIu32 " bytes)\r\n",
	     (uint32_t)(uintptr_t)img->kernel_dest,
	     (uint32_t)(uintptr_t)img->dtb_dest,
	     (uint32_t)(uintptr_t)img->initrd_dest,
	     (uint32_t)(uintptr_t)img->initrd_size);
}
#endif

image_info_t image;

static char cmd_line[128] = {0};

#if CONFIG_BOOT_SDCARD || CONFIG_BOOT_MMC
static char	  filename[16];
static slot_t slot;
#endif

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
	uint32_t	 wait		 = 0;
	uint8_t		 btn_led_val = false;

#if CONFIG_BOOT_SDCARD || CONFIG_BOOT_MMC
	uint32_t i		   = 0;
	char	 slot_name = 'R';
	uint8_t	 slot_num  = 0;
	uint8_t	 slot_boots[3];
	bool	 slot_valid[3];
	char	 slots[3]	   = {'R', 'A', 'B'};
	bool	 sd_boot_ready = false;
#endif

#if CONFIG_BOOT_SPINAND
	bool image_loaded = false;
#endif
	sunxi_clk_init();
	board_init();

	message("\r\n");
	info("AWBoot r%" PRIu32 " starting...\r\n", (u32)BUILD_REVISION);

  psci_init();

	uint32_t clk_fail = sunxi_clk_get_fail_addr();
	if (clk_fail != 0U) {
		warning("CLK: init timeout at 0x%08" PRIx32 "\r\n", clk_fail);
	}

	memory_size = sunxi_dram_init();
	info("DRAM init done: %" PRIu32 " MiB\r\n", memory_size >> 20);

#ifdef CONFIG_ENABLE_CPU_FREQ_DUMP
	sunxi_clk_dump();
#endif

	sunxi_wdg_set(10);

	// Check if we should be running
	if (!board_get_power_on()) {
		info("Waiting for power off...");
		board_set_status(0);
		while (1) { // wait for poweroff or watchdog
			mdelay(500);
			message(".");
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
		sunxi_wdg_set(3);
		if (wait > 10000) {
			message("\r\n");
			info("Release button to enter FEL mode\r\n");
			board_set_led(LED_BUTTON, 0);
			board_set_status(0);
			sunxi_wdg_set(10);
			while (1) {
				mdelay(100);
			};
		}
	};

	// Give enough time to load files
	sunxi_wdg_set(10);

	memset(&image, 0, sizeof(image_info_t));

	image.dtb_dest	  = (u8 *)CONFIG_DTB_LOAD_ADDR;
	image.kernel_dest = (u8 *)CONFIG_KERNEL_LOAD_ADDR;

// Normal media boot
#if CONFIG_BOOT_SDCARD || CONFIG_BOOT_MMC

	info("SMHC: init start\r\n");
	if (sunxi_sdhci_init(&sdhci2) != 0) {
		fatal("SMHC: %s controller init failed\r\n", sdhci2.name);
	} else {
		info("SMHC: %s controller v%x initialized\r\n", sdhci2.name, (unsigned int)sdhci2.reg->vers);
	}
	info("SMHC: detect start\r\n");
	if (sdmmc_init(&card0, &sdhci2) != 0) {
#if CONFIG_BOOT_SPINAND
		warning("SMHC: init failed, trying SPI\r\n");
		strcpy(cmd_line, CONFIG_DEFAULT_BOOT_CMD);
#else
		fatal("SMHC: init failed\r\n");
#endif
	} else {
		sdmmc_speed_test();
		info("SMHC: mount start\r\n");
		if (mount_sdmmc() != 0) {
			fatal("SMHC: card mount failed\r\n");
		}

		strcpy(filename + 1, ".state");

		// Check all slots for validity
		for (i = 0; i < sizeof(slot_boots); i++) {
			slot_boots[i] = RTC_BKP_REG(i);
			filename[0]	  = slots[i];
			if (slot_boots[i] > CONFIG_BOOT_MAX_TRIES) {
				info("BOOT: slot %u has %u failures, ignored\r\n", (unsigned int)i, (unsigned int)slot_boots[i]);
				slot_valid[i] = false;
			} else {
				if (bootconf_is_slot_state_good(filename)) {
					info("BOOT: slot %u valid\r\n", (unsigned int)i);
					slot_valid[i] = true;
				} else {
					info("BOOT: slot %u marked bad\r\n", (unsigned int)i);
					slot_valid[i] = false;
				}
			}
		}

		if (wait >= 3000) {
			info("BOOT: forced recovery boot\r\n");
			slot_name = 'R';
		} else {
			slot_name = bootconf_get_slot(CONFIG_CONF_FILENAME);
		}

		// Convert to num for backup registers
		switch (slot_name) {
			case 'A':
				slot_num = 1;
				break;
			case 'B':
				slot_num = 2;
				break;
			case 'R':
			default:
				slot_name = 'R';
				slot_num  = 0;
		}

		info("BOOT: selected slot %c\r\n", slot_name);

		if (!slot_valid[slot_num] && slot_name != 'R') {
			// Selected slot is A, slot B is valid
			if (slot_name == 'A' && slot_valid[2]) {
				slot_num  = 2;
				slot_name = 'B';
			}
			// Selected slot is B, slot A is valid
			else if (slot_name == 'B' && slot_valid[1]) {
				slot_num  = 1;
				slot_name = 'A';
			} else {
				// Recovery slot in last resort
				slot_num  = 0;
				slot_name = 'R';
			}
			warning("BOOT: switch to slot %c after %u failures\r\n", slot_name, (unsigned int)slot_boots[slot_num]);
		} else {
			info("BOOT: standard boot on slot %c\r\n", slot_name);
		}

		filename[0] = slot_name;
		strcpy(filename + 1, ".cfg");
		if (bootconf_load_slot_data(filename, &slot) != 0) {
			if (slot_name != 'R') {
				if (slot_name == 'A' && slot_valid[2]) { // try B
					filename[0] = 'B';
					error("BOOT: failed to load slot %c config, fallback to slot %c\r\n", slot_name, filename[0]);
					if (bootconf_load_slot_data(filename, &slot) != 0) {
						error("BOOT: failed to load slot %c config, fallback to slot %c\r\n", filename[0], 'R');
						if (bootconf_load_slot_data("R.cfg", &slot) != 0) {
							fatal("BOOT: failed to load recovery slot config\r\n");
						}
					}
				} else if (slot_name == 'B' && slot_valid[1]) { // try A
					filename[0] = 'A';
					error("BOOT: failed to load slot %c config, fallback to slot %c\r\n", slot_name, filename[0]);
					if (bootconf_load_slot_data(filename, &slot) != 0) {
						error("BOOT: failed to load slot %c config, fallback to slot %c\r\n", filename[0], 'R');
						if (bootconf_load_slot_data("R.cfg", &slot) != 0) {
							fatal("BOOT: failed to load recovery slot config\r\n");
						}
					}
				} else if (bootconf_load_slot_data("R.cfg", &slot) != 0) {
					fatal("BOOT: failed to load recovery slot config\r\n");
				}
			} else {
				fatal("BOOT: failed to load recovery slot config\r\n");
			}
		}

		image.initrd_size = 0; // Set by load_sdmmc()

		strcpy(cmd_line, slot.kernel_cmd);
		image.filename		  = slot.kernel_filename;
		image.of_filename	  = slot.dtb_filename;
		image.initrd_filename = slot.initrd_filename;
		sd_boot_ready		  = true;
	}

#elif CONFIG_BOOT_SPINAND
	// Static slot configs for SPI
	image.initrd_size = 0; // disabled
	strcpy(cmd_line, CONFIG_DEFAULT_BOOT_CMD);

#else // 100% Fel boot
	info("BOOT: FEL mode\r\n");

	apply_fel_mailboxes(&image);

	strcpy(cmd_line, CONFIG_DEFAULT_BOOT_CMD);
#endif

#if CONFIG_BOOT_SDCARD || CONFIG_BOOT_MMC
	if (sd_boot_ready) {
#if CONFIG_BOOT_SPINAND
		if (load_sdmmc(&image) != 0) {
			unmount_sdmmc();
			warning("SMHC: loading failed, trying SPI\r\n");
			sd_boot_ready = false;
			strcpy(cmd_line, CONFIG_DEFAULT_BOOT_CMD);
		} else {
			unmount_sdmmc();
			image_loaded = true;
		}
#else
		if (load_sdmmc(&image) != 0) {
			fatal("SMHC: card load failed\r\n");
		}
		unmount_sdmmc();
#endif
	}
#endif

#if CONFIG_BOOT_SPINAND
	if (!image_loaded) {
		if (cmd_line[0] == '\0') {
			strcpy(cmd_line, CONFIG_DEFAULT_BOOT_CMD);
		}
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
		image_loaded = true;
	}
#endif // CONFIG_SPI_NAND

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

#if !CONFIG_BOOT_SPINAND && !CONFIG_BOOT_SDCARD && !CONFIG_BOOT_MMC
	cmd_line[0] = '\0'; 
#endif

	if (strlen(cmd_line) > 0) {
		debug("BOOT: args %s\r\n", cmd_line);
		if (fdt_update_bootargs(image.dtb_dest, cmd_line)) {
			fatal("BOOT: Failed to set boot args\r\n");
		}
	}

	if (fdt_update_memory(image.dtb_dest, SDRAM_BASE, memory_size)) {
		fatal("BOOT: Failed to set memory size\r\n");
	} else {
		debug("BOOT: Set memory size to 0x%" PRIx32 "\r\n", memory_size);
	}

	if ((image.initrd_size > 0U) && (image.initrd_dest == NULL)) {
		image.initrd_dest = (u8 *)(SDRAM_TOP - image.initrd_size);
	}

	if (image.initrd_size > 0U) {
		if (image.initrd_size > CONFIG_INITRAMFS_MAX_SIZE) {
			fatal("BOOT: bad initrd size (%u)\r\n", image.initrd_size);
		}

		const uintptr_t initrd_start = (uintptr_t)image.initrd_dest;
		const uintptr_t initrd_end   = initrd_start + (uintptr_t)image.initrd_size;

		if (!range_in_sdram((uint32_t)initrd_start, image.initrd_size)) {
			fatal("BOOT: initrd range 0x%08" PRIx32 "-0x%08" PRIx32 " invalid\r\n",
			      (uint32_t)initrd_start, (uint32_t)initrd_end);
		}

		if ((CONFIG_INITRD_ALIGNMENT != 0U) &&
			((initrd_start & (CONFIG_INITRD_ALIGNMENT - 1U)) != 0U)) {
			warning("BOOT: initrd start 0x%08" PRIx32 " not %u-byte aligned\r\n",
				(uint32_t)initrd_start, CONFIG_INITRD_ALIGNMENT);
		}

		if (fdt_update_initrd(image.dtb_dest, (uint32_t)image.initrd_dest,
					(uint32_t)(image.initrd_dest + image.initrd_size))) {
			fatal("BOOT: Failed to set initrd address\r\n");
		} else {
			debug("BOOT: Set initrd to 0x%08" PRIx32 "->0x%08" PRIx32 "\r\n",
			      (uint32_t)image.initrd_dest,
			      (uint32_t)(image.initrd_dest + image.initrd_size));
		}
	} else {
		image.initrd_dest = NULL;
	}

#if CONFIG_BOOT_SDCARD || CONFIG_BOOT_MMC
	if (sd_boot_ready) {
		// Increase boot count for this slot
		// It will be set to zero from Linux once boot is validated
		RTC_BKP_REG(slot_num) += 1;
	}
#endif

	info("booting linux...\r\n");
	board_set_led(LED_BOARD, 0);

	arm32_mmu_disable();
	arm32_dcache_disable();
	arm32_icache_disable();
	arm32_interrupt_disable();

  debug("BOOT: kernel entry 0x%08" PRIx32 " dtb 0x%08" PRIx32 "\r\n",
        (uint32_t)entry_point, (uint32_t)(uintptr_t)image.dtb_dest);
	psci_enter_non_secure((uint32_t)entry_point, 0U, ~0U, (uint32_t)image.dtb_dest);
	fatal("psci_enter_non_secure returned\r\n");

	return 0;
}
