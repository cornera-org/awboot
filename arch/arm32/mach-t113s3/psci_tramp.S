.syntax unified
.arch armv7-a
.arch_extension sec

.section .text.psci_secure_entry, "ax", %progbits
.align 2
.global psci_cpu_secure_entry
.extern psci_cpu_entry_phys
.extern psci_cpu_last_context
.extern psci_cpu_tramp_trace
.extern arm32_enter_nonsecure
.extern psci_ns_stage_phys_value
.extern psci_selftest_context_sec_value
.extern psci_monitor_vector_table
.extern __stack_srv_end

#define MON_STACK_SZ 0x200

psci_cpu_secure_entry:
	mrc	p15, 0, r0, c0, c0, 5		@ r0 = MPIDR
	ands	r0, r0, #0xff		@ isolate core ID
	mov	r8, r0			@ preserve core ID
	ldr	r7, =psci_monitor_vector_table
	mcr	p15, 0, r7, c12, c0, 1	@ program MVBAR for this core
	dsb	sy
	isb
	ldr	r5, =__stack_srv_end
	mov	sp, r5			@ ensure SVC stack is valid
	mov	r6, #MON_STACK_SZ
	mul	r6, r6, r8
	add	r6, r6, #MON_STACK_SZ
	sub	r5, r5, r6		@ unique monitor stack per core
	mrs	r9, cpsr
	cps	#0x16			@ switch to Monitor mode
	mov	sp, r5
	msr	cpsr_c, r9		@ restore previous mode
	mov	r0, r8
	ldr	r1, =psci_cpu_entry_phys
	ldr	r2, [r1, r0, lsl #2]		@ r2 = target entry
	ldr	r1, =psci_cpu_last_context
	ldr	r3, [r1, r0, lsl #2]		@ r3 = saved context
	ldr	r1, =psci_cpu_tramp_trace
	str	r2, [r1]
	str	r3, [r1, #4]
	mov	r4, #0x20
	str	r4, [r1, #8]		@ secure stage marker
	ldr	r6, =psci_ns_stage_phys_value
	ldr	r6, [r6]
	str	r4, [r6]		@ bump shared stage
	ldr	r5, =psci_selftest_context_sec_value
	ldr	r5, [r5]
	cmp	r3, r5			@ secure self-test?
	beq	1f
	cmp	r2, #0		@ no entry? bail
	beq	2f
	mov	r7, #0x21
	str	r7, [r6]
	str	r7, [r1, #8]
	mov	r0, r2
	mov	r1, #0
	mov	r2, #0
	ldr	r4, =arm32_enter_nonsecure
	blx	r4				@ jump to non-secure world
	b	2f
1:
	mov	r0, r2			@ secure path: branch directly
	bx	r0
2:
	b	2b				@ should never return

.section .rodata.psci_stub, "a", %progbits
.align 2
.global psci_ns_stub_start
.global psci_ns_stub_end
.global psci_ns_stub_diag_lit
.global psci_ns_stub_stage_lit
.global psci_ns_stub_sram_lit
.global psci_ns_stub_trace_lit

psci_ns_stub_start:
.thumb
.thumb_func
	ldr	r4, psci_ns_stub_diag_lit	@ r4 = diag buffer
	ldr	r6, psci_ns_stub_stage_lit	@ r6 = stage pointer
	ldr	r0, psci_ns_stub_trace_lit	@ r0 = trace slot
	movs	r7, #0x39
	str	r7, [r0]
	movs	r7, #0x40
	str	r7, [r6]
	str	r7, [r0]
	movs	r5, #0
1:
	adds	r5, #1
	cmp	r5, #0x100			@ small busy loop
	bcc	1b
	mrc	p15, 0, r1, c0, c0, 5
	mrs	r2, cpsr
	str	r5, [r4]
	str	r1, [r4, #4]
	str	r2, [r4, #8]
	movs	r7, #0x41
	str	r7, [r6]		@ stage = 3
	str	r7, [r0]
	ldr	r3, psci_ns_stub_sram_lit
	str	r5, [r3]			@ touch SRAM scratch
	dsb	sy
	movs	r7, #0x42
	str	r7, [r6]
	str	r7, [r0]
	movw	r0, #0x0100
	movt	r0, #0x8200
	mov	r3, r5
	smc	#0				@ report via SMC
	b	.

.align 2
psci_ns_stub_diag_lit:
	.word	0
psci_ns_stub_stage_lit:
	.word	0
psci_ns_stub_sram_lit:
	.word	0
psci_ns_stub_trace_lit:
	.word	0
psci_ns_stub_end:
.previous
