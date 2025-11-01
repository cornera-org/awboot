#include "common.h"
#include "sunxi_cpucfg.h"
#include "reg-r-ccu.h"
#include "reg-dram.h"
#include "barrier.h"
#include "debug.h"

#define setbits_le32(addr, set) write32((addr), read32((addr)) | (set))
#define clrbits_le32(addr, clear) write32((addr), read32((addr)) & ~(clear))
#define clrsetbits_le32(addr, clear, set) write32((addr), (read32((addr)) & ~(clear)) | (set))

#define SUNXI_CPUCFG_BASE        0x09010000U
#define CPU_CLUSTER_STRIDE       0x40U
#define CPU_CTRL_OFFSET          0x40U
#define CPU_STATUS_OFFSET        0x44U
#define CPU_RST_CTRL_OFFSET      0x48U
#define CPU_RVBAR_LO_OFFSET      0x4CU
#define CPU_RVBAR_HI_OFFSET      0x50U
#define CPU_PWR_CLAMP_OFFSET     0x54U

#define R_CPUCFG_PWR_CLAMP_OFFSET(cpu) (0x1b0U + ((cpu) * 4U))
#define R_CPUCFG_SUP_STAN_CTRL        (SUNXI_R_CPUCFG_BASE + 0x1d0U)

#define CPU_CTRL_RUN_REQUEST      BIT(0)
#define CPU_RST_CTRL_CORE_RESET   BIT(0)
#define CPU_RST_CTRL_DBG_RESET    BIT(1)
#define CPU_STATUS_RUNNING        BIT(0)

static inline uintptr_t cpucfg_reg(uint32_t cpu, uint32_t offset)
{
	return SUNXI_CPUCFG_BASE + (CPU_CLUSTER_STRIDE * cpu) + offset;
}

static void sunxi_cpucfg_release_clamp(uint32_t cpu)
{
	if (cpu == 0U)
		return;

	uintptr_t reg = SUNXI_R_CPUCFG_BASE + R_CPUCFG_PWR_CLAMP_OFFSET(cpu - 1U);
	uint32_t val = 0xffU;
	write32(reg, val);

	for (uint32_t attempts = 8U; attempts > 0U; --attempts) {
		val = read32(reg) & 0xffU;
		if (val == 0U)
			break;
		if (val <= 0x1fU) {
			write32(reg, 0U);
			break;
		}
		write32(reg, val - 0x1fU);
	}
}

static void sunxi_cpucfg_disable_super_standby(uint32_t cpu)
{
	uint32_t mask = (1U << (16U + cpu));
	if (read32(SUNXI_R_CPUCFG_BASE + SUNXI_R_CPUCFG_SUP_STAN_FLAG) & mask)
		clrbits_le32(R_CPUCFG_SUP_STAN_CTRL, mask);
}

void sunxi_cpucfg_init(void)
{
	setbits_le32(T113_R_CCU_BASE + CLK_BUS_R_CPUCFG, R_CCU_BIT_EN);
	setbits_le32(T113_R_CCU_BASE + CLK_BUS_R_CPUCFG, R_CCU_BIT_RST);
  trace("CPUCFG: clock enabled\r\n");
}

int sunxi_cpucfg_cpu_on(uint32_t cpu, uint32_t rvbar)
{
	if ((cpu >= SUNXI_CPU_MAX_COUNT) || (cpu == 0U))
		return -1;

	sunxi_cpucfg_disable_super_standby(cpu);
	sunxi_cpucfg_release_clamp(cpu);

	write32(cpucfg_reg(cpu, CPU_RVBAR_LO_OFFSET), rvbar);
	write32(cpucfg_reg(cpu, CPU_RVBAR_HI_OFFSET), 0U);

	/* Clear reset and request run */
	clrbits_le32(cpucfg_reg(cpu, CPU_RST_CTRL_OFFSET), CPU_RST_CTRL_CORE_RESET | CPU_RST_CTRL_DBG_RESET);
	setbits_le32(cpucfg_reg(cpu, CPU_CTRL_OFFSET), CPU_CTRL_RUN_REQUEST);

	dsb();
	__asm__ __volatile__("sev" : : : "memory");

	/* Wait briefly for CPU to report running (best-effort) */
	uint32_t timeout = 100000U;
	while (timeout--) {
		if (read32(cpucfg_reg(cpu, CPU_STATUS_OFFSET)) & CPU_STATUS_RUNNING)
			break;
	}

	return 0;
}

void sunxi_cpucfg_cpu_off(uint32_t cpu)
{
	if (cpu == 0U || cpu >= SUNXI_CPU_MAX_COUNT)
		return;

	clrbits_le32(cpucfg_reg(cpu, CPU_CTRL_OFFSET), CPU_CTRL_RUN_REQUEST);
	setbits_le32(cpucfg_reg(cpu, CPU_RST_CTRL_OFFSET), CPU_RST_CTRL_CORE_RESET | CPU_RST_CTRL_DBG_RESET);
}

bool sunxi_cpucfg_cpu_is_on(uint32_t cpu)
{
	if (cpu >= SUNXI_CPU_MAX_COUNT)
		return false;

	return (read32(cpucfg_reg(cpu, CPU_STATUS_OFFSET)) & CPU_STATUS_RUNNING) != 0U;
}
