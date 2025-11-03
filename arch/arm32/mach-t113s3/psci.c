#include <stdbool.h>
#include "common.h"
#include <stddef.h>
#include <string.h>
#include "psci.h"
#include "arm32.h"
#include "barrier.h"
#include "sunxi_wdg.h"
#include "board.h"
#include "sunxi_cpucfg.h"

#define SUNXI_CPUCFG_RST_CTRL_REG(cluster)     (SUNXI_CPUCFG_BASE + 0x0000u + (cluster) * 4u)
#define SUNXI_CPUCFG_CLS_CTRL_REG0(cluster)    (SUNXI_CPUCFG_BASE + 0x0010u + (cluster) * 0x10u)
#define SUNXI_CPUCFG_CLS_CTRL_REG1(cluster)    (SUNXI_CPUCFG_BASE + 0x0014u + (cluster) * 0x10u)
#define SUNXI_CPUCFG_CACHE_CFG_REG             (SUNXI_CPUCFG_BASE + 0x0024u)
#define SUNXI_CPUCFG_DBG_REG0                  (SUNXI_CPUCFG_BASE + 0x00c0u)

#define SUNXI_CPUCFG_C0_RST_CTRL               (SUNXI_CPUCFG_BASE + 0x0000u)
#define SUNXI_CPUCFG_C0_CTRL_REG0              (SUNXI_CPUCFG_BASE + 0x0010u)
#define SUNXI_CPUCFG_C0_CPU_STATUS_REG         (SUNXI_CPUCFG_BASE + 0x0080u)
#define SUNXI_CPUCFG_GEN_CTRL_REG0             (SUNXI_CPUCFG_BASE + 0x0184u)

#define SUNXI_AARCH_CTRL_BIT(core)             (1u << (4u + (core)))

#define SUNXI_POWERON_RST_REG(cluster)         (SUNXI_R_CPUCFG_BASE + 0x0040u + (cluster) * 4u)
#define SUNXI_POWEROFF_GATING_REG(cluster)     (SUNXI_R_CPUCFG_BASE + 0x0044u + (cluster) * 4u)
#define SUNXI_CPU_POWER_CLAMP_REG(cluster, core) \
	(SUNXI_R_CPUCFG_BASE + 0x0050u + (cluster) * 0x10u + (core) * 4u)
#define SUNXI_CPU_SOFT_ENTRY_REG               (SUNXI_R_CPUCFG_BASE + 0x01c8u)
#define SUNXI_CPU_STATUS_WFI_BIT(core)         BIT(16u + (core))

#define PSCI_CPU1_PROBE_MAGIC                  0x43505531u
#define PSCI_CPU_SELFTEST                      1
#define SUNXI_R_CPUCFG_CLK_REG                (SUNXI_R_PRCM_BASE + 0x022cu)
#define SUNXI_R_CPUCFG_CLK_GATE               BIT(0)
#define SUNXI_R_CPUCFG_CLK_RST                BIT(16)

/* Secondary CPU entry trampoline written into SRAM B */
#define SUNXI_SRAM_B_BASE        0x00030000u
/* Keep the trampoline out of the SPL text/stack region (0x30000-0x37900). */
#define SUNXI_SRAM_B_TRAMPOLINE  (SUNXI_SRAM_B_BASE + 0x00007c00u)
#define SUNXI_SRAM_B_TRAMPOLINE_NS (SUNXI_SRAM_B_TRAMPOLINE + PSCI_TRAMP_NS_OFFSET)

#define PSCI_NS_SHMEM_BASE        (SDRAM_TOP - 0x00001000u)

#define TRAMP_NS_OPCODE(idx)    (*(volatile uint32_t *)(SUNXI_SRAM_B_TRAMPOLINE_NS + (idx) * sizeof(uint32_t)))

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
#define PSCI_SELFTEST_CONTEXT_SEC          0xffffffffu
#define PSCI_SELFTEST_CONTEXT_NS           0xfffffffeu
#define PSCI_TRAMP_NS_OFFSET               0x40u
#define PSCI_NS_DIAG_PHYS                 (PSCI_NS_SHMEM_BASE + 0x00u)
#define PSCI_NS_STAGE_PHYS                (PSCI_NS_SHMEM_BASE + 0x10u)
#define PSCI_NS_SRAM_TEST_PHYS            (PSCI_NS_SHMEM_BASE + 0x20u)
#define PSCI_NS_TRACE_PHYS                (PSCI_NS_SHMEM_BASE + 0x30u)
#define PSCI_NS_STUB_PHYS                 (PSCI_NS_SHMEM_BASE + 0x100u)
#define PSCI_SELFTEST_NS_FID               0x82000100u

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

