#include <stdbool.h>
#include <inttypes.h>
#include "common.h"
#include <stddef.h>
#include <string.h>
#include "psci.h"
#include "arm32.h"
#include "barrier.h"
#include "sunxi_wdg.h"
#include "board.h"
#include "sunxi_cpucfg.h"
#if PSCI_CPU_SELFTEST
#include "psci_selftest.h"
#endif

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
#define SUNXI_R_CPUCFG_CLK_REG                (SUNXI_R_PRCM_BASE + 0x022cu)
#define SUNXI_R_CPUCFG_CLK_GATE               BIT(0)
#define SUNXI_R_CPUCFG_CLK_RST                BIT(16)

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
#define PSCI_SELFTEST_CONTEXT_NS           0xfffffffeu
#define PSCI_TRAMP_NS_OFFSET               0x40u
#define PSCI_NS_DIAG_PHYS                 (PSCI_NS_SHMEM_BASE + 0x00u)
#define PSCI_NS_STAGE_PHYS                (PSCI_NS_SHMEM_BASE + 0x10u)
#define PSCI_NS_TEST_PHYS                 (PSCI_NS_SHMEM_BASE + 0x20u)
#define PSCI_NS_TRACE_PHYS                (PSCI_NS_SHMEM_BASE + 0x30u)
#define PSCI_NS_ENTRY_TARGET_PHYS         (PSCI_NS_SHMEM_BASE + 0x40u)
#define PSCI_NS_STUB_PHYS                 (PSCI_NS_SHMEM_BASE + 0x100u)
#define PSCI_NS_LINUX_SECONDARY_PHYS      (PSCI_NS_STUB_PHYS + 0x200u)
#define PSCI_NS_LINUX_STACK_PHYS          (PSCI_NS_LINUX_SECONDARY_PHYS + 0x200u)
#define PSCI_NS_LINUX_STACK_SIZE          0x4000u
#define PSCI_NS_LINUX_TASK_PHYS           (PSCI_NS_LINUX_STACK_PHYS + PSCI_NS_LINUX_STACK_SIZE)
#define PSCI_NS_LINUX_TASK_SIZE           0x1000u
#define PSCI_NS_LINUX_ENTRY_PHYS          (PSCI_NS_LINUX_TASK_PHYS + PSCI_NS_LINUX_TASK_SIZE + 0x1000u)
#define PSCI_NS_LINUX_ENTRY_MAX_SIZE      0x200u
#define PSCI_NS_LINUX_PGDIR_PHYS          0x00400000ull
#define PSCI_NS_LINUX_SWAPPER_PHYS        0x00800000u
#define PSCI_NS_STAGE_SELFTEST_FINAL      0x53u
#define PSCI_DCACHE_LINE_SIZE             32u
#ifndef PSCI_TRACE_ENABLE
#define PSCI_TRACE_ENABLE 0
#endif
#define PSCI_SELFTEST_NS_FID               0x82000100u
#ifndef PSCI_LINUX_SECONDARY_DATA_OFFSET
#define PSCI_LINUX_SECONDARY_DATA_OFFSET  0u
#endif

#if PSCI_TRACE_ENABLE
#define PSCI_TRACE_DEBUG(...) debug(__VA_ARGS__)
#else
#define PSCI_TRACE_DEBUG(...) \
	do {                       \
	} while (0)
#endif

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

#if PSCI_TRACE_ENABLE
static volatile uint32_t psci_smc_count;
#endif

#define PSCI_MONITOR_DIAG_WORDS 8u

