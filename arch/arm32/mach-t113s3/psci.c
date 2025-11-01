#include "common.h"
#include "psci.h"
#include "barrier.h"
#include "sunxi_wdg.h"

#define SUNXI_CPUCFG_BASE        0x08100000u
#define SUNXI_C0_CPUXCFG_BASE    0x09010000u
#define SUNXI_R_CPUCFG_BASE      0x07000400u
#define SUNXI_R_CCU_BASE         0x07010000u

#define SUNXI_CPUCFG_RST_CTRL_REG(cluster)     (SUNXI_C0_CPUXCFG_BASE + 0x0000u + (cluster) * 4u)
#define SUNXI_CPUCFG_CLS_CTRL_REG0(cluster)    (SUNXI_C0_CPUXCFG_BASE + 0x0010u + (cluster) * 0x10u)
#define SUNXI_CPUCFG_CLS_CTRL_REG1(cluster)    (SUNXI_C0_CPUXCFG_BASE + 0x0014u + (cluster) * 0x10u)
#define SUNXI_CPUCFG_CACHE_CFG_REG             (SUNXI_C0_CPUXCFG_BASE + 0x0024u)
#define SUNXI_CPUCFG_DBG_REG0                  (SUNXI_C0_CPUXCFG_BASE + 0x00c0u)

#define SUNXI_CPUCFG_GEN_CTRL_REG0             (SUNXI_CPUCFG_BASE   + 0x0000u)
#define SUNXI_CPUCFG_PRIV0_REG                 (SUNXI_C0_CPUXCFG_BASE + 0x01a4u)

#define SUNXI_AARCH_CTRL_BIT(core)             (1u << (4u + (core)))

#define SUNXI_POWERON_RST_REG(cluster)         (SUNXI_R_CPUCFG_BASE + 0x0040u + (cluster) * 4u)
#define SUNXI_POWEROFF_GATING_REG(cluster)     (SUNXI_R_CPUCFG_BASE + 0x0044u + (cluster) * 4u)
#define SUNXI_CPU_POWER_CLAMP_REG(cluster, core) \
	(SUNXI_R_CPUCFG_BASE + 0x0050u + (cluster) * 0x10u + (core) * 4u)
#define SUNXI_CPU_SOFT_ENTRY_REG              (SUNXI_R_CPUCFG_BASE + 0x01a4u)
#define SUNXI_PRCM_CPU_SOFT_ENTRY_REG         (SUNXI_R_CCU_BASE + 0x0164u)
#define SUNXI_R_CPUCFG_CLK_REG                (SUNXI_R_CCU_BASE + 0x022cu)
#define SUNXI_R_CPUCFG_CLK_GATE               BIT(0)
#define SUNXI_R_CPUCFG_CLK_RST                BIT(16)

/* Secondary CPU entry trampoline written into SRAM B */
#define SUNXI_SRAM_B_BASE        0x00030000u
#define SUNXI_SRAM_B_TRAMPOLINE  (SUNXI_SRAM_B_BASE)

#define TRAMP_OPCODE(idx)       (*(volatile uint32_t *)(SUNXI_SRAM_B_TRAMPOLINE + (idx) * sizeof(uint32_t)))

#define PSCI_VERSION_1_0		 0x00010000u

#define PSCI_RET_SUCCESS		 0
#define PSCI_RET_NOT_SUPPORTED		-1
#define PSCI_RET_INVALID_PARAMETERS	-2
#define PSCI_RET_DENIED			-3
#define PSCI_RET_ALREADY_ON		-4
#define PSCI_RET_INTERNAL_FAILURE	-6

#define PSCI_AFFINITY_ON		 0
#define PSCI_AFFINITY_OFF		 1
#define PSCI_AFFINITY_ON_PENDING	 2

#define PSCI_CPU_COUNT			 2u

#define PSCI_0_2_FN_PSCI_VERSION	0x84000000u
#define PSCI_0_2_FN_CPU_SUSPEND		0x84000001u
#define PSCI_0_2_FN_CPU_OFF		0x84000002u
#define PSCI_0_2_FN_CPU_ON		0x84000003u
#define PSCI_0_2_FN_AFFINITY_INFO	0x84000004u
#define PSCI_0_2_FN_MIGRATE_INFO_TYPE 0x84000006u
#define PSCI_0_2_FN_SYSTEM_OFF		0x84000008u
#define PSCI_0_2_FN_SYSTEM_RESET	0x84000009u
#define PSCI_1_0_FN_PSCI_FEATURES	0x8400000au

