.syntax unified
.arch armv7-a
.arch_extension sec
.arch_extension virt
.arm

#include <arch/arm32/include/linkage.h>

	.extern	psci_smc_handler
	.extern	psci_cpu_data
	.extern	__psci_stack_base
	.extern	__psci_stack_top
	.extern	psci_has_virt
	.extern	__psci_mon_stack_base
	.extern	__psci_mon_stack_top
	.extern	__vector_table
	.extern	psci_debug_hyp_trap
	.extern	psci_debug_enter_ns
	.extern	psci_debug_mon_call
	.extern	psci_debug_mon_prepare
	.extern	sunxi_gic_disable
	.extern	psci_debug_pre_smc
	.extern	psci_enter_return_fail

.equ	ARMV7_SVC_MODE,      0x13
.equ	ARMV7_MON_MODE,      0x16
.equ	ARMV7_IRQ_MASK,      0x80
.equ	ARMV7_FIQ_MASK,      0x40
.equ	ARMV7_T_BIT,         0x20

	.equ	PSCI_MAX_CPUS,       2
	.equ	PSCI_CPU_CTX_STRIDE, 16
	.equ	PSCI_CPU_CTX_ENTRY,  0
	.equ	PSCI_CPU_CTX_CONTEXT,4
	.equ	PSCI_CPU_CTX_STATE,  8
	.equ	PSCI_CPU_STATE_OFF,  0
	.equ	PSCI_CPU_STATE_ON,   2

	.equ	HSR_EC_SHIFT,        26
	.equ	HSR_EC_HVC,          0x12
	.equ	HSR_EC_SMC,          0x17
	.equ	HCR_TSC_BIT,        (1 << 19)
.equ	CNTHCTL_EL1PCTEN,   (1 << 0)
.equ	CNTHCTL_EL1PCEN,    (1 << 1)
.equ	CNTHCTL_PL1_ENABLE, (CNTHCTL_EL1PCTEN | CNTHCTL_EL1PCEN)
.equ	ARMV7_HYP_MODE,      0x1a
.equ	ARMV7_ABT_MASK,      0x100
.equ	PSCI_SMC_ENTER_NS,  0x534e534d

/*
 * Monitor vector table to handle SMC calls.
 */
.section .psci_mon, "ax", %progbits
 	.align	5
psci_mon_vectors:
	b	psci_mon_reset
	b	psci_mon_undef
	b	psci_mon_smc
	b	psci_mon_abort
	b	psci_mon_abort
	b	psci_mon_unused
	b	psci_mon_irq
	b	psci_mon_fiq

psci_mon_reset:
psci_mon_undef:
psci_mon_abort:
psci_mon_unused:
psci_mon_irq:
psci_mon_fiq:
	b	psci_mon_loop

	.section .psci_data, "aw", %progbits
	.align	2
psci_ns_params:
	.word	0, 0, 0, 0

	.section .psci_mon, "ax", %progbits

psci_mon_loop:
	b	psci_mon_loop

psci_mon_smc:
	push	{r4-r7, lr}
	mov	r4, r0
	mov	r5, r1
	mov	r6, r2
	mov	r7, r3
	mov	r0, r4
	mov	r1, r5
	mov	r2, r6
	mov	r3, r7
	bl	psci_debug_mon_call
	mov	r0, r4
	mov	r1, r5
	mov	r2, r6
	mov	r3, r7
	ldr	r10, =PSCI_SMC_ENTER_NS
	cmp	r0, r10
	beq	1f
	bl	psci_smc_handler
	pop	{r4-r7, lr}
	movs	pc, lr
	pop	{r4-r7, lr}
	mrc	p15, 0, r0, c1, c1, 0
	orr	r0, r0, #1
	mcr	p15, 0, r0, c1, c1, 0
	isb
	b	psci_mon_enter_ns

