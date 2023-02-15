#include "common.h"
#include "board.h"
#include "sunxi_gpio.h"
#include "sunxi_sdhci.h"
#include "sunxi_usart.h"
#include "sunxi_spi.h"
#include "sunxi_wdg.h"
#include "sdmmc.h"

sunxi_usart_t usart_dbg = {
	.id		 = 3,
	.gpio_tx = {GPIO_PIN(PORTB, 6), GPIO_PERIPH_MUX7},
	.gpio_rx = {GPIO_PIN(PORTB, 7), GPIO_PERIPH_MUX7},
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

static const gpio_t led_board = GPIO_PIN(PORTD, 19);
static const gpio_t led_btn	  = GPIO_PIN(PORTB, 5); // rev1.1 (PORTD, 18)
static const gpio_t btn		  = GPIO_PIN(PORTB, 4); // rev1.1 (PORTD, 11)

static const gpio_t status	 = GPIO_PIN(PORTD, 5);
static const gpio_t power_on = GPIO_PIN(PORTD, 6);

static const gpio_t phyaddr0 = GPIO_PIN(PORTE, 7);
static const gpio_t phyaddr1 = GPIO_PIN(PORTE, 0);
static const gpio_t phyaddr2 = GPIO_PIN(PORTE, 1);
static const gpio_t phyaddr3 = GPIO_PIN(PORTE, 2);
static const gpio_t phynrst	 = GPIO_PIN(PORTE, 11);

static void output_init(const gpio_t gpio)
{
	sunxi_gpio_init(gpio, GPIO_OUTPUT);
	sunxi_gpio_write(gpio, 0);
};

static void intput_init(const gpio_t gpio)
{
	sunxi_gpio_init(gpio, GPIO_INPUT);
};

void board_set_led(uint8_t num, uint8_t on)
{
	switch (num) {
		case LED_BOARD:
			sunxi_gpio_write(led_board, on);
			break;
		case LED_BUTTON:
			sunxi_gpio_write(led_btn, on);
			break;
		default:
			break;
	}
}

bool board_get_button()
{
	return !sunxi_gpio_read(btn);
}

bool board_get_power_on()
{
	return sunxi_gpio_read(power_on);
}

void board_set_status(bool on)
{
	sunxi_gpio_write(status, on);
}

void board_init()
{
	output_init(led_board);
	output_init(led_btn);
	intput_init(btn);

	output_init(status);
	intput_init(power_on);

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

	// Start PHY with addr 0
	mdelay(2);
	sunxi_gpio_write(phynrst, 1);

	board_set_led(LED_BOARD, 1);
	board_set_led(LED_BUTTON, 1);
	sunxi_usart_init(&usart_dbg, 115200);
	sunxi_wdg_init();

	// We need to set the pin to 1 for MCU to detect fallin edge later on.
	board_set_status(1);
}