#define PSCI_0_2_FN64_CPU_SUSPEND	0xc4000001u
#define PSCI_0_2_FN64_CPU_ON		0xc4000003u
#define PSCI_0_2_FN64_AFFINITY_INFO	0xc4000004u
#define PSCI_0_2_FN64_MIGRATE_INFO_TYPE 0xc4000006u
#define PSCI_0_2_FN64_SYSTEM_OFF	0xc4000008u
#define PSCI_0_2_FN64_SYSTEM_RESET	0xc4000009u
#define PSCI_1_0_FN64_PSCI_FEATURES	0xc400000au

extern uint32_t psci_monitor_vector_table;

static volatile uint32_t psci_smc_count;

static volatile uint32_t psci_cpu_states[PSCI_CPU_COUNT] = {
	PSCI_AFFINITY_ON,
	PSCI_AFFINITY_OFF,
};

static inline uint32_t mpidr_read(void)
{
	uint32_t mpidr;
	__asm__ __volatile__("mrc p15, 0, %0, c0, c0, 5" : "=r"(mpidr));
	return mpidr;
}

static inline uint32_t readl(uint32_t addr)
{
	return read32(addr);
}

static inline void writel(uint32_t val, uint32_t addr)
{
	write32(addr, val);
}

static inline void setbits(uint32_t addr, uint32_t mask)
{
	writel(readl(addr) | mask, addr);
}

static inline void clrbits(uint32_t addr, uint32_t mask)
{
	writel(readl(addr) & ~mask, addr);
}

static void sunxi_r_cpucfg_enable(void)
{
	uint32_t val = readl(SUNXI_R_CPUCFG_CLK_REG);

	if ((val & SUNXI_R_CPUCFG_CLK_GATE) && (val & SUNXI_R_CPUCFG_CLK_RST))
		return;

	setbits(SUNXI_R_CPUCFG_CLK_REG, SUNXI_R_CPUCFG_CLK_GATE | SUNXI_R_CPUCFG_CLK_RST);
	dsb();
	isb();
}

static void sunxi_cpu_disable_power(unsigned int cluster, unsigned int core)
{
	if (readl(SUNXI_CPU_POWER_CLAMP_REG(cluster, core)) == 0xffu)
		return;
	writel(0xffu, SUNXI_CPU_POWER_CLAMP_REG(cluster, core));
}

static void sunxi_cpu_enable_power(unsigned int cluster, unsigned int core)
{
	if (readl(SUNXI_CPU_POWER_CLAMP_REG(cluster, core)) == 0u)
		return;
	writel(0xfeu, SUNXI_CPU_POWER_CLAMP_REG(cluster, core));
	writel(0xf8u, SUNXI_CPU_POWER_CLAMP_REG(cluster, core));
	writel(0xe0u, SUNXI_CPU_POWER_CLAMP_REG(cluster, core));
	writel(0x80u, SUNXI_CPU_POWER_CLAMP_REG(cluster, core));
	writel(0x00u, SUNXI_CPU_POWER_CLAMP_REG(cluster, core));
	udelay(1);
}

static void psci_trampoline_write(uint32_t entry_point)
{
 	/* Build small stub pointing to the requested entry */
 	TRAMP_OPCODE(0) = 0xe3a00000; /* mov r0, #0 */
 	TRAMP_OPCODE(1) = 0xee101fb0; /* mrc p15, 0, r1, c0, c0, 5 */
 	TRAMP_OPCODE(2) = 0xe59f2000; /* ldr r2, [pc] */
 	TRAMP_OPCODE(3) = 0xe12fff12; /* bx r2 */
 	TRAMP_OPCODE(4) = entry_point;
 	dsb();
 	isb();

	/* Clean data cache lines covering the trampoline */
	for (uint32_t off = 0; off < 32u; off += 32u) {
		uint32_t addr = SUNXI_SRAM_B_TRAMPOLINE + off;
		__asm__ __volatile__("mcr p15, 0, %0, c7, c10, 1" : : "r"(addr) : "memory");
	}
	dsb();
	isb();

	/* Invalidate corresponding instruction cache lines */
	for (uint32_t off = 0; off < 32u; off += 32u) {
		uint32_t addr = SUNXI_SRAM_B_TRAMPOLINE + off;
		__asm__ __volatile__("mcr p15, 0, %0, c7, c5, 1" : : "r"(addr) : "memory");
	}
	dsb();
	isb();
}

