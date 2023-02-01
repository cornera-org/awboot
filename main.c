#include "main.h"
#include "fdt.h"
#include "ff.h"
#include "sunxi_gpio.h"
#include "sunxi_sdhci.h"
#include "sunxi_spi.h"
#include "sunxi_clk.h"
#include "sunxi_dma.h"
#include "sdmmc.h"
#include "arm32.h"
#include "reg-ccu.h"
#include "debug.h"
#include "board.h"
#include "barrier.h"
#include "mgmt.h"

image_info_t image;
extern u32	 _start;
extern u32	 __spl_start;
extern u32	 __spl_end;
extern u32	 __spl_size;
extern u32	 __stack_srv_start;
extern u32	 __stack_srv_end;
extern u32	 __stack_ddr_srv_start;
extern u32	 __stack_ddr_srv_end;

/* Linux zImage Header */
#define LINUX_ZIMAGE_MAGIC 0x016f2818
typedef struct {
	unsigned int code[9];
	unsigned int magic;
	unsigned int start;
	unsigned int end;
} linux_zimage_header_t;

uint8_t uartbuf[40];
char	cmd_line[128];

#if defined(CONFIG_BOOT_SDCARD) || defined(CONFIG_BOOT_MMC)

static int fatfs_loadimage(char *filename, BYTE *dest)
{
	FIL		 file;
	UINT	 bytes_to_read = 0x1000000; // 16MB
	UINT	 bytes_read;
	UINT	 total_read = 0;
	FRESULT	 fret;
	int		 ret;
	uint32_t start, time;

	fret = f_open(&file, filename, FA_OPEN_EXISTING | FA_READ);
	if (fret != FR_OK) {
		error("FATFS: open, filename: [%s]: error %d\r\n", filename, fret);
		return -1;
	}

	start = time_ms();

	do {
		bytes_read = 0;
		fret	   = f_read(&file, (void *)(dest), bytes_to_read, &bytes_read);
		if (fret != FR_OK) {
			error("FATFS: read error %d\r\n", fret);
			ret = -1;
			goto close;
		}
		dest += bytes_to_read;
		total_read += bytes_read;
	} while (bytes_read >= bytes_to_read && fret == FR_OK);

	time = time_ms() - start + 1;

	debug("FATFS: read in %ums at %.2fMB/S\r\n", time, (f32)(total_read / time) / 1024.0f);

close:
	fret = f_close(&file);

	return (int)total_read;
}

static int load_sdcard(image_info_t *image)
{
	FATFS	fs;
	FRESULT fret;
	int		ret;
	u32		start;

#if defined(CONFIG_SDMMC_SPEED_TEST_SIZE) && LOG_LEVEL >= LOG_DEBUG
	u32 test_time;
	start = time_ms();
	sdmmc_blk_read(&card0, (u8 *)(SDRAM_BASE), 0, CONFIG_SDMMC_SPEED_TEST_SIZE);
	test_time = time_ms() - start;
	debug("SDMMC: speedtest %uKB in %ums at %uKB/S\r\n", (CONFIG_SDMMC_SPEED_TEST_SIZE * 512) / 1024, test_time,
		  (CONFIG_SDMMC_SPEED_TEST_SIZE * 512) / test_time);
#endif // SDMMC_SPEED_TEST

	start = time_ms();
	/* mount fs */
	fret = f_mount(&fs, "", 1);
	if (fret != FR_OK) {
		error("FATFS: mount error: %d\r\n", fret);
		return -1;
	} else {
		debug("FATFS: mount OK\r\n");
	}

	info("FATFS: read %s addr=%x\r\n", image->of_filename, (unsigned int)image->dtb_dest);
	ret = fatfs_loadimage(image->of_filename, image->dtb_dest);
	if (ret <= 0)
		return ret;
	image->kernel_size = ret;

	info("FATFS: read %s addr=%x\r\n", image->filename, (unsigned int)image->kernel_dest);
	ret = fatfs_loadimage(image->filename, image->kernel_dest);
	if (ret <= 0)
		return ret;
	image->dtb_size = ret;

	if (strlen(image->initrd_filename) && image->initrd_dest) {
		info("FATFS: read %s addr=%x\r\n", image->initrd_filename, (unsigned int)image->initrd_dest);
		ret = fatfs_loadimage(image->initrd_filename, image->initrd_dest);
		if (ret <= 0)
			return ret;
		image->initrd_size = ret;
	}

	/* umount fs */
	fret = f_mount(0, "", 0);
	if (fret != FR_OK) {
		error("FATFS: unmount error %d\r\n", fret);
		return -1;
	} else {
		debug("FATFS: unmount OK\r\n");
	}
	debug("FATFS: done in %ums\r\n", time_ms() - start);

	return 0;
}

#endif