static volatile uint32_t psci_cpu_states[PSCI_CPU_COUNT] = {
	PSCI_AFFINITY_ON,
	PSCI_AFFINITY_OFF,
};
volatile uint32_t psci_cpu_last_context[PSCI_CPU_COUNT];
volatile uint32_t psci_cpu_entry_phys[PSCI_CPU_COUNT];
extern volatile uint32_t psci_monitor_diag[];
const uint32_t psci_ns_stage_phys_value __attribute__((used)) = PSCI_NS_STAGE_PHYS;
const uint32_t psci_ns_diag_phys_value __attribute__((used)) = PSCI_NS_DIAG_PHYS;
const uint32_t psci_ns_linux_secondary_phys_value __attribute__((used)) = PSCI_NS_LINUX_SECONDARY_PHYS;
const uint32_t psci_ns_entry_target_phys_value __attribute__((used)) = PSCI_NS_ENTRY_TARGET_PHYS;
const uint32_t psci_ns_trace_phys_value __attribute__((used)) = PSCI_NS_TRACE_PHYS;
const uint32_t psci_ns_stack_phys_value __attribute__((used)) = PSCI_NS_LINUX_STACK_PHYS;
const uint32_t psci_ns_stack_size_value __attribute__((used)) = PSCI_NS_LINUX_STACK_SIZE;
const uint32_t psci_ns_task_phys_value __attribute__((used)) = PSCI_NS_LINUX_TASK_PHYS;
const uint32_t psci_ns_task_size_value __attribute__((used)) = PSCI_NS_LINUX_TASK_SIZE;
const uint64_t psci_ns_pgdir_phys_value __attribute__((used)) = PSCI_NS_LINUX_PGDIR_PHYS;
const uint32_t psci_ns_swapper_phys_value __attribute__((used)) = PSCI_NS_LINUX_SWAPPER_PHYS;
extern uint8_t psci_ns_stub_start[];
extern uint8_t psci_ns_stub_end[];
extern uint32_t psci_ns_stub_diag_lit[];
extern uint32_t psci_ns_stub_stage_lit[];
extern uint32_t psci_ns_stub_sram_lit[];
extern uint32_t psci_ns_stub_trace_lit[];
extern uint32_t psci_ns_stub_secdata_lit[];
extern uint32_t psci_ns_stub_context_lit[];
extern uint32_t psci_ns_stub_entry_lit[];
extern uint8_t psci_ns_linux_entry_start[];
extern uint8_t psci_ns_linux_entry_end[];
extern uint32_t psci_ns_linux_entry_target_lit[];
extern uint32_t psci_ns_linux_entry_trace_lit[];
extern void psci_cpu_secure_entry(void);

void psci_clean_dcache_range(uintptr_t addr, size_t size);
void psci_invalidate_icache_range(uintptr_t addr, size_t size);
static inline void writel(uint32_t val, uint32_t addr);
static inline void setbits(uint32_t addr, uint32_t mask);
static inline void clrbits(uint32_t addr, uint32_t mask);

#if PSCI_CPU_SELFTEST
volatile uint32_t psci_cpu1_probe_magic;
volatile uint32_t psci_cpu1_probe_counter;
volatile uint32_t psci_cpu1_probe_mpidr;
volatile uint32_t psci_cpu1_probe_cpsr;

static int32_t psci_smc_call(uint32_t fid, uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
	register uint32_t r0 __asm__("r0") = fid;
	register uint32_t r1 __asm__("r1") = arg0;
	register uint32_t r2 __asm__("r2") = arg1;
	register uint32_t r3 __asm__("r3") = arg2;

	__asm__ __volatile__("smc #0"
			 : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3)
			 :
			 : "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "lr", "cc", "memory");

	return (int32_t)r0;
}

static void psci_cpu1_manual_bringup(void);
#endif

static void psci_ns_entry_prepare(uint32_t entry_target)
{
#if LOG_LEVEL >= LOG_DEBUG
	const size_t entry_size =
		(size_t)(psci_ns_linux_entry_end - psci_ns_linux_entry_start);

	if (entry_size > PSCI_NS_LINUX_ENTRY_MAX_SIZE) {
		fatal("PSCI: linux self-test entry too large (%zu bytes)\r\n", entry_size);
	}

	uint8_t *entry_dest = (uint8_t *)(uintptr_t)PSCI_NS_LINUX_ENTRY_PHYS;
	memcpy(entry_dest, psci_ns_linux_entry_start, entry_size);

	const uintptr_t entry_src = (uintptr_t)psci_ns_linux_entry_start;
	const uintptr_t target_off =
		(uintptr_t)psci_ns_linux_entry_target_lit - entry_src;

	uint32_t *target_ptr = (uint32_t *)(entry_dest + target_off);
	target_ptr[0] = PSCI_NS_ENTRY_TARGET_PHYS;

	const uintptr_t trace_src = (uintptr_t)psci_ns_linux_entry_trace_lit;
	const uintptr_t trace_off = trace_src - entry_src;
	uint32_t *trace_ptr = (uint32_t *)(entry_dest + trace_off);
	trace_ptr[0] = PSCI_NS_TRACE_PHYS;

	uint32_t entry_thumb = entry_target | 0x1u;
	if (entry_thumb != entry_target) {
		PSCI_TRACE_DEBUG("PSCI: forcing Thumb entry bit (target=0x%08" PRIx32
				 " -> 0x%08" PRIx32 ")\r\n",
				 entry_target, entry_thumb);
	}
	writel(entry_thumb, PSCI_NS_ENTRY_TARGET_PHYS);

	psci_clean_dcache_range(PSCI_NS_ENTRY_TARGET_PHYS, sizeof(entry_thumb));
	psci_clean_dcache_range(PSCI_NS_LINUX_ENTRY_PHYS, entry_size);
	psci_invalidate_icache_range(PSCI_NS_LINUX_ENTRY_PHYS, entry_size);
#else
	(void)entry_target;
#endif
}
enum psci_monitor_reason {
	PSCI_MONITOR_REASON_NONE = 0u,
	PSCI_MONITOR_REASON_UNDEF = 0xe0u,
	PSCI_MONITOR_REASON_PREFETCH = 0xe1u,
	PSCI_MONITOR_REASON_DATA = 0xe2u,
	PSCI_MONITOR_REASON_IRQ = 0xe3u,
	PSCI_MONITOR_REASON_FIQ = 0xe4u,
	PSCI_MONITOR_REASON_RESET = 0xe5u,
};

