#include "main.h"
#include "board.h"
#include "sunxi_gpio.h"
#include "sunxi_sdhci.h"
#include "sunxi_usart.h"
#include "sunxi_spi.h"
#include "sdmmc.h"


#define UART_BASE(id) ((uint32_t)(0x02500000 + (0x400 * (id))))

const sunxi_usart_t usart_dbg = {
	.base	 = UART_BASE(3),
	.id		 = 3,
	.gpio_tx = {GPIO_PIN(PORTB, 6), GPIO_PERIPH_MUX7},
	.gpio_rx = {GPIO_PIN(PORTB, 7), GPIO_PERIPH_MUX7},
};

const sunxi_usart_t usart_mgmt = {
	.base	 = UART_BASE(5),
	.id		 = 5,
	.gpio_tx = {GPIO_PIN(PORTD, 5), GPIO_PERIPH_MUX5},
	.gpio_rx = {GPIO_PIN(PORTD, 6), GPIO_PERIPH_MUX5},
};

sunxi_spi_t sunxi_spi0 = {
	.base	   = 0x04025000,
	.id		   = 0,
	.clk_rate  = 100 * 1000 * 1000,
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

static const gpio_t led1 = GPIO_PIN(PORTD, 19);
static const gpio_t led2 = GPIO_PIN(PORTB, 5);
static const gpio_t hold = GPIO_PIN(PORTC, 7);

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
	sunxi_gpio_init(led1, GPIO_OUTPUT);
	sunxi_gpio_init(led2, GPIO_OUTPUT);
	sunxi_gpio_init(hold, GPIO_OUTPUT);
	board_set_led(1, 1);
	board_set_led(2, 1);
	sunxi_gpio_set_value(hold, 1);
	sunxi_usart_init(&usart_dbg);
	sunxi_usart_init(&usart_mgmt);
}
