.syntax unified
.arch armv7-a
.arch_extension sec
.arch_extension virt

#define ARMV7_SVC_MODE        0x13
#define F_BIT                 (1 << 6)
#define I_BIT                 (1 << 7)
#define A_BIT                 (1 << 8)
#define T_BIT                 (1 << 5)

#define GIC_DIST_BASE         0x03021000
#define GIC_CPU_BASE          0x03022000

.section .text.psci_monitor, "ax", %progbits

.align 5
.globl psci_monitor_vector_table
psci_monitor_vector_table:
	b	psci_secure_monitor
	b	psci_secure_default
	b	psci_secure_monitor
	b	psci_secure_default
	b	psci_secure_default
	b	psci_secure_default
	b	psci_secure_default
	b	psci_secure_default

.align 5
.globl psci_psci_vector_table
psci_psci_vector_table:
	b	psci_secure_default
	b	psci_secure_default
	b	psci_smc_entry
	b	psci_secure_default
	b	psci_secure_default
	b	psci_secure_default
	b	psci_secure_default
	b	psci_secure_default

.type	psci_secure_default, %function
psci_secure_default:
	b	psci_secure_default

.extern psci_smc_entry

.type	psci_nonsec_init, %function
psci_nonsec_init:
	ldr	r3, =GIC_DIST_BASE
	mvn	r1, #0
	str	r1, [r3, #0x80]

	ldr	r3, =GIC_CPU_BASE
	mov	r1, #3
	str	r1, [r3, #0x0]
	mov	r1, #0xff
	str	r1, [r3, #0x4]
	mov	r1, #0x0
	str	r1, [r3, #0x8]

	mrc	p15, 0, r0, c1, c1, 2
	ldr	r1, =0x00043fff
	orr	r0, r0, r1
	mcr	p15, 0, r0, c1, c1, 2

	ldr	r1, =psci_monitor_vector_table
	mcr	p15, 0, r1, c12, c0, 1
	isb
	bx	lr

.type	psci_secure_monitor, %function
psci_secure_monitor:
	ldr	r5, =psci_psci_vector_table
	mcr	p15, 0, r5, c12, c0, 1
	isb

	bl	psci_nonsec_init

	mrc	p15, 0, r5, c1, c1, 0
	bic	r5, r5, #0x4a
	orr	r5, r5, #0x31
	mcr	p15, 0, r5, c1, c1, 0
	isb

	mov	lr, ip
	mov	ip, #(ARMV7_SVC_MODE | F_BIT | I_BIT | A_BIT)
	tst	lr, #1
	orrne	ip, ip, #T_BIT
	msr	spsr_cxfs, ip
	movs	pc, lr

.globl psci_do_nonsec_entry
.type	psci_do_nonsec_entry, %function
psci_do_nonsec_entry:
	mov	ip, r0
	mov	r0, r1
	mov	r1, r2
	mov	r2, r3
	smc	#0
	bx	lr