#define PSCI_MONITOR_DIAG_WORDS 8u

static volatile uint32_t psci_cpu_states[PSCI_CPU_COUNT] = {
	PSCI_AFFINITY_ON,
	PSCI_AFFINITY_OFF,
};
volatile uint32_t psci_cpu_last_context[PSCI_CPU_COUNT];
volatile uint32_t psci_cpu_entry_phys[PSCI_CPU_COUNT];
volatile uint32_t psci_cpu_tramp_trace[3];
extern volatile uint32_t psci_monitor_diag[];

const uint32_t psci_ns_stage_phys_value __attribute__((used)) = PSCI_NS_STAGE_PHYS;
const uint32_t psci_selftest_context_sec_value __attribute__((used)) = PSCI_SELFTEST_CONTEXT_SEC;
extern uint8_t psci_ns_stub_start[];
extern uint8_t psci_ns_stub_end[];
extern uint32_t psci_ns_stub_diag_lit[];
extern uint32_t psci_ns_stub_stage_lit[];
extern uint32_t psci_ns_stub_sram_lit[];
extern uint32_t psci_ns_stub_trace_lit[];
extern void psci_cpu_secure_entry(void);

#if PSCI_CPU_SELFTEST
volatile uint32_t psci_cpu1_probe_magic;
volatile uint32_t psci_cpu1_probe_counter;
volatile uint32_t psci_cpu1_probe_mpidr;
volatile uint32_t psci_cpu1_probe_cpsr;

static void __attribute__((noreturn)) psci_cpu1_dummy_entry(void);
static void psci_cpu1_manual_bringup(void);
#endif

enum psci_monitor_reason {
	PSCI_MONITOR_REASON_NONE = 0u,
	PSCI_MONITOR_REASON_UNDEF = 0xe0u,
	PSCI_MONITOR_REASON_PREFETCH = 0xe1u,
	PSCI_MONITOR_REASON_DATA = 0xe2u,
	PSCI_MONITOR_REASON_IRQ = 0xe3u,
	PSCI_MONITOR_REASON_FIQ = 0xe4u,
	PSCI_MONITOR_REASON_RESET = 0xe5u,
};

static const char *psci_monitor_reason_name(uint32_t reason)
{
	switch (reason) {
	case PSCI_MONITOR_REASON_UNDEF:
		return "undef";
	case PSCI_MONITOR_REASON_PREFETCH:
		return "prefetch";
	case PSCI_MONITOR_REASON_DATA:
		return "data";
	case PSCI_MONITOR_REASON_IRQ:
		return "irq";
	case PSCI_MONITOR_REASON_FIQ:
		return "fiq";
	case PSCI_MONITOR_REASON_RESET:
		return "reset";
	default:
		return "unknown";
	}
}

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

