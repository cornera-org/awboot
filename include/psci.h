#ifndef __PSCI_H__
#define __PSCI_H__

#include <stdint.h>
#include <stdbool.h>

#define PSCI_VERSION(major, minor) (((major) << 16) | ((minor) & 0xffff))

/* PSCI return codes */
#define PSCI_RET_SUCCESS              0
#define PSCI_RET_NOT_SUPPORTED       -1
#define PSCI_RET_INVALID_PARAMETERS  -2
#define PSCI_RET_DENIED              -3
#define PSCI_RET_ALREADY_ON          -4
#define PSCI_RET_ON_PENDING          -5
#define PSCI_RET_INTERNAL_FAILURE    -6
#define PSCI_RET_NOT_PRESENT         -7
#define PSCI_RET_DISABLED            -8
#define PSCI_RET_INVALID_ADDRESS     -9

/* CPU state tracking */
#define PSCI_CPU_STATE_OFF        0U
#define PSCI_CPU_STATE_ON_PENDING 1U
#define PSCI_CPU_STATE_ON         2U

#define PSCI_AFFINITY_INFO_ON          0U
#define PSCI_AFFINITY_INFO_OFF         1U
#define PSCI_AFFINITY_INFO_ON_PENDING  2U

#define PSCI_MAX_CPUS 2U

#define PSCI_TEXT_SECTION __attribute__((section(".psci_text")))
#define PSCI_DATA_SECTION __attribute__((section(".psci_data")))
#define PSCI_ALIGNED(x)   __attribute__((aligned(x)))

#ifdef __ASSEMBLER__
#define PSCI_CPU_CTX_ENTRY_OFFSET    0
#define PSCI_CPU_CTX_CONTEXT_OFFSET  4
#define PSCI_CPU_CTX_STATE_OFFSET    8
#define PSCI_CPU_CTX_RESERVED_OFFSET 12
#define PSCI_CPU_CTX_STRIDE          16
#else
struct psci_cpu_ctx {
	volatile uint32_t entry;
	volatile uint32_t context;
	volatile uint32_t state;
	volatile uint32_t reserved;
};

extern struct psci_cpu_ctx psci_cpu_data[PSCI_MAX_CPUS];

extern volatile uint32_t psci_ns_entered;

void psci_init(void) PSCI_TEXT_SECTION;
uint32_t psci_smc_handler(uint32_t fid, uint32_t arg0, uint32_t arg1, uint32_t arg2) PSCI_TEXT_SECTION;
void psci_enter_non_secure(uint32_t entry, uint32_t arg0, uint32_t arg1, uint32_t arg2) PSCI_TEXT_SECTION;

#endif /* __ASSEMBLER__ */

#endif /* __PSCI_H__ */
