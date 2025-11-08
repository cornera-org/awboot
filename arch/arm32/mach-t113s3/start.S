#include <arch/arm32/include/linkage.h>

.syntax unified
.arm

#define CONFIG_NBOOT_STACK	0x38000
#define  ARMV7_USR_MODE        0x10
#define  ARMV7_FIQ_MODE        0x11
#define  ARMV7_IRQ_MODE        0x12
#define  ARMV7_SVC_MODE        0x13
#define  ARMV7_MON_MODE        0x16
#define  ARMV7_HYP_MODE        0x1a
#define  ARMV7_ABT_MODE        0x17
#define  ARMV7_UND_MODE        0x1b
#define  ARMV7_SYSTEM_MODE     0x1f
#define  ARMV7_MODE_MASK       0x1f
#define  ARMV7_FIQ_MASK        0x40
#define  ARMV7_IRQ_MASK        0x80
#define  ARMV7_ABT_MASK        0x100

#define PSCI_MON_REASON_UNDEF   0xE0
#define PSCI_MON_REASON_PREFETCH 0xE1
#define PSCI_MON_REASON_DATA    0xE2
#define PSCI_MON_REASON_IRQ     0xE3
#define PSCI_MON_REASON_FIQ     0xE4
#define PSCI_MON_REASON_RESET   0xE5

#ifndef MACH_T113S3_HAS_VIRTUAL_COUNTER
#define MACH_T113S3_HAS_VIRTUAL_COUNTER 0
#endif

/* Reserve stack slices for each privileged mode */
.set IRQ_STACK_SZ,  0x400
.set ABT_STACK_SZ,  0x400
.set UND_STACK_SZ,  0x400
.set FIQ_STACK_SZ,  0x100
.set MON_STACK_SZ,  0x200
.extern arm32_fault_stub
.extern psci_handle_smc
.extern psci_ns_stage_phys_value

.globl reset
reset:
	/* Boot head information for BROM */
	.long 0xea000016
	.byte 'e', 'G', 'O', 'N', '.', 'B', 'T', '0'
	.long 0x12345678                                /* checksum */
	.long __spl_size                                /* spl size */
	.long 0x30                                              /* boot header size */
	.long 0x30303033                                /* boot header version */
	.long 0x00020000                                /* return value */
	.long 0x00028000                                /* run address */
	.long 0x0                                               /* eGON version */
	.byte 0x00, 0x00, 0x00, 0x00    /* platform information - 8byte */
	.byte 0x34, 0x2e, 0x30, 0x00

/*
 * The actual reset code
 */
	mrs r0, cpsr
	bic r0, r0, #ARMV7_MODE_MASK
	orr r0, r0, #ARMV7_SVC_MODE
	orr r0, r0, #(ARMV7_IRQ_MASK | ARMV7_FIQ_MASK)
	bic r0, r0, #(1<<9)     @set little-endian
	msr cpsr_c, r0

/* Set vector base address register */

	ldr r0, =vector_template_start
	mcr p15, 0, r0, c12, c0, 0
	mrc p15, 0, r0, c1, c0, 0
	bic r0, #(1 << 13)
	mcr p15, 0, r0, c1, c0, 0

	mrc     p15, 0, r0, c1, c0, 0
	bic     r0, r0, #0x00002000     @ clear bits 13 (--V-)
	bic     r0, r0, #0x00000007     @ clear bits 2:0 (-CAM)
	orr     r0, r0, #0x00000800     @ set bit 11 (Z---) BTB
	bic     r0, r0, #0x00001000     @ clear bit 12 (I) I-cache
	mcr     p15, 0, r0, c1, c0, 0
	dsb sy
	isb

	/* Ensure non-secure world sees VFP if we drop out of secure */
	mrc     p15, 0, r1, c1, c1, 2
	orr     r1, r1, #(3 << 10)
	mcr     p15, 0, r1, c1, c1, 2
	dsb sy
	isb

	/* Enable neon/vfp unit */
	mrc p15, 0, r0, c1, c0, 2
	orr r0, r0, #(0xf << 20)
	mcr p15, 0, r0, c1, c0, 2
	isb
	mov r0, #0x40000000
	vmsr fpexc, r0

	/* Set stack pointer */
	ldr sp, =__stack_srv_end
	bl init_banked_stacks

	bl  clear_bss

	/* Build runtime vector table in SRAM */
	ldr r0, =vector_template_start
	ldr r1, =vector_template_end
	ldr r2, =__vector_table
	mov r3, r2

copy_vectors_loop:
	cmp r0, r1
	bhs copy_vectors_done
	ldr r4, [r0], #4
	str r4, [r3], #4
	b copy_vectors_loop

