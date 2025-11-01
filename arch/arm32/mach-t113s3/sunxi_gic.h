#ifndef __SUNXI_GIC_H__
#define __SUNXI_GIC_H__

#include <stdint.h>

#define SUNXI_GICD_BASE 0x03021000U
#define SUNXI_GICC_BASE 0x03022000U

/* Distributor registers */
#define GICD_CTLR        0x000U
#define GICD_TYPER       0x004U
#define GICD_IGROUPR(n)   (0x080U + ((n) * 4U))
#define GICD_ICENABLER(n) (0x180U + ((n) * 4U))
#define GICD_ISPENDR(n)   (0x200U + ((n) * 4U))
#define GICD_ICPENDR(n)   (0x280U + ((n) * 4U))
#define GICD_ISACTIVER(n) (0x300U + ((n) * 4U))
#define GICD_ICACTIVER(n) (0x380U + ((n) * 4U))

/* CPU interface registers */
#define GICC_CTLR 0x000U
#define GICC_PMR  0x004U
#define GICC_BPR  0x008U
#define GICC_IAR  0x00CU
#define GICC_EOIR 0x010U
#define GICC_RPR  0x014U
#define GICC_HPPIR 0x018U

void sunxi_gic_init_non_secure(void);
void sunxi_gic_disable(void);

#endif /* __SUNXI_GIC_H__ */
