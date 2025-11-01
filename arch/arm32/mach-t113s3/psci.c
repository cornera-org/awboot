#include "common.h"
#include "psci.h"
#include <stdbool.h>
#include "sunxi_cpucfg.h"
#include "sunxi_wdg.h"
#include "board.h"
#include "barrier.h"
#include "debug.h"

struct psci_cpu_ctx psci_cpu_data[PSCI_MAX_CPUS] PSCI_DATA_SECTION PSCI_ALIGNED(16) = { 0 };

volatile uint32_t psci_ns_entered PSCI_DATA_SECTION = 0U;

extern void psci_secondary_entry(void);
extern void psci_init_asm(void);
uint32_t psci_has_virt = 0U;

void psci_debug_hyp_trap(uint32_t hsr, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3) PSCI_TEXT_SECTION;
void psci_debug_enter_ns(uint32_t entry, uint32_t arg0, uint32_t arg1, uint32_t arg2) PSCI_TEXT_SECTION;
void psci_debug_mon_call(uint32_t fid, uint32_t arg0, uint32_t arg1, uint32_t arg2) PSCI_TEXT_SECTION;
void psci_debug_mon_prepare(uint32_t entry, uint32_t arg0, uint32_t arg1, uint32_t arg2) PSCI_TEXT_SECTION;

void psci_debug_hyp_trap(uint32_t hsr, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3)
{
	debug("PSCI: hyp trap hsr=0x%08" PRIx32
	      " r0=0x%08" PRIx32 " r1=0x%08" PRIx32
	      " r2=0x%08" PRIx32 " r3=0x%08" PRIx32 "\r\n",
	      hsr, r0, r1, r2, r3);
}

void psci_debug_enter_ns(uint32_t entry, uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
	debug("PSCI: enter_non_secure entry=0x%08" PRIx32
	      " arg0=0x%08" PRIx32 " arg1=0x%08" PRIx32
	      " arg2=0x%08" PRIx32 "\r\n",
	      entry, arg0, arg1, arg2);
}

void psci_debug_mon_call(uint32_t fid, uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
	debug("PSCI: monitor SMC fid=0x%08" PRIx32
	      " arg0=0x%08" PRIx32 " arg1=0x%08" PRIx32
	      " arg2=0x%08" PRIx32 "\r\n",
	      fid, arg0, arg1, arg2);
}

void psci_debug_mon_prepare(uint32_t entry, uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
	debug("PSCI: preparing NS entry=0x%08" PRIx32
	      " arg0=0x%08" PRIx32 " arg1=0x%08" PRIx32
	      " arg2=0x%08" PRIx32 "\r\n",
	      entry, arg0, arg1, arg2);
}

void psci_debug_pre_smc(uint32_t scr, uint32_t hcr)
{
	debug("PSCI: pre-SMC SCR=0x%08" PRIx32 " HCR=0x%08" PRIx32 "\r\n", scr, hcr);
}

void psci_enter_return_fail(void)
{
	fatal("PSCI: smc returned unexpectedly\r\n");
}

#define PSCI_0_2_FN_PSCI_VERSION      0x84000000U
#define PSCI_0_2_FN_CPU_SUSPEND       0x84000001U
#define PSCI_0_2_FN_CPU_OFF           0x84000002U
#define PSCI_0_2_FN_CPU_ON            0x84000003U
#define PSCI_0_2_FN_AFFINITY_INFO     0x84000004U
#define PSCI_0_2_FN_MIGRATE_INFO_TYPE 0x84000006U
#define PSCI_0_2_FN_SYSTEM_OFF        0x84000008U
#define PSCI_0_2_FN_SYSTEM_RESET      0x84000009U
#define PSCI_0_2_FN_FEATURES          0x8400000AU

#define PSCI_0_2_FN64_CPU_SUSPEND     0xC4000001U
#define PSCI_0_2_FN64_CPU_ON          0xC4000003U
#define PSCI_0_2_FN64_AFFINITY_INFO   0xC4000004U

#define MPIDR_U_BIT   BIT(30)
#define MPIDR_AFFLVL_MASK   0xffU
#define MPIDR_AFFLVL_SHIFT  8U
#define MPIDR_AFF0(mpidr)   ((mpidr) & MPIDR_AFFLVL_MASK)
#define MPIDR_AFF1(mpidr)   (((mpidr) >> MPIDR_AFFLVL_SHIFT) & MPIDR_AFFLVL_MASK)
#define MPIDR_AFF2(mpidr)   (((mpidr) >> (MPIDR_AFFLVL_SHIFT * 2U)) & MPIDR_AFFLVL_MASK)

static inline uint32_t read_mpidr(void)
{
	uint32_t mpidr;
	__asm__ __volatile__("mrc p15, 0, %0, c0, c0, 5" : "=r"(mpidr));
	return mpidr;
}