copy_vectors_done:
	bl arm32_invalidate_icache_btb
	ldr r0, =__vector_table
	mcr p15, 0, r0, c12, c0, 0
	dsb sy
	isb
	/*
		* disable interrupts (FIQ and IRQ), also set the cpu to SVC32 mode,
		* except if in HYP mode already
		*/
	mrs     r0, cpsr
	and     r1, r0, #0x1f           @ mask mode bits
	teq     r1, #0x1a               @ test for HYP mode
	bicne   r0, r0, #0x1f           @ clear all mode bits
	orrne   r0, r0, #0x13           @ set SVC mode
	orr     r0, r0, #0xc0           @ disable FIQ and IRQ
	msr     cpsr,r0

	@set cntfrq to 24M
	ldr r0, =24000000
	mcr p15, 0, r0, c14, c0, 0
	dsb sy
	isb

	bl main

	clear_bss:
	ldr     r0, =_sbss
	ldr     r1, =_ebss
	mov     r2, #0

	clbss_1:
	stmia   r0!, {r2}
	cmp r0, r1
	blt clbss_1

	mov pc, lr

.align 5
.arm
.globl init_banked_stacks
init_banked_stacks:
	ldr   r0, =__stack_srv_end
	sub   r0, r0, #MON_STACK_SZ
	cps   #0x16
	mov   sp, r0
	sub   r0, r0, #IRQ_STACK_SZ
	cps   #0x12
	mov   sp, r0
	sub   r0, r0, #ABT_STACK_SZ
	cps   #0x17
	mov   sp, r0
	sub   r0, r0, #UND_STACK_SZ
	cps   #0x1b
	mov   sp, r0
	sub   r0, r0, #FIQ_STACK_SZ
	cps   #0x13
	bx    lr

vector_template_start:
	b reset
	ldr pc, vector_slot_undefined
	ldr pc, vector_slot_software
	ldr pc, vector_slot_prefetch
	ldr pc, vector_slot_data
	ldr pc, vector_slot_unused
	ldr pc, vector_slot_irq
	ldr pc, vector_slot_fiq

vector_slot_undefined:
	.word arm_vector_undefined_trampoline
vector_slot_software:
	.word arm_vector_software_trampoline
vector_slot_prefetch:
	.word arm_vector_prefetch_trampoline
vector_slot_data:
	.word arm_vector_data_trampoline
vector_slot_unused:
	.word arm_vector_unused_trampoline
vector_slot_irq:
	.word arm_vector_irq_trampoline
vector_slot_fiq:
	.word arm_vector_fiq_trampoline

vector_template_end:

	.align 5
	.globl arm32_invalidate_icache_btb
arm32_invalidate_icache_btb:
	push {r0}
	mov r0, #0
	mcr p15, 0, r0, c7, c5, 0	@ invalidate entire I-cache
	mcr p15, 0, r0, c7, c5, 6	@ invalidate entire branch predictor array
	dsb sy
	isb
	pop {r0}
	bx lr

.section .bss, "aw", %nobits
	.align 5
	.globl __vector_table
__vector_table:
	.space 32		@ mirror the 8-entry vector template above
__vector_table_end:

	.align 5
	.globl psci_monitor_diag
psci_monitor_diag:
	.space 0x40		@ reason, spsr, lr, fsr/far snapshots (+hyp info)
psci_monitor_diag_end:

.section .rodata
	.align 2
fault_str_undef:
	.asciz "UNDEF"
fault_str_prefetch:
	.asciz "PREFETCH"
fault_str_data:
	.asciz "DATA"

.section .text

