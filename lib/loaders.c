#include "common.h"
#include "loaders.h"
#include "board.h"

#if defined(CONFIG_BOOT_SDCARD) || defined(CONFIG_BOOT_MMC)

FATFS fs;

int mount_sdmmc()
{
	FRESULT fret;

	/* mount fs */
	fret = f_mount(&fs, "", 1);
	if (fret != FR_OK) {
		error("FATFS: mount error: %d\r\n", fret);
		return -1;
	} else {
		debug("FATFS: mount OK\r\n");
	}

	return 0;
}

void unmount_sdmmc(void)
{
	FRESULT fret;

	/* umount fs */
	fret = f_mount(0, "", 0);
	if (fret != FR_OK) {
		error("FATFS: unmount error %d\r\n", fret);
	} else {
		debug("FATFS: unmount OK\r\n");
	}
}

int read_file(const char *filename, uint8_t *dest)
{
	FIL		 file;
	UINT	 bytes_to_read = 0x1000000; // 16MB
	UINT	 bytes_read;
	UINT	 total_read = 0;
	int		 ret;
	FRESULT	 fret;
	uint32_t start, time;

	if (!filename) {
		error("FATFS: empty filename\r\n");
		return -1;
	}

	fret = f_open(&file, filename, FA_OPEN_EXISTING | FA_READ);
	if (fret != FR_OK) {
		error("FATFS: file open: [%s]: error %d\r\n", filename, fret);
		return -1;
	}

	start = time_ms();

	do {
		bytes_read = 0;
		fret	   = f_read(&file, (void *)(dest), bytes_to_read, &bytes_read);
		if (fret != FR_OK) {
			error("FATFS: file read error %d\r\n", fret);
			ret = -1;
			goto close;
		}
		dest += bytes_to_read;
		total_read += bytes_read;
	} while (bytes_read >= bytes_to_read && fret == FR_OK);

	ret = (int)total_read;

	time = time_ms() - start + 1;

	debug("FATFS: %s read in %ums at %.2fMB/S\r\n", filename, time, (f32)(total_read / time) / 1024.0f);

close:
	fret = f_close(&file);

	return ret;
}

int load_sdmmc(image_info_t *image)
{
	int ret;
	u32 start;

#if defined(CONFIG_SDMMC_SPEED_TEST_SIZE) && LOG_LEVEL >= LOG_DEBUG
	u32 test_time;
	start = time_ms();
	sdmmc_blk_read(&card0, (u8 *)(SDRAM_BASE), 0, CONFIG_SDMMC_SPEED_TEST_SIZE);
	test_time = time_ms() - start;
	debug("SDMMC: speedtest %uKB in %ums at %uKB/S\r\n", (CONFIG_SDMMC_SPEED_TEST_SIZE * 512) / 1024, test_time,
		  (CONFIG_SDMMC_SPEED_TEST_SIZE * 512) / test_time);
#endif // SDMMC_SPEED_TEST

	start = time_ms();

	info("FATFS: read %s addr=%x\r\n", image->of_filename, (unsigned int)image->dtb_dest);
	ret = read_file(image->of_filename, image->dtb_dest);
	if (ret <= 0)
		return ret;
	image->kernel_size = ret;

	info("FATFS: read %s addr=%x\r\n", image->filename, (unsigned int)image->kernel_dest);
	ret = read_file(image->filename, image->kernel_dest);
	if (ret <= 0)
		return ret;
	image->dtb_size = ret;

	if (image->initrd_filename && image->initrd_dest) {
		if (strlen(image->initrd_filename)) {
			info("FATFS: read %s addr=%x\r\n", image->initrd_filename, (unsigned int)image->initrd_dest);
			ret = read_file(image->initrd_filename, image->initrd_dest);
			if (ret <= 0)
				return ret;
			image->initrd_size = ret;
		}
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