#if PSCI_TRACE_ENABLE
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

static void psci_log_monitor_diag(void)
{
	uint32_t reason_raw = psci_monitor_diag[0];
	if (reason_raw == PSCI_MONITOR_REASON_NONE)
		return;

	uint32_t reason = reason_raw & 0xffu;
	const char *reason_name = psci_monitor_reason_name(reason);
	uint32_t spsr = psci_monitor_diag[1];
	uint32_t lr = psci_monitor_diag[2];
	uint32_t ifsr = psci_monitor_diag[3];
	uint32_t ifar = psci_monitor_diag[4];
	uint32_t dfsr = psci_monitor_diag[5];
	uint32_t dfar = psci_monitor_diag[6];
	uint32_t mpidr = psci_monitor_diag[7];
	uint32_t elr_hyp = psci_monitor_diag[8];
	uint32_t spsr_hyp = psci_monitor_diag[9];
	uint32_t spsr_target = psci_monitor_diag[10];
	uint32_t virt_field = psci_monitor_diag[11];
	uint32_t inst = 0u;
	if ((reason == PSCI_MONITOR_REASON_PREFETCH) ||
	    (reason == PSCI_MONITOR_REASON_UNDEF)) {
		inst = read32(ifar);
	}

	PSCI_TRACE_DEBUG("PSCI: monitor trap %s (raw=0x%08" PRIx32 ") "
			 "spsr=0x%08" PRIx32 " lr=0x%08" PRIx32
			 " ifsr=0x%08" PRIx32 " ifar=0x%08" PRIx32
			 " dfsr=0x%08" PRIx32 " dfar=0x%08" PRIx32
			 " mpidr=0x%08" PRIx32
			 " elr_hyp=0x%08" PRIx32 " spsr_hyp=0x%08" PRIx32
			 " spsr_target=0x%08" PRIx32 " virt_field=0x%08" PRIx32 "\r\n",
			 reason_name,
			 reason_raw,
			 spsr,
			 lr,
			 ifsr,
			 ifar,
			 dfsr,
			 dfar,
			 mpidr,
			 elr_hyp,
			 spsr_hyp,
			 spsr_target,
			 virt_field);
	PSCI_TRACE_DEBUG("PSCI: monitor trap inst=0x%08" PRIx32 "\r\n", inst);

	psci_monitor_diag[0] = PSCI_MONITOR_REASON_NONE;
}
#else
static inline void psci_log_monitor_diag(void)
{
}
#endif

static inline void writel(uint32_t val, uint32_t addr)
{
	write32(addr, val);
}

static inline void setbits(uint32_t addr, uint32_t mask)
{
	writel(read32(addr) | mask, addr);
}

static inline void clrbits(uint32_t addr, uint32_t mask)
{
	writel(read32(addr) & ~mask, addr);
}

void psci_clean_dcache_range(uintptr_t addr, size_t size)
{
	const uintptr_t line_mask = (uintptr_t)(PSCI_DCACHE_LINE_SIZE - 1u);
	uintptr_t start = addr & ~line_mask;
	uintptr_t end = (addr + size + line_mask) & ~line_mask;

	for (uintptr_t cur = start; cur < end; cur += PSCI_DCACHE_LINE_SIZE)
		arm32_dcache_clean_line_by_mva(cur);

	__asm__ __volatile__("dsb sy" ::: "memory");
}

void psci_invalidate_icache_range(uintptr_t addr, size_t size)
{
	const uintptr_t line_mask = (uintptr_t)(PSCI_DCACHE_LINE_SIZE - 1u);
	uintptr_t start = addr & ~line_mask;
	uintptr_t end = (addr + size + line_mask) & ~line_mask;

	for (uintptr_t cur = start; cur < end; cur += PSCI_DCACHE_LINE_SIZE)
		arm32_icache_invalidate_line_by_mva(cur);

	__asm__ __volatile__("dsb sy" ::: "memory");
	__asm__ __volatile__("isb" ::: "memory");
}

static inline bool psci_addr_in_sdram(uintptr_t addr, size_t size)
{
	const uintptr_t base = (uintptr_t)SDRAM_BASE;
	const uintptr_t top = (uintptr_t)SDRAM_TOP;

	if (addr < base)
		return false;
	if ((addr + size) > top)
		return false;
	return true;
}

#if LOG_LEVEL >= LOG_DEBUG
struct psci_linux_secondary_data_snapshot {
	uint32_t words[5];
};

