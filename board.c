#include "main.h"
#include "board.h"
#include "sunxi_gpio.h"
#include "sunxi_sdhci.h"
#include "sunxi_usart.h"
#include "sunxi_spi.h"
#include "sdmmc.h"

sunxi_usart_t usart_dbg = {
	.id		 = 3,
	.gpio_tx = {GPIO_PIN(PORTB, 6), GPIO_PERIPH_MUX7},
	.gpio_rx = {GPIO_PIN(PORTB, 7), GPIO_PERIPH_MUX7},
};

sunxi_usart_t usart_mgmt = {
	.id		 = 5,
	.gpio_tx = {GPIO_PIN(PORTD, 5), GPIO_PERIPH_MUX5},
	.gpio_rx = {GPIO_PIN(PORTD, 6), GPIO_PERIPH_MUX5},
};

sunxi_spi_t sunxi_spi0 = {
	.base	   = 0x04025000,
	.id		   = 0,
	.clk_rate  = 25 * 1000 * 1000,
	.gpio_cs   = {GPIO_PIN(PORTC, 3), GPIO_PERIPH_MUX2},
	.gpio_sck  = {GPIO_PIN(PORTC, 2), GPIO_PERIPH_MUX2},
	.gpio_mosi = {GPIO_PIN(PORTC, 4), GPIO_PERIPH_MUX2},
	.gpio_miso = {GPIO_PIN(PORTC, 5), GPIO_PERIPH_MUX2},
	.gpio_wp   = {GPIO_PIN(PORTC, 6), GPIO_PERIPH_MUX2},
	.gpio_hold = {GPIO_PIN(PORTC, 7), GPIO_PERIPH_MUX2},
};

sdhci_t sdhci0 = {
	.name	   = "sdhci0",
	.reg	   = (sdhci_reg_t *)0x04020000,
	.voltage   = MMC_VDD_27_36,
	.width	   = MMC_BUS_WIDTH_4,
	.clock	   = MMC_CLK_50M,
	.removable = 0,
	.isspi	   = FALSE,
	.gpio_clk  = {GPIO_PIN(PORTF, 2), GPIO_PERIPH_MUX2},
	.gpio_cmd  = {GPIO_PIN(PORTF, 3), GPIO_PERIPH_MUX2},
	.gpio_d0   = {GPIO_PIN(PORTF, 1), GPIO_PERIPH_MUX2},
	.gpio_d1   = {GPIO_PIN(PORTF, 0), GPIO_PERIPH_MUX2},
	.gpio_d2   = {GPIO_PIN(PORTF, 5), GPIO_PERIPH_MUX2},
	.gpio_d3   = {GPIO_PIN(PORTF, 4), GPIO_PERIPH_MUX2},
};

const gpio_t led1 = GPIO_PIN(PORTD, 19);
const gpio_t led2 = GPIO_PIN(PORTB, 5);
const gpio_t hold = GPIO_PIN(PORTC, 7);
const gpio_t bus1 = GPIO_PIN(PORTD, 18);
const gpio_t bus2 = GPIO_PIN(PORTD, 15);

static const gpio_t phyaddr0 = GPIO_PIN(PORTE, 7);
static const gpio_t phyaddr1 = GPIO_PIN(PORTE, 0);
static const gpio_t phyaddr2 = GPIO_PIN(PORTE, 1);
static const gpio_t phyaddr3 = GPIO_PIN(PORTE, 2);
static const gpio_t phynrst	 = GPIO_PIN(PORTE, 11);

slot_t slots[3] = {
	{
		.dtb_filename	 = "core1-t113-v1-recovery.dtb",
		.kernel_filename = "zImage.R",
		.kernel_cmd		 = " rauc.slot=R",
		.initrd_filename = "rootfs.cpio.zst",
		.initrd_start	 = CONFIG_BOOT_INITRD_START,
		.initrd_end		 = CONFIG_BOOT_INITRD_END,
	 },
	{
		.dtb_filename	 = "core1-t113-v1-a.dtb",
		.kernel_filename = "zImage.A",
		.kernel_cmd		 = " root=/dev/mmcblk0p4 rootwait rauc.slot=A",
		.initrd_filename = "",
		.initrd_start	 = 0,
		.initrd_end		 = 0,
	 },
	{
		.dtb_filename	 = "core1-t113-v1-b.dtb",
		.kernel_filename = "zImage.B",
		.kernel_cmd		 = " root=/dev/mmcblk0p2 rootwait rauc.slot=B",
		.initrd_filename = "",
		.initrd_start	 = 0,
		.initrd_end		 = 0,
	 }
};

static void output_init(const gpio_t gpio)
{
	sunxi_gpio_init(gpio, GPIO_OUTPUT);
	sunxi_gpio_set_value(gpio, 0);
};

void board_set_led(uint8_t num, uint8_t on)
{
	switch (num) {
		case 1:
			sunxi_gpio_set_value(led1, on);
			break;
		case 2:
			sunxi_gpio_set_value(led2, on);
			break;
		default:
			break;
	}
}

void board_init()
{
	output_init(led1);
	output_init(led2);
	output_init(bus1);
	output_init(bus2);

	// Set eth phy address to 0
	output_init(phyaddr0);
	output_init(phyaddr1);
	output_init(phyaddr2);
	output_init(phyaddr3);
	output_init(phynrst);

	sunxi_gpio_set_pull(phyaddr0, GPIO_PULL_DOWN);
	sunxi_gpio_set_pull(phyaddr1, GPIO_PULL_DOWN);
	sunxi_gpio_set_pull(phyaddr2, GPIO_PULL_DOWN);
	sunxi_gpio_set_pull(phyaddr3, GPIO_PULL_DOWN);

	mdelay(5);
	sunxi_gpio_set_value(phynrst, 1);

	// sunxi_gpio_set_value(bus1, 1);
	// sunxi_gpio_set_value(bus2, 1);
	board_set_led(1, 1);
	board_set_led(2, 1);
	sunxi_usart_init(&usart_dbg, 115200);
	sunxi_usart_init(&usart_mgmt, 115200);
	// rtc_init();
}