static int psci_mpidr_to_cpu(uint32_t mpidr)
{
	mpidr &= ~MPIDR_U_BIT;

	if ((MPIDR_AFF2(mpidr) != 0U) || (MPIDR_AFF1(mpidr) != 0U))
		return -1;

	uint32_t cpu = MPIDR_AFF0(mpidr);
	return (cpu < PSCI_MAX_CPUS) ? (int)cpu : -1;
}

static inline void enable_smp_bit(void)
{
	uint32_t actlr;
	__asm__ __volatile__("mrc p15, 0, %0, c1, c0, 1" : "=r"(actlr));
	actlr |= (1U << 6);
	__asm__ __volatile__("mcr p15, 0, %0, c1, c0, 1" : : "r"(actlr) : "memory");
	dsb();
	isb();
  trace("PSCI: SMP bit enabled in ACTLR\r\n");
}

static uint32_t psci_features_fn(uint32_t fid) PSCI_TEXT_SECTION;
static uint32_t psci_features_fn(uint32_t fid)
{
	switch (fid) {
		case PSCI_0_2_FN_PSCI_VERSION:
		case PSCI_0_2_FN_CPU_OFF:
		case PSCI_0_2_FN_CPU_ON:
		case PSCI_0_2_FN_AFFINITY_INFO:
		case PSCI_0_2_FN_SYSTEM_OFF:
		case PSCI_0_2_FN_SYSTEM_RESET:
		case PSCI_0_2_FN_FEATURES:
			return PSCI_RET_SUCCESS;
		default:
			return PSCI_RET_NOT_SUPPORTED;
	}
}

static uint32_t psci_cpu_on_impl(uint32_t target_cpu, uint32_t entry_point, uint32_t context_id) PSCI_TEXT_SECTION;
static uint32_t psci_cpu_on_impl(uint32_t target_cpu, uint32_t entry_point, uint32_t context_id)
{
	int cpu = psci_mpidr_to_cpu(target_cpu);
	debug("PSCI: CPU_ON mpidr=0x%08" PRIx32 " cpu=%d entry=0x%08" PRIx32 " ctx=0x%08" PRIx32 "\r\n",
	      target_cpu, cpu, entry_point, context_id);
	if (cpu < 0)
		return PSCI_RET_INVALID_PARAMETERS;

	if (cpu == 0)
		return PSCI_RET_ALREADY_ON;

	dmb();
	struct psci_cpu_ctx *ctx = &psci_cpu_data[(uint32_t)cpu];

	switch (ctx->state) {
		case PSCI_CPU_STATE_ON:
			return PSCI_RET_ALREADY_ON;
		case PSCI_CPU_STATE_ON_PENDING:
			return PSCI_RET_ON_PENDING;
		default:
			break;
	}

	if ((entry_point & 0x3U) != 0U)
		return PSCI_RET_INVALID_ADDRESS;

	ctx->entry   = entry_point;
	ctx->context = context_id;
	ctx->state   = PSCI_CPU_STATE_ON_PENDING;
	dmb();

	if (sunxi_cpucfg_cpu_on((uint32_t)cpu, (uint32_t)(uintptr_t)&psci_secondary_entry) != 0)
	{
		error("PSCI: sunxi_cpucfg_cpu_on failed for cpu %d\r\n", cpu);
		ctx->state = PSCI_CPU_STATE_OFF;
		return PSCI_RET_INTERNAL_FAILURE;
	}

	debug("PSCI: CPU_ON cpu=%d accepted\r\n", cpu);
	return PSCI_RET_SUCCESS;
}

static uint32_t psci_affinity_info_impl(uint32_t target_affinity, uint32_t level) PSCI_TEXT_SECTION;
static uint32_t psci_affinity_info_impl(uint32_t target_affinity, uint32_t level)
{
	if (level > 0U)
		return PSCI_RET_INVALID_PARAMETERS;

	int cpu = psci_mpidr_to_cpu(target_affinity);
	if (cpu < 0)
		return PSCI_RET_INVALID_PARAMETERS;

	if ((cpu != 0) && sunxi_cpucfg_cpu_is_on((uint32_t)cpu) &&
	    (psci_cpu_data[(uint32_t)cpu].state == PSCI_CPU_STATE_OFF)) {
		psci_cpu_data[(uint32_t)cpu].state = PSCI_CPU_STATE_ON;
	}

	switch (psci_cpu_data[(uint32_t)cpu].state) {
		case PSCI_CPU_STATE_ON:
			return PSCI_AFFINITY_INFO_ON;
		case PSCI_CPU_STATE_ON_PENDING:
			return PSCI_AFFINITY_INFO_ON_PENDING;
		default:
			return PSCI_AFFINITY_INFO_OFF;
	}
}

static void psci_system_off(void) __attribute__((noreturn)) PSCI_TEXT_SECTION;
static void psci_system_reset(void) __attribute__((noreturn)) PSCI_TEXT_SECTION;

