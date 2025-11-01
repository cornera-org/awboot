#include "sunxi_security.h"
#include "io.h"
#include "sunxi_dma.h"
#include "reg-ccu.h"

#define SUNXI_SPC_BASE			 0x03008000u
#define SUNXI_SPC_PORT_STRIDE	 0x10u
#define SUNXI_SPC_DECPORT_SET(p) (SUNXI_SPC_BASE + 0x0004u + ((u32)(p) * SUNXI_SPC_PORT_STRIDE))
#define SUNXI_SPC_NUM_PORTS		 16u

#define SUNXI_CCU_SEC_SWITCH_REG	 (T113_CCU_BASE + 0x0f00u)
#define SUNXI_R_PRCM_BASE			 0x07010000u
#define SUNXI_R_PRCM_SEC_SWITCH_REG (SUNXI_R_PRCM_BASE + 0x0290u)

#define GIC_DIST_BASE 0x03021000u
#define GIC_CPU_BASE  0x03022000u
#define SUNXI_SCU_BASE 0x03000000u

static inline void writel_relaxed(u32 val, u32 addr)
{
	write32(addr, val);
}

void sunxi_security_setup(void)
{
	u32 i;

	for (i = 0; i < SUNXI_SPC_NUM_PORTS; ++i) {
		writel_relaxed(0xffffffffu, SUNXI_SPC_DECPORT_SET(i));
	}

	writel_relaxed(0x7u, SUNXI_CCU_SEC_SWITCH_REG);
	writel_relaxed(0x1u, SUNXI_R_PRCM_SEC_SWITCH_REG);
	writel_relaxed(0xffffu, SUNXI_DMA_BASE + 0x20u);

	writel_relaxed(read32(SUNXI_SCU_BASE) | 0x1u, SUNXI_SCU_BASE);
}

void sunxi_gic_set_nonsecure(void)
{
	u32 typer;
	u32 num_ints;
	u32 num_regs;
	u32 i;

	writel_relaxed(0x0u, GIC_DIST_BASE + 0x000u);

	typer	   = read32(GIC_DIST_BASE + 0x004u);
	num_ints   = ((typer & 0x1fu) + 1u) * 32u;
	num_regs   = (num_ints + 31u) / 32u;

	for (i = 0; i < num_regs; ++i) {
		writel_relaxed(0xffffffffu, GIC_DIST_BASE + 0x080u + (i * 4u));
	}

	writel_relaxed(0x0u, GIC_DIST_BASE + 0x000u);
	writel_relaxed(0x0u, GIC_CPU_BASE + 0x000u);
	writel_relaxed(0xffu, GIC_CPU_BASE + 0x004u);
	writel_relaxed(0x0u, GIC_CPU_BASE + 0x008u);
}