static int32_t psci_do_cpu_on(uint32_t target_affinity, uint32_t entry_point)
{
	const uint32_t cluster = (target_affinity >> 8) & 0xffu;
	const uint32_t core = target_affinity & 0xffu;
	const uint32_t entry_phys = entry_point;

	if ((cluster != 0u) || (core >= PSCI_CPU_COUNT))
		return PSCI_RET_INVALID_PARAMETERS;

	if (entry_phys == 0u)
		return PSCI_RET_INVALID_PARAMETERS;

	if (core == 0u)
		return PSCI_RET_ALREADY_ON;

	if (psci_cpu_states[core] == PSCI_AFFINITY_ON)
		return PSCI_RET_ALREADY_ON;

	psci_cpu_states[core] = PSCI_AFFINITY_ON_PENDING;

	psci_trampoline_write(entry_phys);
	writel(SUNXI_SRAM_B_TRAMPOLINE, SUNXI_CPUCFG_PRIV0_REG);
	writel(SUNXI_SRAM_B_TRAMPOLINE >> 2, SUNXI_CPU_SOFT_ENTRY_REG);
	writel(SUNXI_SRAM_B_TRAMPOLINE >> 2, SUNXI_PRCM_CPU_SOFT_ENTRY_REG);

    UNUSED_DEBUG uint32_t soft_entry = readl(SUNXI_CPU_SOFT_ENTRY_REG);
    UNUSED_DEBUG uint32_t prcm_entry = readl(SUNXI_PRCM_CPU_SOFT_ENTRY_REG);
    UNUSED_DEBUG uint32_t priv0 = readl(SUNXI_CPUCFG_PRIV0_REG);

	debug("PSCI: CPU%" PRIu32 " SOFT=0x%08" PRIx32 " PRCM=0x%08" PRIx32
	      " PRIV0=0x%08" PRIx32 "\r\n",
	      core, soft_entry, prcm_entry, priv0);

	/* Ensure CPU will come up in AArch32 */
	clrbits(SUNXI_CPUCFG_GEN_CTRL_REG0, SUNXI_AARCH_CTRL_BIT(core));

	/* Reset sequencing */
	clrbits(SUNXI_CPUCFG_RST_CTRL_REG(cluster), BIT(core));
	clrbits(SUNXI_POWERON_RST_REG(cluster), BIT(core));
	dsb();
	isb();
	sunxi_cpu_enable_power(cluster, core);
	clrbits(SUNXI_POWEROFF_GATING_REG(cluster), BIT(core));
	setbits(SUNXI_POWERON_RST_REG(cluster), BIT(core));
	setbits(SUNXI_CPUCFG_RST_CTRL_REG(cluster), BIT(core));
	setbits(SUNXI_CPUCFG_DBG_REG0, BIT(core));

    UNUSED_DEBUG uint32_t rst = readl(SUNXI_CPUCFG_RST_CTRL_REG(cluster));
    UNUSED_DEBUG uint32_t por = readl(SUNXI_POWERON_RST_REG(cluster));
    UNUSED_DEBUG uint32_t dbg = readl(SUNXI_CPUCFG_DBG_REG0);
    UNUSED_DEBUG uint32_t clamp = readl(SUNXI_CPU_POWER_CLAMP_REG(cluster, core));
    UNUSED_DEBUG uint32_t pow_gate = readl(SUNXI_POWEROFF_GATING_REG(cluster));

	debug("PSCI: CPU%" PRIu32 " rst=0x%08" PRIx32 " por=0x%08" PRIx32
	      " dbg=0x%08" PRIx32 " clamp=0x%02" PRIx32 " gate=0x%08" PRIx32 "\r\n",
	      core, rst, por, dbg, clamp & 0xffu, pow_gate);

	dsb();
	isb();
	__asm__ __volatile__("sev" ::: "memory");

	psci_cpu_states[core] = PSCI_AFFINITY_ON;
	return PSCI_RET_SUCCESS;
}

static int32_t psci_do_cpu_off(void)
{
	const uint32_t mpidr = mpidr_read();
	const uint32_t core = mpidr & 0xffu;
	const uint32_t cluster = (mpidr >> 8) & 0xffu;

	if ((cluster != 0u) || (core >= PSCI_CPU_COUNT))
		return PSCI_RET_INVALID_PARAMETERS;

	if (core == 0u)
		return PSCI_RET_DENIED;

	clrbits(SUNXI_CPUCFG_DBG_REG0, BIT(core));
	setbits(SUNXI_POWEROFF_GATING_REG(cluster), BIT(core));
	clrbits(SUNXI_POWERON_RST_REG(cluster), BIT(core));
	dsb();
	isb();
	sunxi_cpu_disable_power(cluster, core);
	psci_cpu_states[core] = PSCI_AFFINITY_OFF;
	while (true) {
		__asm__ __volatile__("wfi");
	}
}

