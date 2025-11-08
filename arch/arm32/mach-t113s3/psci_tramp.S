.syntax unified
.arch armv7-a
.arch_extension sec

.section .text.psci_secure_entry, "ax", %progbits
.align 2
.global psci_cpu_secure_entry
.extern psci_cpu_entry_phys
.extern arm32_enter_nonsecure
.extern psci_ns_stage_phys_value
.extern psci_ns_diag_phys_value
.extern psci_ns_linux_secondary_phys_value
.extern psci_monitor_vector_table
.extern __stack_srv_end

#define MON_STACK_SZ 0x200

psci_cpu_secure_entry:
	mrc	p15, 0, r0, c0, c0, 5		@ r0 = MPIDR
	ands	r0, r0, #0xff		@ isolate core ID
	mov	r8, r0		@ preserve core ID
	ldr	r7, =psci_monitor_vector_table
	mcr	p15, 0, r7, c12, c0, 1	@ program MVBAR for this core
	dsb	sy
	isb
	ldr	r5, =__stack_srv_end
	mov	sp, r5		@ ensure SVC stack is valid
	mov	r6, #MON_STACK_SZ
	mul	r6, r6, r8
	add	r6, r6, #MON_STACK_SZ
	sub	r5, r5, r6		@ unique monitor stack per core
	mrs	r9, cpsr
	cps	#0x16		@ switch to Monitor mode
	mov	sp, r5
	msr	cpsr_c, r9		@ restore previous mode
	mov	r0, r8
	ldr	r1, =psci_cpu_entry_phys
	ldr	r2, [r1, r0, lsl #2]		@ r2 = target entry
	mov	r4, #0x20
	ldr	r6, =psci_ns_stage_phys_value
	ldr	r6, [r6]
	str	r4, [r6]		@ bump shared stage
	cmp	r2, #0	@ no entry? bail
	beq	2f
	mov	r7, #0x21
	str	r7, [r6]
	mov	r0, r2
	mov	r1, #0
	mov	r2, #0
	ldr	r4, =arm32_enter_nonsecure
	blx	r4			@ jump to non-secure world
2:
	b	2b			@ should never return

.section .rodata.psci_stub, "a", %progbits
.align 2
.global psci_ns_stub_start
.global psci_ns_stub_end
.global psci_ns_stub_diag_lit
.global psci_ns_stub_stage_lit
.global psci_ns_stub_sram_lit
.global psci_ns_stub_trace_lit
.global psci_ns_stub_secdata_lit
.global psci_ns_stub_context_lit
.global psci_ns_stub_entry_lit

#define PSCI_SELFTEST_CONTEXT_NS 0xfffffffeu

psci_ns_stub_start:
	.arm
	ldr	r4, psci_ns_stub_diag_lit	@ diag buffer pointer
	ldr	r5, psci_ns_stub_stage_lit	@ stage pointer
	ldr	r6, psci_ns_stub_trace_lit	@ trace slot pointer
	ldr	r7, psci_ns_stub_entry_lit	@ entry literal
	ldr	r8, psci_ns_stub_secdata_lit	@ secondary_data pointer
	ldr	r12, psci_ns_stub_sram_lit	@ Linux pen slot
	mov	r9, #0x50
	str	r9, [r5]
#if PSCI_TRACE_ENABLE
	str	r9, [r6]
#endif
	str	r9, [r4]
	mrc	p15, 0, r10, c0, c0, 5		@ r10 = MPIDR
	mrs	r11, cpsr
	str	r10, [r4, #4]
	str	r11, [r4, #8]
	mov	r9, #0x51
	str	r9, [r5]
#if PSCI_TRACE_ENABLE
	str	r9, [r6]
#endif
	str	r9, [r4]
	mov	r0, #0
	mov	r1, #0
	mov	r2, #0
	mov	r3, #0
	mov	lr, #0
	cmp	r8, #0
	beq	1f
	ldr	r0, [r8]
	ldr	r1, [r8, #4]
	ldr	r2, [r8, #8]
	ldr	r3, [r8, #12]
	ldr	lr, [r8, #16]
1:
	str	r0, [r4, #12]
	str	r1, [r4, #44]
	str	r2, [r4, #20]
	str	r3, [r4, #24]
	str	lr, [r4, #28]
2:
	mov	r9, #0x52
	str	r9, [r5]
#if PSCI_TRACE_ENABLE
	str	r9, [r6]
#endif
	str	r9, [r4]
	cmp	r12, #0
	beq	3f
	str	lr, [r12]
3:
	dsb	sy
	mov	r9, #0x53
	str	r9, [r5]
#if PSCI_TRACE_ENABLE
	str	r9, [r6]
	mov	r9, #0xa0
	str	r9, [r6]
#endif
	str	r9, [r4]
	ldr	r0, psci_ns_stub_context_lit
	ldr	r0, [r0]
	ldr	r1, =PSCI_SELFTEST_CONTEXT_NS
	cmp	r0, r1
	bne	5f
	movw	r0, #0x0100
	movt	r0, #0x8200
	mov	r1, r10
	mov	r2, r11
	mov	r3, r3
	smc	#0
	mov	r9, #0xa1
#if PSCI_TRACE_ENABLE
	str	r9, [r6]
#endif
	b	6f
5:
	mov	r9, #0xa1
#if PSCI_TRACE_ENABLE
	str	r9, [r6]
#endif
6:
	ldr	r7, psci_ns_stub_entry_lit
	mov	r9, #0xa2
#if PSCI_TRACE_ENABLE
	str	r9, [r6]
#endif
	cmp	r7, #0
	beq	4f
	mov	r9, #0xa3
#if PSCI_TRACE_ENABLE
	str	r9, [r6]
#endif
	str	r7, [r4, #32]		@ log entry pointer
	bic	r11, r7, #1
	ldr	r0, [r11]
	str	r0, [r4, #36]
	ldr	r0, [r11, #4]
	str	r0, [r4, #40]
	ldr	r0, [r11, #8]
	str	r0, [r4, #48]
	ldr	r0, [r11, #12]
	str	r0, [r4, #52]
	mov	r9, #0x54
	str	r9, [r5]
#if PSCI_TRACE_ENABLE
	str	r9, [r6]
	mov	r9, #0xa4
	str	r9, [r6]
#endif
	str	r9, [r4]
	mov	r0, #0
	mov	r1, #0
	mov	r2, #0
	mov	r3, #0
	bx	r7
4:
	b	4b

.align 2
psci_ns_stub_diag_lit:
	.word	0
psci_ns_stub_stage_lit:
	.word	0
psci_ns_stub_sram_lit:
	.word	0
psci_ns_stub_trace_lit:
	.word	0
psci_ns_stub_secdata_lit:
	.word	0
psci_ns_stub_context_lit:
	.word	0
psci_ns_stub_entry_lit:
	.word	0
psci_ns_stub_end:
.previous

.section .rodata.psci_linux_entry, "a", %progbits
.align 2
.global psci_ns_linux_entry_start
.global psci_ns_linux_entry_end
.global psci_ns_linux_entry_target_lit
.global psci_ns_linux_entry_trace_lit
.arm
.align 2
psci_ns_linux_entry_start:
#if LOG_LEVEL >= LOG_DEBUG
	ldr	r6, psci_ns_linux_entry_target_lit
	ldr	r6, [r6]
	ldr	r8, =psci_ns_trace_phys_value
	ldr	r8, [r8]
	mov	r9, #0x32
	mrc	p15, 0, r10, c0, c0, 5
	mrs	r11, cpsr
	ldr	r4, =psci_ns_diag_phys_value
	ldr	r4, [r4]
	ldr	r5, =psci_ns_stage_phys_value
	ldr	r5, [r5]
	str	r9, [r5]
	str	r9, [r4]
	str	r10, [r4, #4]
	str	r11, [r4, #8]
	mov	r9, #0xb0
#if PSCI_TRACE_ENABLE
	str	r9, [r8]
#endif
	mov	r9, #0x51
	str	r9, [r5]
	str	r9, [r4]
	mov	r9, #0xb1
#if PSCI_TRACE_ENABLE
	str	r9, [r8]
#endif
	ldr	r12, [r6]
	str	r12, [r4, #60]
	ldr	r0, [r12]
	str	r0, [r4, #64]
	ldr	r0, [r12, #4]
	str	r0, [r4, #68]
	cmp	r12, #0
	beq	2f
	mov	r9, #0x53
	str	r9, [r5]
	str	r9, [r4]
	mov	r9, #0xb2
#if PSCI_TRACE_ENABLE
	str	r9, [r8]
#endif
	mov	r0, #0
	mov	r1, #0
	mov	r2, #0
	mov	r3, #0
	mov	r9, #0xb3
#if PSCI_TRACE_ENABLE
	str	r9, [r8]
#endif
	bx	r12
2:
	wfe
	b	2b
#else
	ldr	r6, psci_ns_linux_entry_target_lit
	ldr	r6, [r6]
	ldr	r12, [r6]
	cmp	r12, #0
	beq	1f
	mov	r0, #0
	mov	r1, #0
	mov	r2, #0
	mov	r3, #0
	bx	r12
1:
	wfe
	b	1b
#endif

psci_ns_linux_entry_target_lit:
	.word	0
psci_ns_linux_entry_trace_lit:
	.word	0
psci_ns_linux_entry_end:
.previous