psci_mon_enter_ns:
	ldr	r10, =psci_ns_params
	ldr	r4, [r10, #0]	@ entry
	ldr	r5, [r10, #4]	@ arg0
	ldr	r6, [r10, #8]	@ arg1
	ldr	r7, [r10, #12]	@ arg2
	ldr	r0, =psci_ns_entered
	mov	r1, #1
	str	r1, [r0]

	ldr	r0, =psci_mon_vectors
	mcr	p15, 0, r0, c12, c0, 1
	isb

	ldr	r0, =__psci_mon_stack_top
	mov	sp, r0

	mrc	p15, 0, r0, c1, c1, 0
	push	{r0-r3, lr}
	bl	sunxi_gic_disable
	pop	{r0-r3, lr}

	mrc	p15, 0, r5, c1, c1, 0
	bic	r5, r5, #0x4a			@ clear IRQ, EA, nET
	orr	r5, r5, #0x31			@ set NS, AW, FW

	mrc	p15, 0, r0, c1, c1, 2
	movw	r1, #0x3fff
	movt	r1, #0x0004
	orr	r0, r0, r1
	mcr	p15, 0, r0, c1, c1, 2
	isb

	ldr	r0, =__vector_table
	mcr	p15, 0, r0, c12, c0, 0
	isb
	dsb	sy

	add	r1, r4, #0x8000
	msr	sp_svc, r1

	ldr	r0, =psci_has_virt
	ldr	r0, [r0]
	mov	r9, #(ARMV7_SVC_MODE | ARMV7_IRQ_MASK | ARMV7_FIQ_MASK)
	cmp	r0, #0
	beq	1f
	orr	r5, r5, #0x100			@ enable HVC
	mrs	r3, sp_svc
	msr	sp_hyp, r3
	mov	r9, #ARMV7_HYP_MODE
1:
	mcr	p15, 0, r5, c1, c1, 0
	isb

	ldr	r0, =psci_has_virt
	ldr	r0, [r0]
	cmp	r0, #0
	beq	1f
	bl	psci_init_asm
1:
	mov	r0, r4
	mov	r1, r5
	mov	r2, r6
	mov	r3, r7
	bl	psci_debug_mon_prepare
	mov	r0, r5
	mov	r1, r6
	mov	r2, r7
	mov	r3, #0
	mov	lr, r4
	mov	r8, #(ARMV7_ABT_MASK | ARMV7_IRQ_MASK | ARMV7_FIQ_MASK)
	tst	lr, #1
	orrne	r8, r8, #ARMV7_T_BIT
	orr	r8, r8, r9
	msr	spsr_cxsf, r8
	movs	pc, lr

/*
 * Hypervisor vector table – keep in dedicated section for linker ordering.
 */
	.section .psci_vectors, "ax", %progbits
	.align	5
psci_hyp_vectors:
	b	psci_hyp_sync
	b	psci_hyp_irq
	b	psci_hyp_fiq
	b	psci_hyp_error
	b	psci_hyp_error
	b	psci_hyp_error
	b	psci_hyp_error
	b	psci_hyp_error

/*
 * Common helper to install HVBAR, SP_hyp and sane defaults.
 */
	.section .psci_text, "ax", %progbits
	.align	2
	.global	psci_init_asm
psci_init_asm:
	/* Determine CPU index from MPIDR */
	mrc	p15, 0, r0, c0, c0, 5
	bic	r0, r0, #0x80000000
	and	r0, r0, #0xff

	/* Compute target Hyp stack for this CPU */
	ldr	r1, =__psci_stack_top
	ldr	r2, =__psci_stack_base
	sub	r2, r1, r2
	lsr	r2, r2, #1			@ divide by max CPUs (2)
	mul	r2, r2, r0
	sub	r1, r1, r2
	bic	r1, r1, #7			@ 8-byte aligned SP

	/* Temporarily switch to Hyp mode to touch Hyp registers */
	mrs	r12, cpsr
	cps	#0x1a				@ enter Hyp mode
	mov	sp, r1

	ldr	r1, =psci_hyp_vectors
	mcr	p15, 4, r1, c12, c0, 0		@ HVBAR
	isb

	ldr	r1, =HCR_TSC_BIT
	mcr	p15, 4, r1, c1, c1, 0		@ HCR

	mov	r1, #CNTHCTL_PL1_ENABLE
	mcr	p15, 4, r1, c14, c1, 0		@ CNTHCTL

	mov	r0, #0
	mov	r2, #0
	mcrr	p15, 4, r0, r2, c14		@ CNTVOFF = 0

	mov	r1, #0
	mcr	p15, 4, r1, c1, c1, 2		@ HCPTR
	mcr	p15, 4, r1, c1, c1, 3		@ HSTR
	mcr	p15, 4, r1, c1, c0, 0		@ HSCTLR
	isb
	dsb	sy

	cps	#0x13				@ back to SVC
	msr	cpsr_cxsf, r12			@ restore saved CPSR

	bx	lr

/*
 * Final hand-off into the non-secure world.
 */
	.section .psci_text, "ax", %progbits
	.align	2
	.global	psci_enter_non_secure
psci_enter_non_secure:
	push	{r4-r9, lr}
	mov	r4, r0
	mov	r5, r1
	mov	r6, r2
	mov	r7, r3
	mov	r0, r4
	mov	r1, r5
	mov	r2, r6
	mov	r3, r7
	bl	psci_debug_enter_ns

	mrs	r8, cpsr
	cps	#ARMV7_MON_MODE
	ldr	sp, =__psci_mon_stack_top
	ldr	r9, =psci_mon_vectors
	mcr	p15, 0, r9, c12, c0, 1
	cps	#ARMV7_SVC_MODE
	msr	cpsr_cxsf, r8

	mrc	p15, 4, r10, c1, c1, 0
	mrc	p15, 0, r11, c1, c1, 0
	mov	r0, r11
	mov	r1, r10
	bl	psci_debug_pre_smc
	bic	r10, r10, #(1 << 19)
	mcr	p15, 4, r10, c1, c1, 0
	isb
	bic	r11, r11, #1
	mcr	p15, 0, r11, c1, c1, 0
	isb

	ldr	r12, =psci_ns_params
	str	r0, [r12, #0]		@ entry
	str	r1, [r12, #4]		@ arg0
	str	r2, [r12, #8]		@ arg1
	str	r3, [r12, #12]		@ arg2
	ldr	r0, =PSCI_SMC_ENTER_NS
	smc	#0
	bl	psci_enter_return_fail

psci_enter_ns_return:
	bx	lr

/*
 * Hypervisor synchronous exception handler.
 * Handle PSCI calls invoked via HVC or trapped SMC (if enabled).
 */
	.align	5
psci_hyp_sync:
	stmdb	sp!, {r4-r7, lr}
	mrc	p15, 4, r4, c5, c2, 0		@ HSR
	stmdb	sp!, {r0-r3}
	mov	r0, r4				@ arg0 = HSR
	ldr	r1, [sp, #0]			@ original r0
	ldr	r2, [sp, #4]			@ original r1
	ldr	r3, [sp, #8]			@ original r2
	ldr	r7, [sp, #12]			@ original r3
	stmdb	sp!, {r7}			@ 5th argument
	bl	psci_debug_hyp_trap
	ldmia	sp!, {r7}
	ldmia	sp!, {r0-r3}
	lsr	r5, r4, #HSR_EC_SHIFT
	cmp	r5, #HSR_EC_HVC
	beq	1f
	cmp	r5, #HSR_EC_SMC
	beq	1f
	/* Unknown trap: report not supported */
	mvn	r0, #0
	b	2f
1:	bl	psci_smc_handler
2:	ldmia	sp!, {r4-r7, lr}
	eret

psci_hyp_irq:
psci_hyp_fiq:
psci_hyp_error:
	/* Re-synchronize lower level for unexpected events */
	eret

/*
 * Secondary CPU trampoline – entered after sunxi_cpucfg released the core.
 */
	.section .psci_tramp, "ax", %progbits
	.align	5
	.global	psci_secondary_entry
psci_secondary_entry:
	/* Enter SVC mode with IRQ/FIQ masked */
	mrs	r4, cpsr
	bic	r4, r4, #0x1f
	orr	r4, r4, #0x13
	orr	r4, r4, #0xc0
	msr	cpsr_c, r4

	/* Prepare hypervisor state for this CPU if available */
	ldr	r0, =psci_has_virt
	ldr	r0, [r0]
	cmp	r0, #0
	beq	0f
	bl	psci_init_asm
0:

	/* Enable SMP bit in ACTLR */
	mrc	p15, 0, r0, c1, c0, 1
	orr	r0, r0, #(1 << 6)
	mcr	p15, 0, r0, c1, c0, 1
	dsb	sy
	isb

	/* Locate per-CPU PSCI context */
	mrc	p15, 0, r0, c0, c0, 5
	bic	r0, r0, #0x80000000
	and	r0, r0, #0xff
	ldr	r1, =psci_cpu_data
	add	r1, r1, r0, lsl #4		@ context stride = 16 bytes

	ldr	r4, [r1, #PSCI_CPU_CTX_ENTRY]
	ldr	r5, [r1, #PSCI_CPU_CTX_CONTEXT]
	cmp	r4, #0
	bne	1f

	mov	r0, #PSCI_CPU_STATE_OFF
	str	r0, [r1, #PSCI_CPU_CTX_STATE]
	mov	r0, #0
	str	r0, [r1, #PSCI_CPU_CTX_ENTRY]
	str	r0, [r1, #PSCI_CPU_CTX_CONTEXT]
	dmb	sy
0:	wfi
	b	0b

1:	mov	r0, #PSCI_CPU_STATE_ON
	str	r0, [r1, #PSCI_CPU_CTX_STATE]
	mov	r0, #0
	str	r0, [r1, #PSCI_CPU_CTX_ENTRY]
	str	r0, [r1, #PSCI_CPU_CTX_CONTEXT]
	dmb	sy
	isb

	/* PSCI CPU_ON entry protocol */
	mov	r0, #0
	mov	r1, r5
	mov	r2, #0
	bx	r4
