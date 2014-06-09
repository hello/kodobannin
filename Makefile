# default target: all

.PHONY: all
all: b m

# apps & platforms

TEST_APPS = hello_world rtc_test imu_test ble_test pill_test
APPS = band bootloader morpheus pill $(TEST_APPS)

S110_PLATFORMS = band_EVT3 pca10001 pca10000 pill_EVT1
S310_PLATFORMS = pca10003
PLATFORMS = $(S110_PLATFORMS) $(S310_PLATFORMS)

# product aliases

.PHONY: b
b: band+band_EVT3 bootloader+band_EVT3

.PHONY: m
m: morpheus+EK bootloader+EK

# program targets

.PHONY: pb
pb: p-band+band_EVT3

.PHONY: pbb
pbb: p-band+bootloader+band_EVT3

.PHONY: pbl
pbl: p-bootloader+band_EVT3

.PHONY: pm
pm: p-morpheus+EK

# debug targets
.PHONY: gb
gb: g-band+band_EVT3

.PHONY: gbl
gbl: g-bootloader+band_EVT3

# tool paths


DEFAULT_GCC_ROOT = $(CURDIR)/tools/gcc-arm-none-eabi-4_7-2013q3
KODOBANNIN_GCC_ROOT ?= $(DEFAULT_GCC_ROOT)
DEFAULT_JLINK_ROOT = $(CURDIR)/tools/JLink_MacOSX_V474
KODOBANNIN_JLINK_ROOT ?= $(DEFAULT_JLINK_ROOT)
PREFIX=arm-none-eabi-
BIN = $(KODOBANNIN_GCC_ROOT)/bin
CC = $(BIN)/$(PREFIX)gcc
LD = $(BIN)/$(PREFIX)ld
AS = $(BIN)/$(PREFIX)as
OBJCOPY = $(BIN)/$(PREFIX)objcopy
JPROG=$(KODOBANNIN_JLINK_ROOT)/JLinkExe.command
JGDBServer=$(KODOBANNIN_JLINK_ROOT)/JLinkGDBServer.command

# J-Link

JLINK_OPTIONS = -device nrf51822 -if swd -speed 4000

# file paths

BUILD_DIR = build