static inline uint64_t psci_snapshot_pgdir(const struct psci_linux_secondary_data_snapshot *snap)
{
	return (((uint64_t)snap->words[1]) << 32) | (uint64_t)snap->words[0];
}

static inline uint32_t psci_snapshot_swapper(const struct psci_linux_secondary_data_snapshot *snap)
{
	return snap->words[2];
}

static inline uint32_t psci_snapshot_stack(const struct psci_linux_secondary_data_snapshot *snap)
{
	return snap->words[3];
}

static inline uint32_t psci_snapshot_task(const struct psci_linux_secondary_data_snapshot *snap)
{
	return snap->words[4];
}

static void psci_capture_secondary_data(uint32_t entry_point)
{
#if PSCI_LINUX_SECONDARY_DATA_OFFSET
	const uintptr_t entry_phys =
		((uintptr_t)entry_point) & ~(uintptr_t)0x1u; /* strip Thumb bit */
	const uintptr_t src = entry_phys +
		(uintptr_t)PSCI_LINUX_SECONDARY_DATA_OFFSET;
	if (!psci_addr_in_sdram(src, sizeof(struct psci_linux_secondary_data_snapshot)))
		return;

	struct psci_linux_secondary_data_snapshot snapshot;
	memcpy(&snapshot, (const void *)src, sizeof(snapshot));

#if PSCI_TRACE_ENABLE
	const uint32_t *raw = (const uint32_t *)src;
	PSCI_TRACE_DEBUG("PSCI: secondary_data raw src=0x%08" PRIx32
	                 " w0=0x%08" PRIx32 " w1=0x%08" PRIx32
	                 " w2=0x%08" PRIx32 " w3=0x%08" PRIx32
	                 " w4=0x%08" PRIx32 "\r\n",
	                 (uint32_t)src,
	                 raw[0], raw[1], raw[2], raw[3], raw[4]);

	const uint32_t *snap_words = (const uint32_t *)&snapshot;
	PSCI_TRACE_DEBUG("PSCI: secondary_data snap w0=0x%08" PRIx32
	                 " w1=0x%08" PRIx32 " w2=0x%08" PRIx32
	                 " w3=0x%08" PRIx32 " w4=0x%08" PRIx32 "\r\n",
	                 snap_words[0], snap_words[1], snap_words[2],
	                 snap_words[3], snap_words[4]);
#endif
	memcpy((void *)(uintptr_t)PSCI_NS_LINUX_SECONDARY_PHYS,
	       &snapshot,
	       sizeof(snapshot));
	psci_clean_dcache_range(PSCI_NS_LINUX_SECONDARY_PHYS,
				sizeof(snapshot));
#if PSCI_TRACE_ENABLE
	const uint64_t pgdir = psci_snapshot_pgdir(&snapshot);
	PSCI_TRACE_DEBUG("PSCI: CPU secondary_data mem  pgdir=0x%016llx"
	                 " swapper=0x%08" PRIx32 " stack=0x%08" PRIx32
	                 " task=0x%08" PRIx32 "\r\n",
	                 (unsigned long long)pgdir,
	                 psci_snapshot_swapper(&snapshot),
	                 psci_snapshot_stack(&snapshot),
	                 psci_snapshot_task(&snapshot));
#else
	(void)snapshot;
#endif
#else
	(void)entry_point;
#endif
}
#else
static inline void psci_capture_secondary_data(uint32_t entry_point)
{
	(void)entry_point;
}
#endif

static uint32_t psci_wait_for_stage_nonzero(uint32_t timeout_us)
{
	uint32_t stage = 0u;

	while (timeout_us-- > 0u) {
		stage = read32(PSCI_NS_STAGE_PHYS);
		if (stage != 0u)
			break;
		udelay(1);
	}

	return stage;
}

static uint32_t psci_wait_for_stage_at_least(uint32_t min_stage, uint32_t timeout_us)
{
	uint32_t stage = 0u;

	while (timeout_us-- > 0u) {
		stage = read32(PSCI_NS_STAGE_PHYS);
		if (stage >= min_stage)
			break;
		udelay(1);
	}

	return stage;
}

