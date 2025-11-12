#define PSCI_NS_TRACE_PHYS    (0x47ff0030)

.section .monitor_vectors_ns, "ax", %progbits
.align 5
.global psci_monitor_vector_table_ns
psci_monitor_vector_table_ns:
	b psci_monitor_ns_handler
	b psci_monitor_ns_handler
	b psci_monitor_ns_handler
	b psci_monitor_ns_handler
	b psci_monitor_ns_handler
	b psci_monitor_ns_handler
	b psci_monitor_ns_handler
	b psci_monitor_ns_handler

psci_monitor_ns_handler:
	sub	sp, sp, #16
	stmia	sp, {r0 - r3}
	ldr	r0, =PSCI_NS_TRACE_PHYS
	ldr	r1, [sp]			@ original r0 from NS world
	cmp	r1, #0
	bne	0f
	ldr	r1, [r0, #0x18]
	cmp	r1, #0
	bne	0f
	ldr	r1, [r0, #0x24]
0:
	mov	r2, r1
	cmp	r2, #0
	beq	1f
	mcr	p15, 0, r2, c1, c0, 0
	dsb	sy
	isb
	mov	r1, #0x76
	str	r1, [r0]
	mov	r1, #0xe7
	str	r1, [r0, #0x2c]
1:
	ldmia	sp, {r0 - r3}
	add	sp, sp, #16
	movs	pc, lr
