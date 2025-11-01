#ifndef __SUNXI_CPUCFG_H__
#define __SUNXI_CPUCFG_H__

#include <stdint.h>
#include <stdbool.h>

#define SUNXI_CPU_MAX_COUNT 2U

void sunxi_cpucfg_init(void);
int  sunxi_cpucfg_cpu_on(uint32_t cpu, uint32_t rvbar);
void sunxi_cpucfg_cpu_off(uint32_t cpu);
bool sunxi_cpucfg_cpu_is_on(uint32_t cpu);

#endif /* __SUNXI_CPUCFG_H__ */