static inline void psci_stage_mark(uint32_t stage)
{
	writel(stage, PSCI_NS_STAGE_PHYS);
	dmb();
	isb();
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


static void psci_trampoline_write(uint32_t entry_point, uint32_t context_id)
{
	uint32_t entry_phys = entry_point;

	psci_cpu_tramp_trace[0] = 0xffffffffu;
	psci_cpu_tramp_trace[1] = 0xffffffffu;
	psci_cpu_tramp_trace[2] = 0xffffffffu;
	dmb();
	psci_cpu_tramp_trace[2] = 1u;
	psci_cpu_tramp_trace[0] = entry_phys;
	psci_cpu_tramp_trace[1] = context_id;
	dmb();
	size_t ns_trampoline_size = 128u;
	uintptr_t ns_trampoline_phys = (uintptr_t)SUNXI_SRAM_B_TRAMPOLINE_NS;
	if (context_id == PSCI_SELFTEST_CONTEXT_NS) {
		psci_stage_mark(0x10u);
		psci_cpu_tramp_trace[2] = 0x10u;
	}

	if (context_id == PSCI_SELFTEST_CONTEXT_SEC) {
		memset((void *)SUNXI_SRAM_B_TRAMPOLINE_NS, 0, 128u);
	} else {
		if (context_id == PSCI_SELFTEST_CONTEXT_NS) {
			const size_t stub_size = (size_t)(psci_ns_stub_end - psci_ns_stub_start);
			void *stub_dest = (void *)(uintptr_t)PSCI_NS_STUB_PHYS;
			memcpy(stub_dest, psci_ns_stub_start, stub_size);
			ns_trampoline_size = stub_size;
			ns_trampoline_phys = (uintptr_t)PSCI_NS_STUB_PHYS;
			for (uint32_t i = 0u; i < PSCI_MONITOR_DIAG_WORDS; ++i) {
				psci_monitor_diag[i] = 0u;
			}
			psci_stage_mark(0x11u);
			psci_cpu_tramp_trace[2] = 0x11u;

			const uintptr_t base = ns_trampoline_phys;
			const uintptr_t template_base = (uintptr_t)psci_ns_stub_start;

			const uintptr_t diag_off = (uintptr_t)psci_ns_stub_diag_lit - template_base;
			const uintptr_t stage_off = (uintptr_t)psci_ns_stub_stage_lit - template_base;
			const uintptr_t sram_off = (uintptr_t)psci_ns_stub_sram_lit - template_base;
			const uintptr_t trace_off = (uintptr_t)psci_ns_stub_trace_lit - template_base;

			uint32_t *diag_ptr = (uint32_t *)(base + diag_off);
			uint32_t *stage_ptr = (uint32_t *)(base + stage_off);
			uint32_t *scratch_ptr = (uint32_t *)(base + sram_off);
			uint32_t *trace_ptr = (uint32_t *)(base + trace_off);

			*diag_ptr = PSCI_NS_DIAG_PHYS;
			*stage_ptr = PSCI_NS_STAGE_PHYS;
			*scratch_ptr = PSCI_NS_SRAM_TEST_PHYS;
			*trace_ptr = PSCI_NS_TRACE_PHYS;

			writel(0u, PSCI_NS_DIAG_PHYS + 0u);
			writel(0u, PSCI_NS_DIAG_PHYS + 4u);
			writel(0u, PSCI_NS_DIAG_PHYS + 8u);
			writel(0u, PSCI_NS_STAGE_PHYS);
			writel(0u, PSCI_NS_SRAM_TEST_PHYS);
			writel(0u, PSCI_NS_TRACE_PHYS);
			writel(1u, PSCI_NS_STAGE_PHYS);
			psci_stage_mark(0x12u);
			psci_cpu_tramp_trace[2] = 0x12u;

			debug("PSCI: NS stub copied (%zu bytes) dest=0x%08" PRIx32
		      " diag=0x%08" PRIx32
		      " stage=0x%08" PRIx32 " scratch=0x%08" PRIx32
		      " trace=0x%08" PRIx32
		      " offs(d,s,r,t)=(0x%zx,0x%zx,0x%zx,0x%zx)\r\n",
		      stub_size, (uint32_t)ns_trampoline_phys,
		      *diag_ptr, *stage_ptr, *scratch_ptr, *trace_ptr,
		      (size_t)diag_off, (size_t)stage_off, (size_t)sram_off, (size_t)trace_off);
			psci_stage_mark(0x13u);
			psci_cpu_tramp_trace[2] = 0x13u;
		} else {
			memset((void *)SUNXI_SRAM_B_TRAMPOLINE_NS, 0, 128u);
		}
	}
	memset((void *)SUNXI_SRAM_B_TRAMPOLINE_NS, 0, 128u);
	memset((void *)SUNXI_SRAM_B_TRAMPOLINE, 0, 128u);
	dsb();
	isb();

	/* Clean data cache lines covering the trampolines */
	for (uint32_t off = 0u; off < 128u; off += 32u) {
		uint32_t addr = SUNXI_SRAM_B_TRAMPOLINE + off;
		__asm__ __volatile__("mcr p15, 0, %0, c7, c10, 1" : : "r"(addr) : "memory");
	}
	uintptr_t ns_clean_start = ns_trampoline_phys & ~0x1fu;
	uintptr_t ns_clean_end = (ns_trampoline_phys + ns_trampoline_size + 31u) & ~0x1fu;
	for (uintptr_t addr = ns_clean_start; addr < ns_clean_end; addr += 32u) {
		__asm__ __volatile__("mcr p15, 0, %0, c7, c10, 1" : : "r"(addr) : "memory");
	}
	dsb();
	isb();

	/* Invalidate corresponding instruction cache lines */
	for (uint32_t off = 0u; off < 128u; off += 32u) {
		uint32_t addr = SUNXI_SRAM_B_TRAMPOLINE + off;
		__asm__ __volatile__("mcr p15, 0, %0, c7, c5, 1" : : "r"(addr) : "memory");
	}
	for (uintptr_t addr = ns_clean_start; addr < ns_clean_end; addr += 32u) {
		__asm__ __volatile__("mcr p15, 0, %0, c7, c5, 1" : : "r"(addr) : "memory");
	}
	dsb();
	isb();
	if (context_id == PSCI_SELFTEST_CONTEXT_NS) {
		psci_stage_mark(0x14u);
		psci_cpu_tramp_trace[2] = 0x14u;
	}
}
#if PSCI_CPU_SELFTEST
static void __attribute__((noreturn)) psci_cpu1_dummy_entry(void)
{
	uint32_t local = 0u;

	arm32_enable_smp();
	psci_cpu1_probe_cpsr = arm32_cpsr_read();

	psci_cpu1_probe_magic = PSCI_CPU1_PROBE_MAGIC;
	psci_cpu1_probe_mpidr = mpidr_read();
	dmb();
	dsb();

	while (true) {
		++local;
		psci_cpu1_probe_counter = local;
		dmb();
		for (volatile uint32_t delay = 0u; delay < 0x10000u; ++delay) {
			__asm__ __volatile__("nop");
		}
	}
}
#endif


static int32_t psci_do_cpu_on(uint32_t target_affinity, uint32_t entry_point,
                              uint32_t context_id)
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
	psci_cpu_last_context[core] = context_id;

	uint32_t target_entry = entry_phys;
	if (context_id == PSCI_SELFTEST_CONTEXT_NS) {
		psci_trampoline_write(entry_phys, context_id);
		target_entry = (PSCI_NS_STUB_PHYS | 1u);
	} else {
		writel(0u, PSCI_NS_STAGE_PHYS);
	}

	psci_cpu_entry_phys[core] = target_entry;
	psci_cpu_tramp_trace[0] = 0u;
	psci_cpu_tramp_trace[1] = 0u;
	psci_cpu_tramp_trace[2] = (context_id == PSCI_SELFTEST_CONTEXT_NS) ? 0x10u : 0u;
	if (context_id == PSCI_SELFTEST_CONTEXT_NS) {
		psci_stage_mark(0x18u);
		psci_cpu_tramp_trace[2] = 0x18u;
	}

	writel((uint32_t)(uintptr_t)&psci_cpu_secure_entry, SUNXI_CPU_SOFT_ENTRY_REG);

	UNUSED_DEBUG uint32_t soft_entry = readl(SUNXI_CPU_SOFT_ENTRY_REG);

	debug("PSCI: CPU%" PRIu32 " soft-entry=0x%08" PRIx32
	      " entry=0x%08" PRIx32 " ctx=0x%08" PRIx32 "\r\n",
	      core, soft_entry, entry_phys, context_id);

	/* Hold core in reset while preparing hand-off (matches U-Boot flow) */
	clrbits(SUNXI_CPUCFG_C0_RST_CTRL, BIT(core));
	dsb();
	isb();
	if (context_id == PSCI_SELFTEST_CONTEXT_NS) {
		psci_stage_mark(0x1au);
		psci_cpu_tramp_trace[2] = 0x1au;
	}

	/* Invalidate L1 cache by clearing CTRL_REG0 bit (R528 semantics) */
	clrbits(SUNXI_CPUCFG_C0_CTRL_REG0, BIT(core));
	dsb();
	isb();
	if (context_id == PSCI_SELFTEST_CONTEXT_NS) {
		psci_stage_mark(0x1bu);
		psci_cpu_tramp_trace[2] = 0x1bu;
	}

	/* Ensure core power domain is ready (matches earlier bring-up flow) */
	sunxi_cpu_enable_power(cluster, core);
	clrbits(SUNXI_POWEROFF_GATING_REG(cluster), BIT(core));
	setbits(SUNXI_POWERON_RST_REG(cluster), BIT(core));
	setbits(SUNXI_CPUCFG_DBG_REG0, BIT(core));
	dsb();
	isb();
	if (context_id == PSCI_SELFTEST_CONTEXT_NS) {
		psci_stage_mark(0x1cu);
		psci_cpu_tramp_trace[2] = 0x1cu;
	}

	/* Ensure CPU will come up in AArch32 */
	clrbits(SUNXI_CPUCFG_GEN_CTRL_REG0, SUNXI_AARCH_CTRL_BIT(core));

	setbits(SUNXI_CPUCFG_C0_RST_CTRL, BIT(core));
	if (context_id == PSCI_SELFTEST_CONTEXT_NS) {
		psci_stage_mark(0x1du);
		psci_cpu_tramp_trace[2] = 0x1du;
	}

    UNUSED_DEBUG uint32_t rst = readl(SUNXI_CPUCFG_C0_RST_CTRL);
    UNUSED_DEBUG uint32_t ctrl = readl(SUNXI_CPUCFG_C0_CTRL_REG0);
    UNUSED_DEBUG uint32_t dbg = readl(SUNXI_CPUCFG_DBG_REG0);
    UNUSED_DEBUG uint32_t status = readl(SUNXI_CPUCFG_C0_CPU_STATUS_REG);

    UNUSED_DEBUG uint32_t por = readl(SUNXI_POWERON_RST_REG(cluster));
    UNUSED_DEBUG uint32_t gate = readl(SUNXI_POWEROFF_GATING_REG(cluster));
    UNUSED_DEBUG uint32_t clamp = readl(SUNXI_CPU_POWER_CLAMP_REG(cluster, core));

	debug("PSCI: CPU%" PRIu32 " rst=0x%08" PRIx32 " ctrl0=0x%08" PRIx32
	      " dbg=0x%08" PRIx32 " status=0x%08" PRIx32 " por=0x%08" PRIx32
	      " gate=0x%08" PRIx32 " clamp=0x%02" PRIx32 "\r\n",
	      core, rst, ctrl, dbg, status, por, gate, clamp & 0xffu);

	debug("PSCI: CPU%" PRIu32 " tramp entry=0x%08" PRIx32
	      " ctx=0x%08" PRIx32 " stage=0x%08" PRIx32 "\r\n",
	      core, psci_cpu_tramp_trace[0],
	      psci_cpu_tramp_trace[1],
	      psci_cpu_tramp_trace[2]);
	if (context_id == PSCI_SELFTEST_CONTEXT_NS) {
		psci_stage_mark(0x1eu);
		psci_cpu_tramp_trace[2] = 0x1eu;
	}

	dsb();
	isb();
	if (context_id == PSCI_SELFTEST_CONTEXT_NS) {
		psci_stage_mark(0x1fu);
		psci_cpu_tramp_trace[2] = 0x1fu;
	}
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

#if PSCI_CPU_SELFTEST
static void psci_cpu1_manual_bringup(void)
{
	if (psci_cpu_states[1] != PSCI_AFFINITY_OFF) {
		fatal("PSCI: CPU1 unexpected initial state %" PRIu32 "\r\n",
		      psci_cpu_states[1]);
	}

	const struct {
		const char *label;
		uint32_t entry;
		uint32_t context;
	} tests[] = {
	{
		.label = "secure self-test",
		.entry = (uint32_t)(uintptr_t)&psci_cpu1_dummy_entry,
		.context = PSCI_SELFTEST_CONTEXT_SEC,
	},
	{
		.label = "non-secure handoff test",
		.entry = SUNXI_SRAM_B_TRAMPOLINE + PSCI_TRAMP_NS_OFFSET,
		.context = PSCI_SELFTEST_CONTEXT_NS,
	},
};

	for (uint32_t idx = 0u; idx < (uint32_t)(sizeof(tests) / sizeof(tests[0])); ++idx) {
		const char *label = tests[idx].label;
		const uint32_t entry = tests[idx].entry;
		const uint32_t ctx = tests[idx].context;

		psci_cpu1_probe_magic = 0u;
		psci_cpu1_probe_counter = 0u;
		psci_cpu1_probe_mpidr = 0u;
		psci_cpu1_probe_cpsr = 0u;
		if (ctx == PSCI_SELFTEST_CONTEXT_NS) {
			writel(0u, PSCI_NS_DIAG_PHYS + 0u);
			writel(0u, PSCI_NS_DIAG_PHYS + 4u);
			writel(0u, PSCI_NS_DIAG_PHYS + 8u);
			writel(0u, PSCI_NS_STAGE_PHYS);
			writel(0xdeadbeefu, PSCI_NS_SRAM_TEST_PHYS);
			writel(0u, PSCI_NS_TRACE_PHYS);
			dsb();
			isb();
			psci_stage_mark(0x08u);
			psci_cpu_tramp_trace[2] = 0x08u;
		}

		const int32_t ret = psci_do_cpu_on(1u, entry, ctx);

		if (ret != PSCI_RET_SUCCESS) {
			fatal("PSCI: CPU1 %s failed -> %" PRId32 "\r\n",
			      label, ret);
		}

		bool cpu1_alive = false;
		for (uint32_t wait = 0u; wait < 500000u; ++wait) {
			if (psci_cpu1_probe_magic == PSCI_CPU1_PROBE_MAGIC) {
				const uint32_t UNUSED_DEBUG counter = psci_cpu1_probe_counter;
				const uint32_t UNUSED_DEBUG mpidr = psci_cpu1_probe_mpidr;
				const uint32_t UNUSED_DEBUG cpsr = psci_cpu1_probe_cpsr;
				debug("PSCI: CPU1 %s alive (mpidr=0x%08" PRIx32
				      ", counter=%" PRIu32 " cpsr=0x%08" PRIx32 ")\r\n",
				      label, mpidr, counter, cpsr);
				cpu1_alive = true;
				break;
			}

			__asm__ __volatile__("nop");
		}

		if (!cpu1_alive) {
			if (ctx == PSCI_SELFTEST_CONTEXT_NS) {
				uint32_t UNUSED_DEBUG diag_cnt = readl(PSCI_NS_DIAG_PHYS + 0u);
				uint32_t UNUSED_DEBUG diag_mpidr = readl(PSCI_NS_DIAG_PHYS + 4u);
				uint32_t UNUSED_DEBUG diag_cpsr = readl(PSCI_NS_DIAG_PHYS + 8u);
				uint32_t UNUSED_DEBUG diag_stage = readl(PSCI_NS_STAGE_PHYS);
				uint32_t UNUSED_DEBUG diag_trace = readl(PSCI_NS_TRACE_PHYS);
				debug("PSCI: CPU1 %s diag cnt=%" PRIu32 " mpidr=0x%08" PRIx32
				      " cpsr=0x%08" PRIx32 " stage=0x%08" PRIx32
				      " trace=0x%08" PRIx32 "\r\n",
				      label, diag_cnt, diag_mpidr, diag_cpsr, diag_stage, diag_trace);
				debug("PSCI: CPU1 %s probe magic=0x%08" PRIx32
				      " mpidr=0x%08" PRIx32 " cpsr=0x%08" PRIx32 "\r\n",
				      label, psci_cpu1_probe_magic,
				      psci_cpu1_probe_mpidr,
				      psci_cpu1_probe_cpsr);
				uint32_t mon_reason_raw = psci_monitor_diag[0];
				uint32_t mon_reason = mon_reason_raw & 0xffu;
				if (mon_reason_raw != PSCI_MONITOR_REASON_NONE) {
					const char *mon_name = psci_monitor_reason_name(mon_reason);
					uint32_t mon_spsr = psci_monitor_diag[1];
					uint32_t mon_lr = psci_monitor_diag[2];
					uint32_t mon_ifsr = psci_monitor_diag[3];
					uint32_t mon_ifar = psci_monitor_diag[4];
					uint32_t mon_dfsr = psci_monitor_diag[5];
					uint32_t mon_dfar = psci_monitor_diag[6];
					uint32_t mon_mpidr = psci_monitor_diag[7];
					uint32_t mon_ifsr_fs = ((mon_ifsr & 0x3fu) | ((mon_ifsr >> 6) & 0x40u));
					uint32_t mon_dfsr_fs = ((mon_dfsr & 0x3fu) | ((mon_dfsr >> 6) & 0x40u));
					debug("PSCI: monitor %s fault reason(raw)=0x%08" PRIx32
					      " reason=0x%02" PRIx32
					      " spsr=0x%08" PRIx32 " lr=0x%08" PRIx32
					      " ifsr=0x%08" PRIx32 " fs=0x%02" PRIx32
					      " ifar=0x%08" PRIx32
					      " dfsr=0x%08" PRIx32 " fs=0x%02" PRIx32
					      " dfar=0x%08" PRIx32
					      " mpidr=0x%08" PRIx32 "\r\n",
					      mon_name, mon_reason_raw, mon_reason,
					      mon_spsr, mon_lr,
					      mon_ifsr, mon_ifsr_fs,
					      mon_ifar,
					      mon_dfsr, mon_dfsr_fs,
					      mon_dfar,
					      mon_mpidr);
				}
			}
			debug("PSCI: CPU1 %s trace entry=0x%08" PRIx32
		      " ctx=0x%08" PRIx32 " stage=0x%08" PRIx32 "\r\n",
			      label, psci_cpu_tramp_trace[0],
			      psci_cpu_tramp_trace[1],
			      psci_cpu_tramp_trace[2]);
			fatal("PSCI: CPU1 %s entry=0x%08" PRIx32
			      " probe failed\r\n",
			      label, entry);
		}

		/* Park CPU1 back into reset so PSCI flow can own it again */
		clrbits(SUNXI_CPUCFG_C0_RST_CTRL, BIT(1u));
		clrbits(SUNXI_CPUCFG_DBG_REG0, BIT(1u));
		setbits(SUNXI_CPUCFG_C0_CTRL_REG0, BIT(1u));
		writel(0u, SUNXI_CPU_SOFT_ENTRY_REG);
		udelay(1);
		dsb();
		isb();

		psci_cpu_states[1] = PSCI_AFFINITY_OFF;

		UNUSED_DEBUG uint32_t rst = readl(SUNXI_CPUCFG_C0_RST_CTRL);
		UNUSED_DEBUG uint32_t ctrl = readl(SUNXI_CPUCFG_C0_CTRL_REG0);
		UNUSED_DEBUG uint32_t dbg = readl(SUNXI_CPUCFG_DBG_REG0);

		debug("PSCI: CPU1 %s complete, reset re-asserted "
		      "(rst=0x%08" PRIx32 " ctrl0=0x%08" PRIx32
		      " dbg=0x%08" PRIx32 ")\r\n",
		      label, rst, ctrl, dbg);
	}
}
#else
static inline void psci_cpu1_manual_bringup(void)
{
}
#endif

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

#if PSCI_CPU_SELFTEST
    psci_cpu1_manual_bringup();
#endif
}

int32_t psci_handle_smc(uint32_t fid, uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
    UNUSED_DEBUG uint32_t count = ++psci_smc_count;
	int32_t ret;

	switch (fid) {
	case PSCI_SELFTEST_NS_FID:
		psci_cpu1_probe_magic = PSCI_CPU1_PROBE_MAGIC;
		psci_cpu1_probe_mpidr = arg0;
		psci_cpu1_probe_cpsr = arg1;
		psci_cpu1_probe_counter = arg2;
		ret = PSCI_RET_SUCCESS;
		break;

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
			ret = psci_do_cpu_on(arg0, arg1, arg2);
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