static void psci_cpu_off_local(void) __attribute__((noreturn)) PSCI_TEXT_SECTION;
static void psci_cpu_off_local(void)
{
	uint32_t mpidr = read_mpidr();
	int cpu = psci_mpidr_to_cpu(mpidr);
	if (cpu < 0)
		psci_system_off();

	psci_cpu_data[(uint32_t)cpu].state = PSCI_CPU_STATE_OFF;
	psci_cpu_data[(uint32_t)cpu].entry = 0U;
	psci_cpu_data[(uint32_t)cpu].context = 0U;
	dmb();

	sunxi_cpucfg_cpu_off((uint32_t)cpu);

	while (1) {
		dsb();
		__asm__ __volatile__("wfi" ::: "memory");
	}
}

static void psci_system_off(void) __attribute__((noreturn)) PSCI_TEXT_SECTION;
static void psci_system_off(void)
{
	board_set_status(0);
	sunxi_wdg_set(1);

	while (1) {
		dsb();
		__asm__ __volatile__("wfi" ::: "memory");
	}
}

static void psci_system_reset(void) __attribute__((noreturn)) PSCI_TEXT_SECTION;
static void psci_system_reset(void)
{
	sunxi_wdg_set(1);

	while (1) {
		dsb();
		__asm__ __volatile__("wfi" ::: "memory");
	}
}

void psci_init(void)
{
	uint32_t id_pfr1;
	uint32_t scr;
	bool has_virt;

	__asm__ __volatile__("mrc p15, 0, %0, c0, c1, 1" : "=r"(id_pfr1));
	__asm__ __volatile__("mrc p15, 0, %0, c1, c1, 0" : "=r"(scr));
	has_virt = (((id_pfr1 >> 12) & 0xFU) != 0U);

	psci_has_virt = has_virt ? 1U : 0U;
	debug("PSCI: ID_PFR1=0x%08" PRIx32 " SCR=0x%08" PRIx32 "\r\n", id_pfr1, scr);
	if (has_virt) {
		debug("PSCI: virtualization extensions detected\r\n");
	} else {
		debug("PSCI: virtualization extensions unavailable, using SMC path\r\n");
	}
	enable_smp_bit();
	sunxi_cpucfg_init();

	for (uint32_t i = 0; i < PSCI_MAX_CPUS; ++i) {
		psci_cpu_data[i].entry   = 0U;
		psci_cpu_data[i].context = 0U;
		psci_cpu_data[i].reserved = 0U;
		if ((i != 0U) && sunxi_cpucfg_cpu_is_on(i)) {
			psci_cpu_data[i].state = PSCI_CPU_STATE_ON;
      debug("PSCI: CPU %" PRIu32 " already ON\r\n", i);
		} else {
			psci_cpu_data[i].state = (i == 0U) ? PSCI_CPU_STATE_ON : PSCI_CPU_STATE_OFF;
      debug("PSCI: CPU %" PRIu32 " state %s\r\n", i,
            (psci_cpu_data[i].state == PSCI_CPU_STATE_ON) ? "ON" : "OFF");
		}
	}

}

uint32_t psci_smc_handler(uint32_t fid, uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
	debug("PSCI: fid=0x%08" PRIx32 " arg0=0x%08" PRIx32 " arg1=0x%08" PRIx32 " arg2=0x%08" PRIx32 "\r\n",
	      fid, arg0, arg1, arg2);
	switch (fid) {
		case PSCI_0_2_FN_PSCI_VERSION:
			return PSCI_VERSION(1, 0);

		case PSCI_0_2_FN_FEATURES:
			return psci_features_fn(arg0);

		case PSCI_0_2_FN_CPU_ON:
			return psci_cpu_on_impl(arg0, arg1, arg2);

		case PSCI_0_2_FN_CPU_OFF: {
			int cpu = psci_mpidr_to_cpu(read_mpidr());
			if (cpu < 0)
				return PSCI_RET_INVALID_PARAMETERS;
			if (cpu == 0)
				return PSCI_RET_DENIED;
			psci_cpu_off_local();
			return PSCI_RET_INTERNAL_FAILURE;
		}

		case PSCI_0_2_FN_AFFINITY_INFO:
			return psci_affinity_info_impl(arg0, arg1);

		case PSCI_0_2_FN_SYSTEM_OFF:
			psci_system_off();
			return PSCI_RET_INTERNAL_FAILURE;

		case PSCI_0_2_FN_SYSTEM_RESET:
			psci_system_reset();
			return PSCI_RET_INTERNAL_FAILURE;

		case PSCI_0_2_FN_CPU_SUSPEND:
		case PSCI_0_2_FN_MIGRATE_INFO_TYPE:
		case PSCI_0_2_FN64_CPU_SUSPEND:
		case PSCI_0_2_FN64_CPU_ON:
		case PSCI_0_2_FN64_AFFINITY_INFO:
		default:
			return PSCI_RET_NOT_SUPPORTED;
	}
}
