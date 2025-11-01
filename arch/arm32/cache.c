#include <stddef.h>
#include <stdint.h>

#include "arm32.h"

void arm32_dcache_clean_invalidate_range(uintptr_t start, size_t size)
{
	if (size == 0U)
		return;

	uint32_t csselr = 0U;
	uint32_t ccsidr;
	__asm__ volatile("mcr p15, 2, %0, c0, c0, 0\n"
			      "isb\n"
			      :
			      : "r"(csselr)
			      : "memory");
	__asm__ volatile("mrc p15, 1, %0, c0, c0, 0" : "=r"(ccsidr));

	uint32_t line_shift = (ccsidr & 0x7U) + 4U;
	uintptr_t line_bytes = (uintptr_t)1U << line_shift;
	uintptr_t line_mask  = line_bytes - 1U;

	uintptr_t begin = start & ~line_mask;
	uintptr_t end   = (start + (uintptr_t)size + line_mask) & ~line_mask;

	for (uintptr_t addr = begin; addr < end; addr += line_bytes) {
		__asm__ volatile("mcr p15, 0, %0, c7, c14, 1" : : "r"(addr) : "memory");
	}

	__asm__ volatile("dsb sy\n"
			      "isb\n"
			      :
			      :
			      : "memory");
}
