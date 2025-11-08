#ifndef __ARM32_H__
#define __ARM32_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

static inline uint32_t arm32_read_mpidr(void)
{
	uint32_t mpidr;
	__asm__ __volatile__("mrc p15, 0, %0, c0, c0, 5" : "=r"(mpidr));
	return mpidr;
}

static inline void arm32_dcache_clean_line_by_mva(uintptr_t addr)
{
	__asm__ __volatile__("mcr p15, 0, %0, c7, c10, 1" :: "r"(addr) : "memory");
}

static inline void arm32_dcache_invalidate_line_by_mva(uintptr_t addr)
{
	__asm__ __volatile__("mcr p15, 0, %0, c7, c6, 1" :: "r"(addr) : "memory");
}

static inline void arm32_dcache_clean_invalidate_line_by_mva(uintptr_t addr)
{
	__asm__ __volatile__("mcr p15, 0, %0, c7, c14, 1" :: "r"(addr) : "memory");
}

static inline void arm32_icache_invalidate_line_by_mva(uintptr_t addr)
{
	__asm__ __volatile__("mcr p15, 0, %0, c7, c5, 1" :: "r"(addr) : "memory");
}

static inline void arm32_icache_invalidate_all(void)
{
	__asm__ __volatile__("mcr p15, 0, %0, c7, c5, 0" :: "r"(0u) : "memory");
}

static inline void arm32_dcache_clean_invalidate_all(void)
{
	__asm__ __volatile__("mcr p15, 0, %0, c7, c14, 0" :: "r"(0u) : "memory");
	__asm__ __volatile__("dsb sy" ::: "memory");
	__asm__ __volatile__("isb" ::: "memory");
}

static inline void arm32_branch_predictor_invalidate_all(void)
{
	__asm__ __volatile__("mcr p15, 0, %0, c7, c5, 6" :: "r"(0u) : "memory");
}

static inline void arm32_write_mvbar(uint32_t value)
{
	__asm__ __volatile__("mcr p15, 0, %0, c12, c0, 1" :: "r"(value) : "memory");
}

static inline uint32_t arm32_read_mvbar(void)
{
	uint32_t value;
	__asm__ __volatile__("mrc p15, 0, %0, c12, c0, 1" : "=r"(value));
	return value;
}

static inline uint32_t arm32_read_p15_c1(void)
{
	uint32_t value;

	__asm__ __volatile__("mrc p15, 0, %0, c1, c0, 0" : "=r"(value) : : "memory");

	return value;
}

static inline void arm32_write_p15_c1(uint32_t value)
{
	__asm__ __volatile__("mcr p15, 0, %0, c1, c0, 0" : : "r"(value) : "memory");
	arm32_read_p15_c1();
}

static inline void arm32_interrupt_enable(void)
{
	uint32_t tmp;

	__asm__ __volatile__("mrs %0, cpsr\n"
						 "bic %0, %0, #(1<<7)\n"
						 "msr cpsr_cxsf, %0"
						 : "=r"(tmp)
						 :
						 : "memory");
}

static inline void arm32_interrupt_disable(void)
{
	uint32_t tmp;

	__asm__ __volatile__("mrs %0, cpsr\n"
						 "orr %0, %0, #(1<<7)\n"
						 "msr cpsr_cxsf, %0"
						 : "=r"(tmp)
						 :
						 : "memory");
}

static inline void arm32_mmu_enable(void)
{
	uint32_t value = arm32_read_p15_c1();
	arm32_write_p15_c1(value | (1 << 0));
}

static inline void arm32_mmu_disable(void)
{
	uint32_t value = arm32_read_p15_c1();
	arm32_write_p15_c1(value & ~(1 << 0));
}

static inline void arm32_dcache_enable(void)
{
	uint32_t value = arm32_read_p15_c1();
	arm32_write_p15_c1(value | (1 << 2));
}

static inline void arm32_dcache_disable(void)
{
	uint32_t value = arm32_read_p15_c1();
	arm32_write_p15_c1(value & ~(1 << 2));
}

