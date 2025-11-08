#include <inttypes.h>
#include "common.h"
#include "debug.h"
#include "barrier.h"
#include "psci_selftest.h"
#include "psci.h"

#if PSCI_CPU_SELFTEST

extern const uint32_t psci_ns_stage_phys_value;
extern const uint32_t psci_ns_diag_phys_value;
extern const uint32_t psci_ns_linux_secondary_phys_value;
extern const uint32_t psci_ns_trace_phys_value;
extern const uint32_t psci_ns_task_size_value;

enum {
	PSCI_NS_STAGE_SELFTEST_FINAL_VAL = 0x53u,
};
extern const uint32_t psci_ns_stack_phys_value;
extern const uint32_t psci_ns_stack_size_value;
extern const uint32_t psci_ns_task_phys_value;
extern const uint64_t psci_ns_pgdir_phys_value;
extern const uint32_t psci_ns_swapper_phys_value;

struct psci_selftest_plan {
	uint64_t pgdir;
	uint32_t swapper_pg_dir;
	uint32_t stack_base;
	uint32_t stack_top;
	uint32_t task;
};

struct psci_ns_linux_secondary_data_layout {
	uint32_t pgdir_lo;
	uint32_t pgdir_hi;
	uint32_t swapper_pg_dir;
	uint32_t stack;
	uint32_t task;
};

static struct psci_selftest_plan psci_selftest_plan_state;

static void psci_selftest_payload(void);

void psci_selftest_prepare_environment(void)
{
	psci_selftest_plan_state.pgdir = psci_ns_pgdir_phys_value;
	psci_selftest_plan_state.swapper_pg_dir = psci_ns_swapper_phys_value;
	psci_selftest_plan_state.stack_base = psci_ns_stack_phys_value;
	psci_selftest_plan_state.stack_top =
		psci_selftest_plan_state.stack_base + psci_ns_stack_size_value - 0x10u;
	psci_selftest_plan_state.task = psci_ns_task_phys_value;

	debug("PSCI: selftest secondary_data cfg pgdir=0x%016llx"
	      " swapper=0x%08" PRIx32 " stack_base=0x%08" PRIx32
	      " stack_top=0x%08" PRIx32 " task=0x%08" PRIx32 "\r\n",
	      (unsigned long long)psci_selftest_plan_state.pgdir,
	      psci_selftest_plan_state.swapper_pg_dir,
	      psci_selftest_plan_state.stack_base,
	      psci_selftest_plan_state.stack_top,
	      psci_selftest_plan_state.task);

	struct psci_ns_linux_secondary_data_layout *sec_cfg =
		(struct psci_ns_linux_secondary_data_layout *)(uintptr_t)psci_ns_linux_secondary_phys_value;
	sec_cfg->pgdir_lo = (uint32_t)(psci_selftest_plan_state.pgdir & 0xffffffffu);
	sec_cfg->pgdir_hi = (uint32_t)(psci_selftest_plan_state.pgdir >> 32);
	sec_cfg->swapper_pg_dir = psci_selftest_plan_state.swapper_pg_dir;
	sec_cfg->stack = psci_selftest_plan_state.stack_top;
	sec_cfg->task = psci_selftest_plan_state.task;

	memset((void *)(uintptr_t)psci_selftest_plan_state.stack_base, 0,
	       psci_ns_stack_size_value);
	memset((void *)(uintptr_t)psci_selftest_plan_state.task, 0, psci_ns_task_size_value);

	psci_clean_dcache_range((uintptr_t)sec_cfg, sizeof(*sec_cfg));
	psci_clean_dcache_range(psci_selftest_plan_state.stack_base,
				psci_ns_stack_size_value);
	psci_clean_dcache_range(psci_selftest_plan_state.task, psci_ns_task_size_value);
}

uint32_t psci_selftest_entry_phys(void)
{
	return (uint32_t)(uintptr_t)&psci_selftest_payload;
}

void psci_selftest_validate_environment(void)
{
	const uint32_t diag_base = psci_ns_diag_phys_value;
	const uint32_t diag_stage = read32(diag_base + 0u);
	const uint32_t diag_mpidr UNUSED_DEBUG = read32(diag_base + 4u);
	const uint32_t diag_pgdir_lo = read32(diag_base + 12u);
	const uint32_t diag_pgdir_hi = read32(diag_base + 44u);
	const uint32_t diag_swapper = read32(diag_base + 20u);
	const uint32_t diag_stack = read32(diag_base + 24u);
	const uint32_t diag_task = read32(diag_base + 28u);
	const uint32_t stage_reg = read32(psci_ns_stage_phys_value);
	const uint32_t trace_reg = read32(psci_ns_trace_phys_value);
	const uint64_t diag_pgdir =
		(((uint64_t)diag_pgdir_hi) << 32) | (uint64_t)diag_pgdir_lo;

	debug("PSCI: selftest diag stage=0x%08" PRIx32 " mpidr=0x%08" PRIx32
	      " stack=0x%08" PRIx32 " task=0x%08" PRIx32 "\r\n",
	      stage_reg, diag_mpidr, diag_stack, diag_task);

	if ((diag_stack != psci_selftest_plan_state.stack_top) ||
	    (diag_task != psci_selftest_plan_state.task) ||
	    (diag_pgdir != psci_selftest_plan_state.pgdir) ||
	    (diag_swapper != psci_selftest_plan_state.swapper_pg_dir) ||
	    (stage_reg != PSCI_NS_STAGE_SELFTEST_FINAL_VAL)) {
		fatal("PSCI: selftest validation failed (stage=0x%08" PRIx32
		      ", diag_stage=0x%08" PRIx32 " trace=0x%08" PRIx32 ")\r\n",
		      stage_reg, diag_stage, trace_reg);
	}
}

static void psci_selftest_payload(void)
{
	volatile uint32_t *stage_reg = (uint32_t *)(uintptr_t)psci_ns_stage_phys_value;
	volatile uint32_t *diag_base = (uint32_t *)(uintptr_t)psci_ns_diag_phys_value;
	const struct psci_ns_linux_secondary_data_layout *sec_cfg =
		(const struct psci_ns_linux_secondary_data_layout *)(uintptr_t)psci_ns_linux_secondary_phys_value;

	const uint32_t stack_top = sec_cfg->stack;
	if (stack_top != 0u)
		__asm__ __volatile__("mov sp, %0" :: "r"(stack_top));

	diag_base[0] = 0x60u;
	*stage_reg = 0x60u;

	const uint64_t pgdir =
		(((uint64_t)sec_cfg->pgdir_hi) << 32) | (uint64_t)sec_cfg->pgdir_lo;
	diag_base[3] = (uint32_t)(pgdir & 0xffffffffu);
	diag_base[11] = (uint32_t)(pgdir >> 32);
	diag_base[5] = sec_cfg->swapper_pg_dir;
	diag_base[6] = sec_cfg->stack;
	diag_base[7] = sec_cfg->task;

	diag_base[0] = 0x61u;
	*stage_reg = 0x61u;

	diag_base[0] = PSCI_NS_STAGE_SELFTEST_FINAL_VAL;
	*stage_reg = PSCI_NS_STAGE_SELFTEST_FINAL_VAL;

	for (;;) {
		__asm__ __volatile__("wfe");
	}
}

#endif