HELLO_SRCS = \
	$(wildcard common/*.c) $(wildcard common/*.s) \
	$(wildcard ble/*.c) \
	$(wildcard ble/services/*.c) \
	$(wildcard micro-ecc/*.c) \

NRF_SRCS = \
	nRF51_SDK/nrf51422/Source/templates/system_nrf51.c \
	nRF51_SDK/nrf51422/Source/app_common/app_timer.c \
	nRF51_SDK/nrf51422/Source/app_common/crc16.c \
	nRF51_SDK/nrf51422/Source/ble/ble_advdata.c \
	nRF51_SDK/nrf51422/Source/ble/ble_conn_params.c \
	nRF51_SDK/nrf51422/Source/ble/ble_flash.c \
	nRF51_SDK/nrf51422/Source/ble/ble_radio_notification.c \
	nRF51_SDK/nrf51422/Source/ble/ble_services/ble_srv_common.c \
	nRF51_SDK/nrf51422/Source/ble/ble_services/ble_dis.c \
	nRF51_SDK/nrf51422/Source/nrf_delay/nrf_delay.c \
	nRF51_SDK/nrf51422/Source/app_common/app_scheduler.c \
	nRF51_SDK/nrf51422/Source/app_common/app_gpiote.c \
	nRF51_SDK/nrf51422/Source/app_common/pstorage.c \
	nRF51_SDK/nrf51422/Source/nrf_nvmc/nrf_nvmc.c \
	nRF51_SDK/nrf51422/Source/twi_master/twi_hw_master.c \
	nRF51_SDK/nrf51422/Source/sd_common/softdevice_handler.c \

SRCS = $(HELLO_SRCS) $(NRF_SRCS)

INCS =  ./ \
	./ble \
	./ble/services \
	./drivers \
	./common \
	./micro-ecc \
	./common/hello_bootloader \
	./nRF51_SDK/nrf51422/Include \
	./nRF51_SDK/nrf51422/Include/app_common \
	./nRF51_SDK/nrf51422/Include/gcc \
	./nRF51_SDK/nrf51422/Include/ble \
	./nRF51_SDK/nrf51422/Include/ble/ble_services/ \
	./nRF51_SDK/nrf51422/Include/sd_common/ \
	./nRF51_SDK/nrf51422/Include/s310/ \


# SoftDevice

SOFTDEVICE_SRC = SoftDevice/s310_nrf51422_1.0.0_softdevice.hex
SOFTDEVICE_MAIN = $(BUILD_DIR)/$(basename $(notdir $(SOFTDEVICE_SRC)))_main.bin
SOFTDEVICE_UICR = $(BUILD_DIR)/$(basename $(notdir $(SOFTDEVICE_SRC)))_uicr.bin
SOFTDEVICE_BINARIES = $(SOFTDEVICE_MAIN) $(SOFTDEVICE_UICR)

.PHONY: SoftDevice
SoftDevice: $(SOFTDEVICE_BINARIES)

$(SOFTDEVICE_BINARIES):: $(BUILD_DIR)
$(SOFTDEVICE_BINARIES):: | $(CC)

$(SOFTDEVICE_MAIN):: $(SOFTDEVICE_SRC)
	$(OBJCOPY) -I ihex -O binary --remove-section .sec3 $< $@

$(SOFTDEVICE_UICR):: $(SOFTDEVICE_SRC)
	$(OBJCOPY) -I ihex -O binary --only-section .sec3 $< $@


# SHA-1
%.sha1: %.bin
	openssl sha1 -binary $< > $@
	stat -f "%Xz" $< | xxd -r -p | dd conv=swab 2> /dev/null >> $@


# gdb support
JGDBServer=$(KODOBANNIN_JLINK_ROOT)/JLinkGDBServer.command

.PHONY: gdbs
gdbs:
	$(JGDBServer) $(JLINK_OPTIONS)

# clean
.PHONY: clean
clean:
	-rm -rf $(BUILD_DIR)


# git description
GIT_DESCRIPTION = $(shell git describe --all --long --dirty)
GIT_DESCRIPTION_C_CONTENTS = const char* const GIT_DESCRIPTION = "$(GIT_DESCRIPTION)";
ifneq ($(GIT_DESCRIPTION_C_CONTENTS), $(shell cat $(BUILD_DIR)/git_description.c 2> /dev/null))
.PHONY: git_description.c
endif
$(BUILD_DIR)/git_description.c:
	@mkdir -p $(dir $@)
	@echo '$(GIT_DESCRIPTION_C_CONTENTS)' > $@

$(BUILD_DIR)/git_description.o: $(BUILD_DIR)/git_description.c | $(CC)
	$(CC) $(CFLAGS) -c -o $@ $<


# compile flags
DEBUG = 1

ifeq ($(DEBUG), 1)
OPTFLAGS=-O0 -g -DDEBUG_SERIAL=1
SRCS += nRF51_SDK/nrf51422/Source/simple_uart/simple_uart.c
else
OPTFLAGS=-Os -g -DECC_ASM=1
endif

NRFREV=NRF51422_QFAA_ED
NRFFLAGS=-DBOARD_PCA10001 -DNRF51 -DDO_NOT_USE_DEPRECATED -D$(NRFREV) -DBLE_STACK_SUPPORT_REQD # -DANT_STACK_SUPPORT_REQD
MICROECCFLAGS=-DECC_CURVE=6 # see ecc.h for details
ARCHFLAGS=-mcpu=cortex-m0 -mthumb -march=armv6-m
LDFLAGS := `$(CC) $(ARCHFLAGS) -print-libgcc-file-name` --gc-sections -Lstartup
WARNFLAGS=-Wall -Wno-packed-bitfield-compat -Wno-format
ASFLAGS := $(ARCHFLAGS)
CFLAGS := -std=gnu99 -fshort-enums -fdata-sections -ffunction-sections $(ARCHFLAGS) $(MICROECCFLAGS) $(NRFFLAGS) $(OPTFLAGS) $(WARNFLAGS)


# functions

## returns the build directory for a product
product-dir = $(BUILD_DIR)/$1+$2

# functions for source files, without a build/ prefix. $1 = app
app-srcs = $(SRCS) $(wildcard $1/*.c)
platform-srcs = $(SRCS) $(wildcard $1/*.c)

product-incs-flags = $(addprefix -I,$1 $2 $(INCS))

# functions for object files, with a build/ prefix. $1 = app, $2 = platform
define rule-product-objs
$(foreach src, $(call app-srcs,$1) $(call platform-srcs,$2), $(eval $(call rule-product-obj )))
endef

product-objs = $(patsubst %.c,%.o,$(patsubst %.s,%.o,$(addprefix $(call product-dir,$1,$2)/, $(call app-srcs,$1) $(call platform-srcs,$2))))

## creates a rule for a product. $1 = app, $2 = platform
define rule-product

.PHONY: $1+$2
$1+$2: $(BUILD_DIR)/$1+$2.bin

$(BUILD_DIR)/$1+$2.bin: $(BUILD_DIR)/$1+$2.elf
	$(OBJCOPY) -O binary $$< $$@

$(BUILD_DIR)/$1+$2/%.o: %.c | $(CC)
	@mkdir -p $$(dir $$@)
	$(CC) $(CFLAGS) $(call product-incs-flags,$1,$2) -MMD -c -o $$@ $$<

$(BUILD_DIR)/$1+$2/%.o: %.s | $(CC)
	@mkdir -p $$(dir $$@)
	$(AS) $(ASFLAGS) -o $$@ $$<

$1+$2_OBJS = $(call product-objs,$1,$2)

$1+$2_DEPS = $$($1+$2_OBJS:.o=.d)
-include $$($1+$2_DEPS)

$(BUILD_DIR)/$1+$2.elf: $(BUILD_DIR)/git_description.o $$($1+$2_OBJS)
$(BUILD_DIR)/$1+$2.elf: startup/$1.ld startup/common.ld startup/memory.ld | $(dir $@) $(CC)
	$(LD) $(LDFLAGS) -o $$@ $$(filter %.o, $$^) -Tstartup/$1.ld $(LDFLAGS)

$(BUILD_DIR)/$1+$2/$1 $(BUILD_DIR)/$1+$2/$2 $(BUILD_DIR)/$1+$2:
	mkdir -p $(call product-dir,$1,$2)

$(BUILD_DIR)/$1+$2.jlink: prog/$1+$2.jlink.in
	@mkdir -p $$(dir $$@)
	sed \
	 -e 's,$$$$PWD,$(CURDIR),g' \
	 -e 's,$$$$BUILD_DIR,$$(abspath $(BUILD_DIR)),g' \
	 -e 's,$$$$SOFTDEVICE_MAIN,$$(abspath $(SOFTDEVICE_MAIN)),g' \
	 -e 's,$$$$SOFTDEVICE_UICR,$$(abspath $(SOFTDEVICE_UICR)),g' \
	 -e 's,$$$$BIN,$$(abspath $(BUILD_DIR)/$1+$2.bin),g' \
         -e 's,$$$$1,$1,g' \
	 -e 's,$$$$2,$2,g' \
          < $$< > $$@

.PHONY: p-$1+$2 p-$1+bootloader+$2
p-$1+$2 p-$1+bootloader+$2: $(BUILD_DIR)/$1+$2.bin $(SOFTDEVICE_BINARIES)
p-$1+$2: $(BUILD_DIR)/$1+$2.jlink
	@$(JPROG) $(JLINK_OPTIONS) < $$<
p-$1+bootloader+$2: $(BUILD_DIR)/$1+bootloader+$2.jlink $(BUILD_DIR)/bootloader+$2.bin $(BUILD_DIR)/$1+$2.sha1
	@$(JPROG) $(JLINK_OPTIONS) < $$<

.PHONY: g-$1+$2
g-$1+$2:
	$(BIN)/arm-none-eabi-gdb $(BUILD_DIR)/$1+$2.elf

endef



# auto-generate product rules

$(foreach platform, $(PLATFORMS),$(foreach app, $(APPS), $(eval $(call rule-product,$(app),$(platform)))))

# downloading required dependencies (GCC, J-Link)

GCC_PACKAGE_BASENAME = gcc-arm-none-eabi-4_7-2013q3-20130916-mac.tar.bz2
GCC_PACKAGE_URL = https://launchpadlibrarian.net/151487551/$(GCC_PACKAGE_BASENAME)

.INTERMEDIATE: $(CURDIR)/tools/$(GCC_PACKAGE_BASENAME)
$(CURDIR)/tools/$(GCC_PACKAGE_BASENAME):
	$(info [DOWNLOADING GCC...])
	@(cd $(CURDIR)/tools && curl -O $(GCC_PACKAGE_URL))

$(DEFAULT_GCC_ROOT)/bin/$(PREFIX)gcc: | $(CURDIR)/tools/$(GCC_PACKAGE_BASENAME)
	$(info [UNTARRING GCC...])
	@(cd $(CURDIR)/tools && tar jxf gcc-arm-none-eabi-4_7-2013q3-20130916-mac.tar.bz2)
# submodule inits

GIT_SUBMODULE_DEPENDENCY_FILES = $(SOFTDEVICE_SRC) micro-ecc/README.md nRF51422/Documentation/index.html $(CURDIR)/tools/JLink_MacOSX_V474/JLinkExe.command

$(GIT_SUBMODULE_DEPENDENCY_FILES):
	$(info [GIT SUBMODULE UPDATE])
	@git submodule init
	@git submodule update



# understanding this:
# * objects are part of the _product_
# * for a new platform: .ld file in startup/, prog file in .
# coding style:
# * function names should be lowercase, with dashes separating words
# * variable names use UPPERCASE
# * function names are lowercase