static inline void arm32_icache_enable(void)
{
	uint32_t value = arm32_read_p15_c1();
	arm32_write_p15_c1(value | (1 << 12));
}

static inline void arm32_icache_disable(void)
{
	uint32_t value = arm32_read_p15_c1();
	arm32_write_p15_c1(value & ~(1 << 12));
}

void arm32_invalidate_icache_btb(void);
void arm32_enter_nonsecure(void (*entry)(int, int, unsigned int),
                           unsigned int arg0, unsigned int arg1, unsigned int arg2);
void arm32_shutdown_caches_pre_ns(void);

static inline void arm32_enable_smp(void)
{
	uint32_t value;
	__asm__ __volatile__("mrc p15, 0, %0, c1, c0, 1" : "=r"(value));
	value |= (1u << 6);
	__asm__ __volatile__("mcr p15, 0, %0, c1, c0, 1" :: "r"(value) : "memory");
	__asm__ __volatile__("isb" ::: "memory");
}

static inline uint32_t arm32_cpsr_read(void)
{
	uint32_t value;
	__asm__ __volatile__("mrs %0, cpsr" : "=r"(value));
	return value;
}

static inline uint32_t arm32_read_scr(void)
{
	uint32_t value;
	__asm__ __volatile__("mrc p15, 0, %0, c1, c1, 0" : "=r"(value));
	return value;
}

static inline uint32_t arm32_read_id_pfr1(void)
{
	uint32_t value;
	__asm__ __volatile__("mrc p15, 0, %0, c0, c1, 1" : "=r"(value));
	return value;
}

static inline uint32_t arm32_read_hcr(void)
{
	uint32_t value;
	__asm__ __volatile__("mrc p15, 4, %0, c1, c1, 0" : "=r"(value));
	return value;
}

static inline uint32_t arm32_read_hcptr(void)
{
	uint32_t value;
	__asm__ __volatile__("mrc p15, 4, %0, c1, c1, 2" : "=r"(value));
	return value;
}

static inline uint32_t arm32_read_hdcr(void)
{
	uint32_t value;
	__asm__ __volatile__("mrc p15, 4, %0, c1, c1, 1" : "=r"(value));
	return value;
}

static inline uint32_t arm32_read_hvbar(void)
{
	uint32_t value;
	__asm__ __volatile__("mrc p15, 4, %0, c12, c0, 0" : "=r"(value));
	return value;
}

static inline void arm32_write_hvbar(uint32_t value)
{
	__asm__ __volatile__("mcr p15, 4, %0, c12, c0, 0" :: "r"(value) : "memory");
}

static inline void arm32_write_hcr(uint32_t value)
{
	__asm__ __volatile__("mcr p15, 4, %0, c1, c1, 0" :: "r"(value) : "memory");
}

static inline void arm32_write_hcptr(uint32_t value)
{
	__asm__ __volatile__("mcr p15, 4, %0, c1, c1, 2" :: "r"(value) : "memory");
}

static inline void arm32_write_hstr(uint32_t value)
{
	__asm__ __volatile__("mcr p15, 4, %0, c1, c1, 3" :: "r"(value) : "memory");
}

static inline void arm32_write_hdcr(uint32_t value)
{
	__asm__ __volatile__("mcr p15, 4, %0, c1, c1, 1" :: "r"(value) : "memory");
}

static inline uint32_t arm32_read_cnthctl(void)
{
	uint32_t value;
	__asm__ __volatile__("mrc p15, 4, %0, c14, c1, 0" : "=r"(value));
	return value;
}

static inline void arm32_write_cnthctl(uint32_t value)
{
	__asm__ __volatile__("mcr p15, 4, %0, c14, c1, 0" :: "r"(value) : "memory");
}

static inline void arm32_write_cntvoff(uint64_t value)
{
	uint32_t lo = (uint32_t)(value & 0xffffffffu);
	uint32_t hi = (uint32_t)(value >> 32);
	__asm__ __volatile__("mcrr p15, 4, %0, %1, c14" :: "r"(lo), "r"(hi) : "memory");
}

#ifdef __cplusplus
}
#endif

#endif /* __ARM32_H__ */
