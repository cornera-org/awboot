/*
 * exception.c
 *
 * Copyright(c) 2007-2022 Jianjun Jiang <8192542@qq.com>
 * Official site: http://xboot.org
 * Mobile phone: +86-18665388956
 * QQ: 8192542
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <inttypes.h>
#include <arm32.h>
#include "debug.h"
#include "common.h"
#include "sunxi_gic.h"
#include "io.h"

struct arm_regs_t {
	uint32_t esp;
	uint32_t cpsr;
	uint32_t r[13];
	uint32_t sp;
	uint32_t lr;
	uint32_t pc;
};

void arm32_fault_stub(const char *reason, struct arm_regs_t *regs)
{
	if (!reason || !regs)
		return;

	error("%s fault: pc=%08lx lr=%08lx spsr=%08lx\r\n", reason, regs->pc, regs->lr, regs->cpsr);
}

static void show_regs(struct arm_regs_t *regs)
{
	int i;

	error("pc : [<%08lx>] lr : [<%08lx>] cpsr: %08lx\r\n", regs->pc, regs->lr, regs->cpsr);
	error("sp : %08lx esp : %08lx\r\n", regs->sp, regs->esp);
	for (i = 12; i >= 0; i--)
		error("r%-2d: %08lx\r\n", i, regs->r[i]);
	error("\r\n");
}

void arm32_do_undefined_instruction(struct arm_regs_t *regs)
{
	show_regs(regs);
	// regs->pc += 4;
	fatal("undefined_instruction\r\n");
}

void arm32_do_software_interrupt(struct arm_regs_t *regs)
{
	show_regs(regs);
	// regs->pc += 4;
	fatal("software_interrupt\r\n");
}

void arm32_do_prefetch_abort(struct arm_regs_t *regs)
{
	show_regs(regs);
	// regs->pc += 4;
	fatal("prefetch_abort\r\n");
}

void arm32_do_data_abort(struct arm_regs_t *regs)
{
	show_regs(regs);
	// regs->pc += 4;
	fatal("data_abort\r\n");
}

extern volatile uint32_t psci_ns_entered;

void arm32_do_irq(struct arm_regs_t *regs)
{
	uint32_t iar = read32(SUNXI_GICC_BASE + GICC_IAR);
	uint32_t irq = iar & 0x3ffU;
	uint32_t gicc_ctlr  = read32(SUNXI_GICC_BASE + GICC_CTLR);
	uint32_t gicc_pmr   = read32(SUNXI_GICC_BASE + GICC_PMR);
	uint32_t gicc_bpr   = read32(SUNXI_GICC_BASE + GICC_BPR);
	uint32_t gicc_hppir = read32(SUNXI_GICC_BASE + GICC_HPPIR);
	uint32_t gicc_rpr   = read32(SUNXI_GICC_BASE + GICC_RPR);
	uint32_t gicd_ctlr  = read32(SUNXI_GICD_BASE + GICD_CTLR);
	uint32_t gicd_ispend0 = read32(SUNXI_GICD_BASE + GICD_ISPENDR(0));
	uint32_t gicd_ispend1 = read32(SUNXI_GICD_BASE + GICD_ISPENDR(1));
	uint32_t gicd_isact0  = read32(SUNXI_GICD_BASE + GICD_ISACTIVER(0));
	uint32_t gicd_isact1  = read32(SUNXI_GICD_BASE + GICD_ISACTIVER(1));

	debug("IRQ: GICC_CTLR=0x%08" PRIx32 " PMR=0x%08" PRIx32 " BPR=0x%08" PRIx32
	      " HPPIR=0x%08" PRIx32 " RPR=0x%08" PRIx32 "\r\n",
	      gicc_ctlr, gicc_pmr, gicc_bpr, gicc_hppir, gicc_rpr);
	debug("IRQ: GICD_CTLR=0x%08" PRIx32 " ISPENDR0=0x%08" PRIx32 " ISPENDR1=0x%08" PRIx32
	      " ISACTIVER0=0x%08" PRIx32 " ISACTIVER1=0x%08" PRIx32 "\r\n",
	      gicd_ctlr, gicd_ispend0, gicd_ispend1, gicd_isact0, gicd_isact1);

	if (psci_ns_entered) {
		if (irq < 1022U) {
			uint32_t bank = irq >> 5;
			uint32_t mask = (uint32_t)1U << (irq & 0x1fU);
			write32(SUNXI_GICD_BASE + GICD_ICENABLER(bank), mask);
			write32(SUNXI_GICD_BASE + GICD_ICPENDR(bank), mask);
			write32(SUNXI_GICD_BASE + GICD_ICACTIVER(bank), mask);
		}
		write32(SUNXI_GICC_BASE + GICC_EOIR, iar);
		debug("IRQ: suppressed IRQ%" PRIu32 " after PSCI hand-off\r\n", irq);
		return;
	}

	write32(SUNXI_GICC_BASE + GICC_EOIR, iar);
	show_regs(regs);
	// regs->pc += 4;
	fatal("undefined IRQ\r\n");
}

void arm32_do_fiq(struct arm_regs_t *regs)
{
	show_regs(regs);
	// regs->pc += 4;
	fatal("undefined FIQ\r\n");
}
