#ifndef __AWBOOT_ASM_ARMV7_H__
#define __AWBOOT_ASM_ARMV7_H__

#define MIDR_CORTEX_A7_R0P0   0x410FC070
#define MIDR_CORTEX_A9_R0P1   0x410FC091
#define MIDR_CORTEX_A9_R1P2   0x411FC092
#define MIDR_CORTEX_A9_R1P3   0x411FC093
#define MIDR_CORTEX_A9_R2P10  0x412FC09A
#define MIDR_CORTEX_A15_R0P0  0x410FC0F0
#define MIDR_CORTEX_A15_R2P2  0x412FC0F2

#define MIDR_PRIMARY_PART_MASK 0xFF0FFFF0

#define CPUID_ARM_SEC_SHIFT        4
#define CPUID_ARM_SEC_MASK         (0xF << CPUID_ARM_SEC_SHIFT)
#define CPUID_ARM_VIRT_SHIFT       12
#define CPUID_ARM_VIRT_MASK        (0xF << CPUID_ARM_VIRT_SHIFT)
#define CPUID_ARM_GENTIMER_SHIFT   16
#define CPUID_ARM_GENTIMER_MASK    (0xF << CPUID_ARM_GENTIMER_SHIFT)

#define CBAR_MASK                  0xFFFF8000

#if !defined(__ASSEMBLY__) && !defined(__ASSEMBLER__)
#include <arm32.h>
#include <barrier.h>

int armv7_init_nonsec(void);
void _do_nonsec_entry(void *target_pc, unsigned long r0,
		      unsigned long r1, unsigned long r2);
unsigned int _nonsec_init(void);
void _smp_pen(void);
extern char __secure_start[];
extern char __secure_end[];
extern char __secure_stack_start[];
extern char __secure_stack_end[];

#endif

#endif
