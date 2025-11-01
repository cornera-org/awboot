#include "common.h"
#include "sunxi_gic.h"
#include "io.h"
#include "debug.h"

void sunxi_gic_init_non_secure(void)
{
	/* Disable interfaces before reconfiguration */
	write32(SUNXI_GICC_BASE + GICC_CTLR, 0U);
	write32(SUNXI_GICD_BASE + GICD_CTLR, 0U);

	uint32_t typer   = read32(SUNXI_GICD_BASE + GICD_TYPER);
	uint32_t nirqs   = ((typer & 0x1fU) + 1U) * 32U;
	uint32_t groups  = nirqs / 32U;

	for (uint32_t n = 0U; n < groups; ++n) {
		write32(SUNXI_GICD_BASE + GICD_IGROUPR(n), 0xffffffffU);
		write32(SUNXI_GICD_BASE + GICD_ICENABLER(n), 0xffffffffU);
	}

	write32(SUNXI_GICC_BASE + GICC_PMR, 0xffU);
	write32(SUNXI_GICC_BASE + GICC_BPR, 0x0U);

	/* Enable group 1 (Non-secure) signaling */
	write32(SUNXI_GICC_BASE + GICC_CTLR, (1U << 0) | (1U << 1));
	write32(SUNXI_GICD_BASE + GICD_CTLR, (1U << 0) | (1U << 1));

	debug("GIC: configured for non-secure world\r\n");
}

void sunxi_gic_disable(void)
{
    uint32_t typer = read32(SUNXI_GICD_BASE + GICD_TYPER);
    uint32_t nirqs = ((typer & 0x1fU) + 1U) * 32U;
    uint32_t groups = (nirqs + 31U) / 32U;

    /* Drain any pending interrupts */
    while (1) {
        uint32_t iar = read32(SUNXI_GICC_BASE + GICC_IAR);
        uint32_t id = iar & 0x3ffU;
        if (id >= 1022U)
            break;
        write32(SUNXI_GICC_BASE + GICC_EOIR, iar);
    }

    write32(SUNXI_GICC_BASE + GICC_CTLR, 0U);
    write32(SUNXI_GICD_BASE + GICD_CTLR, 0U);

    for (uint32_t n = 0U; n < groups; ++n) {
        write32(SUNXI_GICD_BASE + GICD_ICENABLER(n), 0xffffffffU);
        write32(SUNXI_GICD_BASE + GICD_ICPENDR(n), 0xffffffffU);
        write32(SUNXI_GICD_BASE + GICD_ICACTIVER(n), 0xffffffffU);
    }

    debug("GIC: disabled before non-secure hand-off\r\n");
}