.macro save_regs
	str lr, [sp, #-4]
	mrs lr, spsr_all
	str lr, [sp, #-8]
	str r1, [sp, #-12]
	str r0, [sp, #-16]
	mov r0, sp
	cps #0x13
	ldr r1, [r0, #-4]
	str r1, [sp, #-4]!
	ldr r1, [r0, #-8]
	str r1, [sp, #-(4 * 16)]
	ldr r1, [r0, #-12]
	ldr r0, [r0, #-16]
	stmdb sp, {r0 - r14}^
	sub sp, sp, #(4 * 16)
	ldr r4, [sp]
	and r0, r4, #0x1f
	cmp r0, #0x10
	beq 10f
	cmp r0, #0x13
	beq 11f
	b .
11:	add r1, sp, #(4 * 17)
	str r1, [sp, #(4 * 14)]
	str lr, [sp, #(4 * 15)]
10:	add r1, sp, #(4 * 17)
	str r1, [sp, #-4]!
	mov r0, sp
.endm

.macro restore_regs
	mov r12, sp
	ldr sp, [r12], #4
	ldr r1, [r12], #4
	msr spsr_cxsf, r1
	and r0, r1, #0x1f
	cmp r0, #0x10
	beq 20f
	cmp r0, #0x13
	beq 21f
	b .
20:	ldr lr, [r12, #(4 * 15)]
	ldmia r12, {r0 - r14}^
	movs pc, lr
21:	ldm r12, {r0 - r15}^
	mov r0, r0
.endm

/*
 * Exception handlers
 */
	.align 5
arm_vector_undefined_trampoline:
	sub lr, lr, #4
	save_regs
	mov r2, r0
	ldr r0, =fault_str_undef
	mov r1, r2
	bl arm32_fault_stub
	mov r0, r2
	bl arm32_do_undefined_instruction
	restore_regs

	.align 5
arm_vector_software_trampoline:
	sub lr, lr, #4
	save_regs
	bl arm32_do_software_interrupt
	restore_regs

	.align 5
arm_vector_prefetch_trampoline:
	sub lr, lr, #4
	save_regs
	mov r2, r0
	ldr r0, =fault_str_prefetch
	mov r1, r2
	bl arm32_fault_stub
	mov r0, r2
	bl arm32_do_prefetch_abort
	restore_regs

	.align 5
arm_vector_data_trampoline:
	sub lr, lr, #8
	save_regs
	mov r2, r0
	ldr r0, =fault_str_data
	mov r1, r2
	bl arm32_fault_stub
	mov r0, r2
	bl arm32_do_data_abort
	restore_regs

	.align 5
arm_vector_unused_trampoline:
	b .

	.align 5
arm_vector_irq_trampoline:
	sub lr, lr, #4
	save_regs
	bl arm32_do_irq
	restore_regs

	.align 5
arm_vector_fiq_trampoline:
	sub lr, lr, #4
	save_regs
	bl arm32_do_fiq
	restore_regs

	.align 5
.globl psci_smc_entry
psci_smc_entry:
	stmdb sp!, {r4-r7, lr}
	bl psci_handle_smc
	ldmia sp!, {r4-r7, lr}
	movs pc, lr

.align 3
	.thumb
	.thumb_func
.globl arm32_shutdown_caches_pre_ns
arm32_shutdown_caches_pre_ns:
	bx pc
	nop
	.align 2
	.arm
arm32_shutdown_caches_pre_ns_arm:
	push {r4, lr}
	mrc p15, 0, r3, c1, c0, 0
	bic r3, r3, #(1 << 0)    @ disable MMU
	bic r3, r3, #(1 << 2)    @ disable dcache
	bic r3, r3, #(1 << 12)   @ disable icache
	mcr p15, 0, r3, c1, c0, 0
	dsb sy
	isb
	pop {r4, lr}
	bx lr

	.align 3
	.thumb
	.thumb_func
.globl arm32_enter_nonsecure
arm32_enter_nonsecure:
	bx pc
	nop
	.align 2
	.arm
arm32_enter_nonsecure_arm:
	ldr r6, =psci_ns_stage_phys_value
	ldr r6, [r6]
	mov r7, #0x30
	str r7, [r6]
	mov r12, r0			@ preserve entry address
	mov r0, r1			@ arg0
	mov r1, r2			@ arg1
	mov r2, r3			@ arg2
	cpsid aif
	dsb sy
	isb
	mov r7, #0x33
	str r7, [r6]
	mrc p15, 0, r3, c1, c1, 0
	bic r3, r3, #0x4a
	orr r3, r3, #(0x31)
	mrc p15, 0, r8, c0, c1, 1	@ ID_PFR1
	mov r11, r8, lsr #4
	and r11, r11, #0xf		@ virtualization support present?
	@ Force hand-off in SVC: keep SCR.HCE cleared regardless of ID_PFR1.
	mcr p15, 0, r3, c1, c1, 0
	mov r7, #0x34
	str r7, [r6]
	@
	@ Reset CNTVOFF when virtualization and the generic timer are present
	@ so the first non-secure instruction does not take a synchronous abort.
	.ifne MACH_T113S3_HAS_VIRTUAL_COUNTER
	mrc p15, 0, r9, c0, c1, 1
	mov r10, r9, lsr #12
	and r10, r10, #0xf
	beq 1f
	mov r10, r9, lsr #16
	and r10, r10, #0xf
	beq 1f
	mov r9, #0
	mov r10, #0
	mcrr p15, 4, r9, r10, c14
1:
	.endif
	isb
	@ copy secure SVC stack to non-secure SVC stack
	mov r5, sp
	mcr p15, 0, r5, c13, c0, 2
	mov r4, r12
	mov r3, #0
	mov r5, #(ARMV7_SVC_MODE | ARMV7_IRQ_MASK | ARMV7_FIQ_MASK)
	tst r4, #1
	orrne r4, r4, #1
	mov r7, #0x31
	str r7, [r6]
	ldr r10, =psci_monitor_diag
	str r5, [r10, #40]		@ record desired SPSR
	str r11, [r10, #44]		@ record virtualization nibble
	mov r7, #0x32
	str r7, [r6]
	push {r0, r1, r2}
	mov r0, r4
	pop {r1, r2, r3}
	bl psci_do_nonsec_entry
	b .