#ifdef CONFIG_BOOT_SPINAND
int load_spi_nand(sunxi_spi_t *spi, image_info_t *image)
{
	linux_zimage_header_t *hdr;
	unsigned int		   size;
	uint64_t			   start, time;

	if (spi_nand_detect(spi) != 0)
		return -1;

	/* get dtb size and read */
	spi_nand_read(spi, image->dtb_dest, CONFIG_SPINAND_DTB_ADDR, (uint32_t)sizeof(boot_param_header_t));
	if (of_get_magic_number(image->dtb_dest) != OF_DT_MAGIC) {
		error("SPI-NAND: DTB verification failed\r\n");
		return -1;
	}

	size = fdt_get_total_size(image->dtb_dest);
	debug("SPI-NAND: dt blob: Copy from 0x%08x to 0x%08lx size:0x%08x\r\n", CONFIG_SPINAND_DTB_ADDR,
		  (uint32_t)image->dtb_dest, size);
	start = time_us();
	spi_nand_read(spi, image->dtb_dest, CONFIG_SPINAND_DTB_ADDR, (uint32_t)size);
	time = time_us() - start;
	info("SPI-NAND: read dt blob of size %u at %.2fMB/S\r\n", size, (f32)(size / time));

	/* get kernel size and read */
	spi_nand_read(spi, image->kernel_dest, CONFIG_SPINAND_KERNEL_ADDR, (uint32_t)sizeof(linux_zimage_header_t));
	hdr = (linux_zimage_header_t *)image->kernel_dest;
	if (hdr->magic != LINUX_ZIMAGE_MAGIC) {
		debug("SPI-NAND: zImage verification failed\r\n");
		return -1;
	}
	size = hdr->end - hdr->start;
	debug("SPI-NAND: Image: Copy from 0x%08x to 0x%08lx size:0x%08x\r\n", CONFIG_SPINAND_KERNEL_ADDR,
		  (uint32_t)image->kernel_dest, size);
	start = time_us();
	spi_nand_read(spi, image->kernel_dest, CONFIG_SPINAND_KERNEL_ADDR, (uint32_t)size);
	time = time_us() - start;
	info("SPI-NAND: read Image of size %u at %.2fMB/S\r\n", size, (f32)(size / time));

	return 0;
}
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
	uint32_t memory_size;
	board_init();
	sunxi_clk_init();

	message("\r\n");
	info("AWBoot r%u starting...\r\n", (u32)BUILD_REVISION);

	memory_size = sunxi_dram_init();

	unsigned int entry_point = 0;
	void (*kernel_entry)(int zero, int arch, unsigned int params);

#ifdef CONFIG_ENABLE_CPU_FREQ_DUMP
	sunxi_clk_dump();
#endif

	memset(&image, 0, sizeof(image_info_t));

	image.dtb_dest	  = (u8 *)CONFIG_DTB_LOAD_ADDR;
	image.kernel_dest = (u8 *)CONFIG_KERNEL_LOAD_ADDR;

// Normal media boot
#if defined(CONFIG_BOOT_SDCARD) || defined(CONFIG_BOOT_MMC) || defined(CONFIG_BOOT_SPINAND)
	info("BOOT: standard mode\r\n");

	uint8_t slot = mgmt_get_slot(&usart_mgmt, uartbuf);
	info("MGMT: boot slot %u\r\n", slot);

	image.initrd_dest = slots[slot].initrd_start;

	strcpy(cmd_line, CONFIG_BOOT_CMD);

	strcat(cmd_line, slots[slot].kernel_cmd);
	strcpy(image.filename, slots[slot].kernel_filename);
	strcpy(image.of_filename, slots[slot].dtb_filename);
	strcpy(image.initrd_filename, slots[slot].initrd_filename);

#else // 100% Fel boot
	info("BOOT: FEL mode\r\n");

	image.initrd_dest = CONFIG_BOOT_INITRD_START;
	image.initrd_size = CONFIG_BOOT_INITRD_END - CONFIG_BOOT_INITRD_START; // Default value of 15MB, since we don't know

	strcpy(cmd_line, CONFIG_BOOT_CMD);

#endif

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
#endif
	}

#ifdef CONFIG_BOOT_SPINAND
	if (load_sdcard(&image) != 0) {
		warning("SMHC: loading failed, trying SPI\r\n");
	} else {
		goto _boot;
	}
#else
	if (load_sdcard(&image) != 0) {
		fatal("SMHC: card load failed\r\n");
	} else {
		goto _boot;
	}
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
	if (boot_image_setup((unsigned char *)image.kernel_dest, &entry_point)) {
		fatal("boot setup failed\r\n");
	}

	if (strlen(cmd_line) > 0) {
		debug("BOOT: args %s\r\n", cmd_line);
		if (fdt_update_bootargs(image.dtb_dest, cmd_line)) {
			error("BOOT: Failed to update boot args\r\n");
		}
	}

	if (fdt_update_memory(image.dtb_dest, SDRAM_BASE, memory_size)) {
		error("BOOT: Failed to update memory size\r\n");
	} else {
		debug("BOOT: Set memory size to 0x%x\r\n", memory_size);
	}

	if (image.initrd_dest) {
		if (fdt_update_initrd(image.dtb_dest, image.initrd_dest, image.initrd_dest + image.initrd_size)) {
			error("BOOT: Failed to initrd address\r\n");
		} else {
			debug("BOOT: Set initrd to 0x%x-0x%x\r\n", image.initrd_dest, image.initrd_dest + image.initrd_size);
		}
	}

#if defined(CONFIG_BOOT_SDCARD) || defined(CONFIG_BOOT_MMC)
	if (slot != 0 && mgmt_boot(&usart_mgmt, uartbuf) != 0) {
		warning("MGMT: boot command error\r\n");
	}
#endif

	info("booting linux...\r\n");
	board_set_led(1, 0);
	board_set_led(2, 0);

	arm32_mmu_disable();
	arm32_dcache_disable();
	arm32_icache_disable();
	arm32_interrupt_disable();

	kernel_entry = (void (*)(int, int, unsigned int))entry_point;
	kernel_entry(0, ~0, (unsigned int)image.dtb_dest);

	return 0;
}
