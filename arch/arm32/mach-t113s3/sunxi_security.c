#include <inttypes.h>

#include "sunxi_security.h"
#include "io.h"
#include "sunxi_dma.h"
#include "reg-ccu.h"
#include "sunxi_spc.h"
#include "debug.h"
#include "sunxi_cpucfg.h"

#define SUNXI_CCU_SEC_SWITCH_REG	 (T113_CCU_BASE + 0x0f00u)
#define SUNXI_R_PRCM_SEC_SWITCH_REG (SUNXI_R_PRCM_BASE + 0x0290u)

#define GIC_DIST_BASE 0x03021000u
#define GIC_CPU_BASE  0x03022000u

#define SUNXI_DMA_SEC_REG (SUNXI_DMA_BASE + 0x20u)

static inline void writel_relaxed(u32 val, u32 addr)
{
	write32(addr, val);
}

void sunxi_security_setup(void)
{
	u32 i;

	/*
	 * Mirror the NCAT2 security hand-off used by TF-A/U-Boot. The T113-S3
	 * only exposes the unified SPC block (no legacy TZPC decport windows), so
	 * program every decode port to allow non-secure access.
	 */
	for (i = 0; i < SUNXI_SPC_NUM_PORTS; ++i) {
		writel_relaxed(0xffffffffu, SUNXI_SPC_DECPORT_SET_REG(i));
	}

	writel_relaxed(0x7u, SUNXI_CCU_SEC_SWITCH_REG);
	writel_relaxed(0x1u, SUNXI_R_PRCM_SEC_SWITCH_REG);
	writel_relaxed(0xffffu, SUNXI_DMA_SEC_REG);

	writel_relaxed(read32(SYS_CONTROL_REG_BASE) | 0x1u, SYS_CONTROL_REG_BASE);

	for (i = 0; i < SUNXI_SPC_NUM_PORTS; ++i) {
		uint32_t dec = read32(SUNXI_SPC_DECPORT_STA_REG(i));
		if (dec != 0xffffffffu) {
			fatal("SPC: port %" PRIu32 " still secure (0x%08" PRIx32 ")\r\n",
			      i, dec);
		}
	}

	uint32_t ccu_chk = read32(SUNXI_CCU_SEC_SWITCH_REG) & 0x7u;
	if (ccu_chk != 0x7u) {
		fatal("SPC: CCU security switch mismatch (0x%08" PRIx32 ")\r\n", ccu_chk);
	}

	uint32_t prcm_chk = read32(SUNXI_R_PRCM_SEC_SWITCH_REG) & 0x1u;
	if (prcm_chk != 0x1u) {
		fatal("SPC: PRCM security switch mismatch (0x%08" PRIx32 ")\r\n", prcm_chk);
	}

	uint32_t dma_chk = read32(SUNXI_DMA_SEC_REG);
	if ((dma_chk & 0xffffu) != 0xffffu) {
		fatal("SPC: DMA security mismatch (0x%08" PRIx32 ")\r\n", dma_chk);
	}

	debug("SPC: NS hand-off confirmed (ccu=0x%08" PRIx32
	      " prcm=0x%08" PRIx32 " dma=0x%08" PRIx32 ")\r\n",
	      ccu_chk, prcm_chk, dma_chk);
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