static int32_t psci_features(uint32_t fid)
{
	switch (fid) {
	case PSCI_0_2_FN_PSCI_VERSION:
	case PSCI_1_0_FN_PSCI_FEATURES:
	case PSCI_0_2_FN_AFFINITY_INFO:
	case PSCI_0_2_FN_SYSTEM_RESET:
	case PSCI_0_2_FN_CPU_ON:
	case PSCI_0_2_FN_CPU_OFF:
		return PSCI_RET_SUCCESS;
		default:
			return PSCI_RET_NOT_SUPPORTED;
	}
}

static int32_t psci_affinity_info(uint32_t target_affinity, uint32_t level)
{
	uint32_t cpu_id;

	if (level != 0u)
		return PSCI_RET_INVALID_PARAMETERS;

	cpu_id = target_affinity & 0x3u;
	if (cpu_id >= PSCI_CPU_COUNT)
		return PSCI_RET_INVALID_PARAMETERS;

	return (int32_t)psci_cpu_states[cpu_id];
}

static void __attribute__((noreturn)) psci_system_reset(void)
{
	sunxi_wdg_set(1);
	while (true) {
		__asm__ __volatile__("wfi");
	}
}

void psci_init(void)
{
	uint32_t mvbar = (uint32_t)(uintptr_t)&psci_monitor_vector_table;

	sunxi_r_cpucfg_enable();

	debug("PSCI: monitor vectors @0x%08" PRIx32 "\r\n", mvbar);
	__asm__ __volatile__("mcr p15, 0, %0, c12, c0, 1" : : "r"(mvbar) : "memory");
	dsb();
	isb();
	UNUSED_INFO uint32_t mvbar_chk;
	__asm__ __volatile__("mrc p15, 0, %0, c12, c0, 1" : "=r"(mvbar_chk));
	debug("PSCI: mvbar readback 0x%08" PRIx32 "\r\n", mvbar_chk);
}

int32_t psci_handle_smc(uint32_t fid, uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
	(void)arg2;
    UNUSED_DEBUG uint32_t count = ++psci_smc_count;
	int32_t ret;

	switch (fid) {
		case PSCI_0_2_FN_PSCI_VERSION:
			ret = (int32_t)PSCI_VERSION_1_0;
			break;

		case PSCI_1_0_FN_PSCI_FEATURES:
		case PSCI_1_0_FN64_PSCI_FEATURES:
			ret = psci_features(arg0);
			break;

		case PSCI_0_2_FN_CPU_SUSPEND:
		case PSCI_0_2_FN64_CPU_SUSPEND:
			ret = PSCI_RET_NOT_SUPPORTED;
			break;

		case PSCI_0_2_FN_CPU_OFF:
			ret = psci_do_cpu_off();
			break;

		case PSCI_0_2_FN_CPU_ON:
		case PSCI_0_2_FN64_CPU_ON:
			ret = psci_do_cpu_on(arg0, arg1);
			break;

		case PSCI_0_2_FN_AFFINITY_INFO:
		case PSCI_0_2_FN64_AFFINITY_INFO:
			ret = psci_affinity_info(arg0, arg1);
			break;

		case PSCI_0_2_FN_MIGRATE_INFO_TYPE:
		case PSCI_0_2_FN64_MIGRATE_INFO_TYPE:
			ret = PSCI_RET_NOT_SUPPORTED;
			break;

		case PSCI_0_2_FN_SYSTEM_OFF:
		case PSCI_0_2_FN64_SYSTEM_OFF:
			ret = PSCI_RET_NOT_SUPPORTED;
			break;

		case PSCI_0_2_FN_SYSTEM_RESET:
		case PSCI_0_2_FN64_SYSTEM_RESET:
			psci_system_reset();
			ret = PSCI_RET_INTERNAL_FAILURE;
			break;

		default:
			ret = PSCI_RET_NOT_SUPPORTED;
			break;
	}

	debug("PSCI[%" PRIu32 "]: fid=0x%08" PRIx32 " a0=0x%08" PRIx32 " a1=0x%08" PRIx32 " -> %" PRId32 "\r\n",
	      count, fid, arg0, arg1, ret);

	return ret;
}