#if PSCI_TRACE_ENABLE
static void psci_log_stage_snapshot(const char *reason, uint32_t core, uint32_t stage)
{
	const uint32_t diag_base = PSCI_NS_DIAG_PHYS;
	const uint32_t diag_stage UNUSED_DEBUG = read32(diag_base + 0u);
	const uint32_t diag_mpidr UNUSED_DEBUG = read32(diag_base + 4u);
	const uint32_t diag_cpsr UNUSED_DEBUG = read32(diag_base + 8u);
	const uint32_t diag_stack UNUSED_DEBUG = read32(diag_base + 24u);
	const uint32_t diag_task UNUSED_DEBUG = read32(diag_base + 28u);
	const uint32_t diag_stub_entry = read32(diag_base + 32u);
	const uint32_t diag_stub_w0 = read32(diag_base + 36u);
	const uint32_t diag_stub_w1 = read32(diag_base + 40u);
	const uint32_t diag_shim_entry = read32(diag_base + 60u);
	const uint32_t diag_shim_w0 = read32(diag_base + 64u);
	const uint32_t diag_shim_w1 = read32(diag_base + 68u);
	const uint32_t trace_base = PSCI_NS_TRACE_PHYS;
	const uint32_t trace_tag = read32(trace_base + 0u);
	const uint32_t trace_cpsr_before = read32(trace_base + 4u);
	const uint32_t trace_cpsr_after = read32(trace_base + 8u);

	debug("PSCI: CPU%u %s stage=0x%08" PRIx32 " diag_stage=0x%08" PRIx32
	      " mpidr=0x%08" PRIx32 " cpsr=0x%08" PRIx32
	      " stack=0x%08" PRIx32 " task=0x%08" PRIx32
	      " stub_entry=0x%08" PRIx32 " stub_w0=0x%08" PRIx32
	      " stub_w1=0x%08" PRIx32
	      " shim_entry=0x%08" PRIx32 " shim_w0=0x%08" PRIx32
	      " shim_w1=0x%08" PRIx32
	      " trace_tag=0x%08" PRIx32 " trace_cpsr_pre=0x%08" PRIx32
	      " trace_cpsr_post=0x%08" PRIx32 "\r\n",
	      (unsigned int)core,
	      reason,
	      stage,
	      diag_stage,
	      diag_mpidr,
	      diag_cpsr,
	      diag_stack,
	      diag_task,
	      diag_stub_entry,
	      diag_stub_w0,
	      diag_stub_w1,
	      diag_shim_entry,
	      diag_shim_w0,
	      diag_shim_w1,
	      trace_tag,
	      trace_cpsr_before,
	      trace_cpsr_after);
}
#else
static inline void psci_log_stage_snapshot(const char *reason, uint32_t core, uint32_t stage)
{
	(void)reason;
	(void)core;
	(void)stage;
}
#endif

#if PSCI_TRACE_ENABLE
static void psci_log_secure_context(uint32_t core)
{
	const uint32_t scr = arm32_read_scr();
	const uint32_t sctlr = arm32_read_p15_c1();
	const uint32_t cpsr = arm32_cpsr_read();
	const uint32_t id_pfr1 = arm32_read_id_pfr1();
	const uint32_t virt_field = (id_pfr1 >> 4) & 0xfu;
	uint32_t hvbar = 0u;
	uint32_t hcr = 0u;
	uint32_t hcptr = 0u;
	uint32_t hdcr = 0u;

	if (virt_field != 0u) {
		hvbar = arm32_read_hvbar();
		hcr = arm32_read_hcr();
		hcptr = arm32_read_hcptr();
		hdcr = arm32_read_hdcr();
	}

	PSCI_TRACE_DEBUG("PSCI: CPU%u secure ctx scr=0x%08" PRIx32
			 " sctlr=0x%08" PRIx32 " cpsr=0x%08" PRIx32
			 " id_pfr1=0x%08" PRIx32
			 " hvbar=0x%08" PRIx32 " hcr=0x%08" PRIx32
			 " hcptr=0x%08" PRIx32 " hdcr=0x%08" PRIx32 "\r\n",
			 (unsigned int)core, scr, sctlr, cpsr, id_pfr1,
			 hvbar, hcr, hcptr, hdcr);
}
#else
static inline void psci_log_secure_context(uint32_t core)
{
	(void)core;
}
#endif

static void psci_configure_hyp_environment(uint32_t entry_target)
{
	(void)entry_target;
}

static void sunxi_r_cpucfg_enable(void)
{
	uint32_t val = read32(SUNXI_R_CPUCFG_CLK_REG);

	if ((val & SUNXI_R_CPUCFG_CLK_GATE) && (val & SUNXI_R_CPUCFG_CLK_RST))
		return;

	setbits(SUNXI_R_CPUCFG_CLK_REG, SUNXI_R_CPUCFG_CLK_GATE | SUNXI_R_CPUCFG_CLK_RST);
	dsb();
	isb();
}

static void sunxi_cpu_disable_power(unsigned int cluster, unsigned int core)
{
	if (read32(SUNXI_CPU_POWER_CLAMP_REG(cluster, core)) == 0xffu)
		return;
	writel(0xffu, SUNXI_CPU_POWER_CLAMP_REG(cluster, core));
}

