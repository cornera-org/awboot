#ifndef PSCI_SELFTEST_H
#define PSCI_SELFTEST_H

#include <stdint.h>
#include <stddef.h>

#ifndef PSCI_CPU_SELFTEST
#define PSCI_CPU_SELFTEST 0
#endif

#if PSCI_CPU_SELFTEST
void psci_selftest_prepare_environment(void);
uint32_t psci_selftest_entry_phys(void);
void psci_selftest_validate_environment(void);
void psci_clean_dcache_range(uintptr_t addr, size_t size);
void psci_invalidate_icache_range(uintptr_t addr, size_t size);
#else
static inline void psci_selftest_prepare_environment(void) {}
static inline uint32_t psci_selftest_entry_phys(void) { return 0u; }
static inline void psci_selftest_validate_environment(void) {}
#endif

#endif
