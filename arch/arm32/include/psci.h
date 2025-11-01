#ifndef __PSCI_H__
#define __PSCI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void psci_init(void);
int32_t psci_handle_smc(uint32_t fid, uint32_t arg0, uint32_t arg1, uint32_t arg2);

#ifdef __cplusplus
}
#endif

#endif /* __PSCI_H__ */