static void sunxi_cpu_enable_power(unsigned int cluster, unsigned int core)
{
	if (read32(SUNXI_CPU_POWER_CLAMP_REG(cluster, core)) == 0u)
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
	const uint32_t entry_target = entry_point;

	const size_t stub_size = (size_t)(psci_ns_stub_end - psci_ns_stub_start);
	const size_t shmem_header_size =
		(size_t)(PSCI_NS_STUB_PHYS - PSCI_NS_SHMEM_BASE);
	const size_t shmem_header_clear_size = shmem_header_size;

	dmb();
	memset((void *)PSCI_NS_SHMEM_BASE, 0, shmem_header_clear_size);
	memset((void *)PSCI_NS_STUB_PHYS, 0, stub_size);
	uint8_t *stub_dest = (uint8_t *)(uintptr_t)PSCI_NS_STUB_PHYS;
	memcpy(stub_dest, psci_ns_stub_start, stub_size);

	const uintptr_t stub_src_base = (uintptr_t)psci_ns_stub_start;
	const uintptr_t diag_off = (uintptr_t)psci_ns_stub_diag_lit - stub_src_base;
	const uintptr_t stage_off = (uintptr_t)psci_ns_stub_stage_lit - stub_src_base;
	const uintptr_t sram_off = (uintptr_t)psci_ns_stub_sram_lit - stub_src_base;
	const uintptr_t trace_off = (uintptr_t)psci_ns_stub_trace_lit - stub_src_base;
	const uintptr_t secdata_off = (uintptr_t)psci_ns_stub_secdata_lit - stub_src_base;
	const uintptr_t context_off = (uintptr_t)psci_ns_stub_context_lit - stub_src_base;
	const uintptr_t entry_off = (uintptr_t)psci_ns_stub_entry_lit - stub_src_base;
	uint32_t *diag_ptr = (uint32_t *)(stub_dest + diag_off);
	uint32_t *stage_ptr = (uint32_t *)(stub_dest + stage_off);
	uint32_t *sram_ptr = (uint32_t *)(stub_dest + sram_off);
	uint32_t *trace_ptr = (uint32_t *)(stub_dest + trace_off);
	uint32_t *secdata_ptr = (uint32_t *)(stub_dest + secdata_off);
	uint32_t *context_ptr = (uint32_t *)(stub_dest + context_off);
	uint32_t *entry_ptr = (uint32_t *)(stub_dest + entry_off);
	diag_ptr[0] = PSCI_NS_DIAG_PHYS;
	stage_ptr[0] = PSCI_NS_STAGE_PHYS;
	sram_ptr[0] = PSCI_NS_TEST_PHYS;
	trace_ptr[0] = PSCI_NS_TRACE_PHYS;
	secdata_ptr[0] = PSCI_NS_LINUX_SECONDARY_PHYS;
	context_ptr[0] = context_id;
	psci_ns_entry_prepare(entry_target);
	entry_ptr[0] = entry_target | 0x1u;
	psci_capture_secondary_data(entry_point);
	psci_configure_hyp_environment(entry_target);

#if PSCI_TRACE_ENABLE
	const uint32_t shim_word0 = read32(PSCI_NS_LINUX_ENTRY_PHYS);
	const uint32_t shim_word1 = read32(PSCI_NS_LINUX_ENTRY_PHYS + 4u);
	const uint32_t target_slot = read32(PSCI_NS_ENTRY_TARGET_PHYS);

	PSCI_TRACE_DEBUG("PSCI: shim copy first words w0=0x%08" PRIx32
			 " w1=0x%08" PRIx32 "\r\n",
			 shim_word0, shim_word1);
	PSCI_TRACE_DEBUG("PSCI: target slot @0x%08" PRIx32 " =0x%08" PRIx32 "\r\n",
			 (uint32_t)PSCI_NS_ENTRY_TARGET_PHYS, target_slot);
	PSCI_TRACE_DEBUG("PSCI: shmem header dump @0x%08" PRIx32 ":", (uint32_t)PSCI_NS_SHMEM_BASE);
	for (size_t idx = 0; idx < 128u; idx += 16u) {
		const uint32_t w0 = read32(PSCI_NS_SHMEM_BASE + idx + 0u);
		const uint32_t w1 = read32(PSCI_NS_SHMEM_BASE + idx + 4u);
		const uint32_t w2 = read32(PSCI_NS_SHMEM_BASE + idx + 8u);
		const uint32_t w3 = read32(PSCI_NS_SHMEM_BASE + idx + 12u);

		PSCI_TRACE_DEBUG("  +0x%02zx: %08" PRIx32 " %08" PRIx32
				 " %08" PRIx32 " %08" PRIx32 "\r\n",
				 idx, w0, w1, w2, w3);
	}
#endif

	psci_clean_dcache_range((uintptr_t)PSCI_NS_SHMEM_BASE, shmem_header_clear_size);
	psci_clean_dcache_range((uintptr_t)PSCI_NS_STUB_PHYS, stub_size);
	psci_invalidate_icache_range((uintptr_t)PSCI_NS_STUB_PHYS, stub_size);

	PSCI_TRACE_DEBUG("PSCI: trampoline literal entry=0x%08" PRIx32 "\r\n",
			 entry_ptr[0]);
}
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

	PSCI_TRACE_DEBUG("PSCI: CPU%lu ON requested (entry=0x%08" PRIx32 ", ctx=0x%08" PRIx32 ")\r\n",
			 core, entry_phys, context_id);

	psci_cpu_states[core] = PSCI_AFFINITY_ON_PENDING;
	psci_cpu_last_context[core] = context_id;

	psci_trampoline_write(entry_phys, context_id);
	psci_log_secure_context(core);

	writel(0u, PSCI_NS_STAGE_PHYS);

	psci_cpu_entry_phys[core] = PSCI_NS_STUB_PHYS;

	writel((uint32_t)(uintptr_t)&psci_cpu_secure_entry, SUNXI_CPU_SOFT_ENTRY_REG);

	/* Hold core in reset while preparing hand-off (matches U-Boot flow) */
	clrbits(SUNXI_CPUCFG_C0_RST_CTRL, BIT(core));
	dsb();
	isb();
	
	/* Invalidate L1 cache by clearing CTRL_REG0 bit (R528 semantics) */
	clrbits(SUNXI_CPUCFG_C0_CTRL_REG0, BIT(core));
	dsb();
	isb();
	/* Ensure core power domain is ready (matches earlier bring-up flow) */
	sunxi_cpu_enable_power(cluster, core);
	clrbits(SUNXI_POWEROFF_GATING_REG(cluster), BIT(core));
	setbits(SUNXI_POWERON_RST_REG(cluster), BIT(core));
	setbits(SUNXI_CPUCFG_DBG_REG0, BIT(core));
	dsb();
	isb();
	/* Ensure CPU will come up in AArch32 */
	clrbits(SUNXI_CPUCFG_GEN_CTRL_REG0, SUNXI_AARCH_CTRL_BIT(core));

	setbits(SUNXI_CPUCFG_C0_RST_CTRL, BIT(core));
	dsb();
	isb();

	__asm__ __volatile__("sev" ::: "memory");

	uint32_t stage = psci_wait_for_stage_nonzero(200u);
	if (stage == 0u) {
		error("PSCI: CPU%u stage did not start within 200us (entry=0x%08" PRIx32 ")\r\n",
		      (unsigned int)core, entry_phys);
	} else {
		psci_log_stage_snapshot("stage", core, stage);
	}
	const uintptr_t entry_lit_phys = PSCI_NS_STUB_PHYS +
		((uintptr_t)psci_ns_stub_entry_lit - (uintptr_t)psci_ns_stub_start);
	uint32_t stage_final = psci_wait_for_stage_at_least(0x53u, 500u);
	if (stage_final >= 0x53u) {
		psci_log_stage_snapshot("stage final", core, stage_final);
#if PSCI_TRACE_ENABLE
		const uint32_t trace_last = read32(PSCI_NS_TRACE_PHYS);
		PSCI_TRACE_DEBUG("PSCI: CPU%u trace last=0x%08" PRIx32 "\r\n",
				 (unsigned int)core, trace_last);
#endif
	} else {
		const uint32_t entry_lit_debug = read32(entry_lit_phys);
		error("PSCI: CPU%u stage failed to reach 0x53 (last=0x%08" PRIx32
		      ", entry_lit=0x%08" PRIx32 ")\r\n",
		      (unsigned int)core, stage_final, entry_lit_debug);
	}

	psci_log_monitor_diag();

	psci_cpu_states[core] = PSCI_AFFINITY_ON;
	return PSCI_RET_SUCCESS;
}

