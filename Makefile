# Target
TARGET := awboot
CROSS_COMPILE ?= arm-none-eabi

# Log level defaults to info
LOG_LEVEL ?= 30

SRCS := main.c board.c

INCLUDE_DIRS :=-I . -I include -I lib
LIBS := -lgcc -nostdlib
DEFINES := -DLOG_LEVEL=$(LOG_LEVEL) -DBUILD_REVISION=$(shell cat .build_revision)

include	arch/arch.mk
include	lib/lib.mk

CFLAGS += -mcpu=cortex-a7 -mthumb-interwork -mthumb -mno-unaligned-access -mfpu=neon-vfpv4 -mfloat-abi=hard
CFLAGS += -ffast-math -ffunction-sections -fdata-sections -Os -std=gnu99 -Wall -Wno-unused-function -g -MMD $(INCLUDES) $(DEFINES)

ASFLAGS += $(CFLAGS)

LDFLAGS += $(CFLAGS) $(LIBS) -Wl,--gc-sections

STRIP=$(CROSS_COMPILE)-strip
CC=$(CROSS_COMPILE)-gcc
SIZE=$(CROSS_COMPILE)-size
OBJCOPY=$(CROSS_COMPILE)-objcopy

HOSTCC=gcc
HOSTSTRIP=strip

MAKE=make

DTB ?= sun8i-t113-mangopi-dual.dtb
KERNEL ?= zImage

all: git begin build mkboot

begin:
	@echo "---------------------------------------------------------------"
	@echo -n "Compiler version: "
	@$(CC) -v 2>&1 | tail -1

build_revision:
	@expr `cat .build_revision` + 1 > .build_revision

.PHONY: tools git begin build mkboot clean format
.SILENT:

git:
	cp -f tools/hooks/* .git/hooks/

build:: build_revision

# $(1): varient name
# $(2): values to remove from board.h
define VARIENT =

# Objects
$(1)_OBJ_DIR = build-$(1)
$(1)_BUILD_OBJS = $$(SRCS:%.c=$$($(1)_OBJ_DIR)/%.o)
$(1)_BUILD_OBJSA = $$(ASRCS:%.S=$$($(1)_OBJ_DIR)/%.o)
$(1)_OBJS = $$($(1)_BUILD_OBJSA) $$($(1)_BUILD_OBJS)

build:: $$($(1)_OBJ_DIR)/$$(TARGET)-boot.elf $$($(1)_OBJ_DIR)/$$(TARGET)-boot.bin $$($(1)_OBJ_DIR)/$$(TARGET)-fel.elf $$($(1)_OBJ_DIR)/$$(TARGET)-fel.bin

.PRECIOUS : $$($(1)_OBJS)
$$($(1)_OBJ_DIR)/$$(TARGET)-fel.elf: $$($(1)_OBJS)
	echo "  LD    $$@"
	$$(CC) -E -P -x c -D__RAM_BASE=0x00030000 ./arch/arm32/mach-t113s3/link.ld > $$($(1)_OBJ_DIR)/link-fel.ld
	$$(CC) $$^ -o $$@ -T $$($(1)_OBJ_DIR)/link-fel.ld $$(LDFLAGS) -Wl,-Map,$$($(1)_OBJ_DIR)/$$(TARGET)-fel.map

$$($(1)_OBJ_DIR)/$$(TARGET)-boot.elf: $$($(1)_OBJS)
	echo "  LD    $$@"
	$$(CC) -E -P -x c -D__RAM_BASE=0x00020000 ./arch/arm32/mach-t113s3/link.ld > $$($(1)_OBJ_DIR)/link-boot.ld
	$$(CC) $$^ -o $$@ -T $$($(1)_OBJ_DIR)/link-boot.ld $$(LDFLAGS) -Wl,-Map,$$($(1)_OBJ_DIR)/$$(TARGET)-boot.map

$$($(1)_OBJ_DIR)/$$(TARGET)-fel.bin: $$($(1)_OBJ_DIR)/$$(TARGET)-fel.elf
	@echo OBJCOPY $$@
	$$(OBJCOPY) -O binary $$< $$@

$$($(1)_OBJ_DIR)/$$(TARGET)-boot.bin: $$($(1)_OBJ_DIR)/$$(TARGET)-boot.elf
	@echo OBJCOPY $$@
	$$(OBJCOPY) -O binary $$< $$@

$$($(1)_OBJ_DIR)/%.o : %.c
	echo "  CC    $$@"
	mkdir -p $$(@D)
	$$(CC) $$(CFLAGS) -include $$($(1)_OBJ_DIR)/board.h $$(INCLUDE_DIRS) -c $$< -o $$@

$$($(1)_OBJ_DIR)/%.o : %.S
	echo "  CC    $$@"
	mkdir -p $$(@D)
	$$(CC) $$(ASFLAGS) $$(INCLUDE_DIRS) -c $$< -o $$@

$$($(1)_OBJS): $$($(1)_OBJ_DIR)/board.h

$$($(1)_OBJ_DIR)/board.h: board.h
	echo "  GEN   $$@"
	mkdir -p $$(@D)
	grep -v "$(2)" >$$@ <$$<

clean::
	rm -rf $$($(1)_OBJ_DIR)

-include $$(patsubst %.o,%.d,$$($(1)_OBJS))

endef

# build eMMC only image
$(eval $(call VARIENT,sdmmc,CONFIG_BOOT_SPINAND,CONFIG_BOOT_SDCARD))

clean::
	rm -f $(TARGET)-*.bin
	rm -f $(TARGET)-*.map
	rm -f *.img
	rm -f *.d
	$(MAKE) -C tools clean

format:
	find . -iname "*.h" -o -iname "*.c" | xargs clang-format --verbose -i

tools:
	$(MAKE) -C tools all

mkboot: build tools
	echo "SDMMC:"
	$(SIZE) build-sdmmc/$(TARGET)-boot.elf
	cp -f build-sdmmc/$(TARGET)-boot.bin $(TARGET)-boot-sd.bin
	tools/mksunxi $(TARGET)-boot-sd.bin 512

	echo "FEL:"
	cp -f build-sdmmc/$(TARGET)-boot.bin $(TARGET)-fel.bin
	tools/mksunxi $(TARGET)-fel.bin 8192

boot-fel:
	@set -e; \
	tmp=$$(mktemp); \
	trap 'rm -f $$tmp' EXIT; \
	len=$$(stat -c '%s' awboot-fel.bin); \
	xfel ddr t113-s3; \
	xfel write   0x00030000 awboot-fel.bin; \
	xfel read    0x00030000 $$len $$tmp; \
	if cmp -s awboot-fel.bin $$tmp; then \
		echo "XFEL verify: payload matches memory"; \
	else \
		echo "XFEL verify: payload mismatch" >&2; \
		exit 1; \
	fi; \
	xfel write32 0x42000024 0; \
	xfel exec    0x00030000
