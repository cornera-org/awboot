ARCH := arch
SOC:=$(ARCH)/arm32/mach-t113s3

INCLUDE_DIRS += -I $(ARCH)/arm32/include -I $(SOC)/include -I $(SOC) -I $(SOC)/mmc

CFLAGS += -DCOUNTER_FREQUENCY=24000000
CFLAGS += -DPSCI_CPU_SELFTEST=0
CFLAGS += -DPSCI_TRACE_ENABLE=1

LINUX_BUILD_DIR ?= $(abspath ../mt4-buildroot/buildroot-main/output/build/linux-6.12.52)
LINUX_SYSTEM_MAP := $(LINUX_BUILD_DIR)/System.map

ifneq ($(wildcard $(LINUX_SYSTEM_MAP)),)
LINUX_SECONDARY_DATA_VIRT := $(strip $(shell awk '$$3=="secondary_data"{print $$1; exit}' $(LINUX_SYSTEM_MAP)))
LINUX_SECONDARY_STARTUP_VIRT := $(strip $(shell awk '$$3=="secondary_startup"{print $$1; exit}' $(LINUX_SYSTEM_MAP)))
LINUX_HYP_VECTORS_VIRT := $(strip $(shell awk '$$3=="__hyp_stub_vectors"{print $$1; exit}' $(LINUX_SYSTEM_MAP)))
ifneq ($(LINUX_SECONDARY_DATA_VIRT)$(LINUX_SECONDARY_STARTUP_VIRT),)
PSCI_LINUX_SECONDARY_DATA_OFFSET := $(strip $(shell printf "0x%X" $$((0x$(LINUX_SECONDARY_DATA_VIRT) - 0x$(LINUX_SECONDARY_STARTUP_VIRT)))))
CFLAGS += -DPSCI_LINUX_SECONDARY_DATA_OFFSET=$(PSCI_LINUX_SECONDARY_DATA_OFFSET)
else
$(warning Failed to compute PSCI_LINUX_SECONDARY_DATA_OFFSET from $(LINUX_SYSTEM_MAP))
endif
ifneq ($(LINUX_HYP_VECTORS_VIRT),)
PSCI_LINUX_HYP_VECTORS_OFFSET := $(strip $(shell printf "0x%X" $$((0x$(LINUX_HYP_VECTORS_VIRT) - 0x$(LINUX_SECONDARY_STARTUP_VIRT)))))
CFLAGS += -DPSCI_LINUX_HYP_VECTORS_OFFSET=$(PSCI_LINUX_HYP_VECTORS_OFFSET)
else
$(warning Failed to locate __hyp_stub_vectors in $(LINUX_SYSTEM_MAP); Hyp vector offset disabled)
endif
else
$(warning Linux System.map missing at $(LINUX_SYSTEM_MAP); PSCI instrumentation offsets disabled)
endif

ASRCS	+=  $(SOC)/start.S
ASRCS	+=  $(SOC)/memcpy.S
ASRCS	+=  $(SOC)/psci_tramp.S
ASRCS	+=  $(SOC)/psci_monitor.S

ifneq ($(wildcard $(SOC)/dram_sun20i_d1.c),)
SRCS	+=  $(SOC)/dram_sun20i_d1.c
else
SRCS	+=  $(SOC)/dram.c
endif
SRCS	+=  $(SOC)/sunxi_usart.c
SRCS	+=  $(SOC)/arch_timer.c
SRCS	+=  $(SOC)/sunxi_gpio.c
SRCS	+=  $(SOC)/sunxi_clk.c
SRCS	+=  $(SOC)/exception.c
SRCS	+=  $(SOC)/sunxi_wdg.c
SRCS	+=  $(SOC)/psci.c
SRCS	+=  $(SOC)/psci_selftest.c
SRCS	+=  $(SOC)/sunxi_security.c

USE_SPI = $(shell grep -E "^\#define CONFIG_BOOT_SPI" board.h)
ifneq ($(USE_SPI),)
SRCS	+=  $(SOC)/sunxi_spi.c
SRCS	+=  $(SOC)/sunxi_dma.c
endif

USE_SDMMC = $(shell grep -E "^\#define CONFIG_BOOT_(SDCARD|MMC)" board.h)
ifneq ($(USE_SDMMC),)
SRCS	+=  $(SOC)/sdmmc.c
SRCS	+=  $(SOC)/sunxi_sdhci.c
endif