static int32_t psci_do_cpu_off(void)
{
	const uint32_t mpidr = arm32_read_mpidr();
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

	const char *label = "non-secure handoff test";
	psci_selftest_prepare_environment();
	const uint32_t entry = psci_selftest_entry_phys();
	const uint32_t ctx = PSCI_SELFTEST_CONTEXT_NS;

	psci_cpu1_probe_magic = 0u;
	psci_cpu1_probe_counter = 0u;
	psci_cpu1_probe_mpidr = 0u;
	psci_cpu1_probe_cpsr = 0u;
	writel(0u, PSCI_NS_DIAG_PHYS + 0u);
	writel(0u, PSCI_NS_DIAG_PHYS + 4u);
	writel(0u, PSCI_NS_DIAG_PHYS + 8u);
	writel(0u, PSCI_NS_STAGE_PHYS);
	writel(0xdeadbeefu, PSCI_NS_TEST_PHYS);
	writel(0u, PSCI_NS_TRACE_PHYS);
	dsb();
	isb();

	const int32_t ret = psci_smc_call(PSCI_0_2_FN_CPU_ON, 1u, entry, ctx);

	if (ret != PSCI_RET_SUCCESS) {
		fatal("PSCI: CPU1 %s failed -> %" PRId32 "\r\n",
					label, ret);
	}

	bool cpu1_alive = false;
	uint64_t start = time_ms();
	while (time_ms() - start < 10u) {
		if (psci_cpu1_probe_magic == PSCI_CPU1_PROBE_MAGIC) {
#if PSCI_TRACE_ENABLE
			const uint32_t counter = psci_cpu1_probe_counter;
			const uint32_t mpidr = psci_cpu1_probe_mpidr;
			const uint32_t cpsr = psci_cpu1_probe_cpsr;
			PSCI_TRACE_DEBUG("PSCI: CPU1 %s alive (mpidr=0x%08" PRIx32
					 ", counter=%" PRIu32 " cpsr=0x%08" PRIx32 ")\r\n",
					 label, mpidr, counter, cpsr);
#endif
			cpu1_alive = true;
			break;
		}

		udelay(10);
	}

	if (!cpu1_alive) {
#if PSCI_TRACE_ENABLE
		const uint32_t diag_cnt = read32(PSCI_NS_DIAG_PHYS + 0u);
		const uint32_t diag_mpidr = read32(PSCI_NS_DIAG_PHYS + 4u);
		const uint32_t diag_cpsr = read32(PSCI_NS_DIAG_PHYS + 8u);
		const uint32_t diag_stage = read32(PSCI_NS_STAGE_PHYS);
		const uint32_t diag_trace = read32(PSCI_NS_TRACE_PHYS);
		PSCI_TRACE_DEBUG("PSCI: CPU1 %s diag cnt=%" PRIu32 " mpidr=0x%08" PRIx32
				 " cpsr=0x%08" PRIx32 " stage=0x%08" PRIx32
				 " trace=0x%08" PRIx32 "\r\n",
				 label, diag_cnt, diag_mpidr, diag_cpsr, diag_stage, diag_trace);
		PSCI_TRACE_DEBUG("PSCI: CPU1 %s probe magic=0x%08" PRIx32
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
			PSCI_TRACE_DEBUG("PSCI: monitor %s fault reason(raw)=0x%08" PRIx32
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
#endif
		fatal("PSCI: CPU1 %s entry=0x%08" PRIx32
					" probe failed\r\n",
					label, entry);
	}

	psci_selftest_validate_environment();

	/* Park CPU1 back into reset so PSCI flow can own it again */
	clrbits(SUNXI_CPUCFG_C0_RST_CTRL, BIT(1u));
	clrbits(SUNXI_CPUCFG_DBG_REG0, BIT(1u));
	setbits(SUNXI_CPUCFG_C0_CTRL_REG0, BIT(1u));
	writel(0u, SUNXI_CPU_SOFT_ENTRY_REG);
	udelay(1);
	dsb();
	isb();

	psci_cpu_states[1] = PSCI_AFFINITY_OFF;

#if PSCI_TRACE_ENABLE
	const uint32_t rst = read32(SUNXI_CPUCFG_C0_RST_CTRL);
	const uint32_t ctrl = read32(SUNXI_CPUCFG_C0_CTRL_REG0);
	const uint32_t dbg = read32(SUNXI_CPUCFG_DBG_REG0);

	PSCI_TRACE_DEBUG("PSCI: CPU1 %s complete, reset re-asserted "
			 "(rst=0x%08" PRIx32 " ctrl0=0x%08" PRIx32
			 " dbg=0x%08" PRIx32 ")\r\n",
			 label, rst, ctrl, dbg);
#endif
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

	PSCI_TRACE_DEBUG("PSCI: monitor vectors @0x%08" PRIx32 "\r\n", mvbar);
	arm32_write_mvbar(mvbar);
	dsb();
	isb();
	PSCI_TRACE_DEBUG("PSCI: mvbar readback 0x%08" PRIx32 "\r\n", arm32_read_mvbar());

#if PSCI_CPU_SELFTEST
		psci_cpu1_manual_bringup();
#endif
}

int32_t psci_handle_smc(uint32_t fid, uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
	int32_t ret;

	switch (fid) {
#if PSCI_CPU_SELFTEST
	case PSCI_SELFTEST_NS_FID:
		psci_cpu1_probe_magic = PSCI_CPU1_PROBE_MAGIC;
		psci_cpu1_probe_mpidr = arg0;
		psci_cpu1_probe_cpsr = arg1;
		psci_cpu1_probe_counter = arg2;
		ret = PSCI_RET_SUCCESS;
		break;
#endif
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

#if PSCI_TRACE_ENABLE
	const uint32_t count = ++psci_smc_count;
	PSCI_TRACE_DEBUG("PSCI[%" PRIu32 "]: fid=0x%08" PRIx32 " a0=0x%08" PRIx32
			 " a1=0x%08" PRIx32 " -> %" PRId32 "\r\n",
			 count, fid, arg0, arg1, ret);
#endif

	return ret;
}
