# since we append a byte of MAC Address to the name now...
BLE_DEVICE_NAME := Band

# default parallel
MAKEFLAGS = -j$(shell sysctl -n hw.ncpu)

# tool locations
KODOBANNIN_GCC_ROOT ?= ~/Downloads/gcc-arm-none-eabi-4_7-2013q3/
KODOBANNIN_JLINK_ROOT ?= ~/Work/jlink_462b
BIN = $(KODOBANNIN_GCC_ROOT)/bin
JLINK_BIN = $(KODOBANNIN_JLINK_ROOT)

# enable debug mode?
DEBUG = 1

# build directory
BUILD_DIR = build

# target to build
APP = $(BUILD_DIR)/app.bin
BOOTLOADER = $(BUILD_DIR)/bootloader.bin
SOFTDEVICE_BINARIES = $(BUILD_DIR)/softdevice_uicr.bin $(BUILD_DIR)/softdevice_main.bin

# nRF51822 revision (for PAN workarounds in nRF SDK)
NRFREV = NRF51822_QFAA_CA

SRCS = \
	error_handler.c \
	sha1.c \
	util.c \
	$(BUILD_DIR)/git_description.c \
	$(wildcard drivers/*.c) \
	$(wildcard ble/*.c) \
	$(wildcard ble/services/*.c) \
	$(wildcard micro-ecc/*.c) \
	startup/gcc_startup_nrf51.s \
	nRF51_SDK/nrf51822/Source/templates/system_nrf51.c \
	nRF51_SDK/nrf51822/Source/app_common/app_timer.c \
	nRF51_SDK/nrf51422/Source/app_common/crc16.c \
	nRF51_SDK/nrf51822/Source/ble/ble_advdata.c \
	nRF51_SDK/nrf51822/Source/ble/ble_bondmngr.c \
	nRF51_SDK/nrf51822/Source/ble/ble_conn_params.c \
	nRF51_SDK/nrf51822/Source/ble/ble_flash.c \
	nRF51_SDK/nrf51822/Source/ble/ble_stack_handler.c \
	nRF51_SDK/nrf51822/Source/ble/ble_radio_notification.c \
	nRF51_SDK/nrf51822/Source/ble/ble_services/ble_srv_common.c \
	nRF51_SDK/nrf51822/Source/ble/ble_services/ble_dis.c \
	nRF51_SDK/nrf51822/Source/nrf_delay/nrf_delay.c \
	nRF51_SDK/nrf51822/Source/app_common/app_scheduler.c \
	nRF51_SDK/nrf51822/Source/app_common/app_gpiote.c \

APP_SRCS = \
	$(SRCS) \
	hrs.c \
	imu.c \
	main.c \
	$(NULL)

BOOTLOADER_SRCS = \
	$(SRCS) \
	$(wildcard hello_bootloader/*.c) \
	hello_bootloader/ble/ble_services/ble_dfu.c \
	ecc_benchmark.c \

INCS =  ./ \
	./ble \
	./ble/services \
	./micro-ecc \
	./hello_bootloader \
	./nRF51_SDK/nrf51822/Include \
	./nRF51_SDK/nrf51822/Include/app_common \
	./nRF51_SDK/nrf51822/Include/gcc \
	./nRF51_SDK/nrf51822/Include/ble \
	./nRF51_SDK/nrf51822/Include/ble/ble_services/ \
	./nRF51_SDK/nrf51822/Include/ble/softdevice/ \
#	./SoftDevice/s110_nrf51822_5.2.1_API/include \


# optimization flags

ifeq ($(DEBUG), 1)
OPTFLAGS=-O0 -g -DDEBUG_SERIAL=1
SRCS += nRF51_SDK/nrf51822/Source/simple_uart/simple_uart.c
else
OPTFLAGS=-Os -g -DECC_ASM=1
endif

# compiler warnings
WARNFLAGS=-Wall
#-Wstrict-prototypes -Wmissing-prototypes
#-std=c99
#    -Wno-missing-braces -Wno-missing-field-initializers -Wformat=2 \
#    -Wswitch-default -Wswitch-enum -Wcast-align -Wpointer-arith \
#    -Wundef -Wcast-qual -Wshadow -Wunreachable-code \
#    -Wlogical-op -Wfloat-equal -Wstrict-aliasing=2 -Wredundant-decls \
#    -Werror

    #-Wbad-function-cast -Wstrict-overflow=5 \
#-Wall -pedantic -Wall -Wshadow -Wpointer-arith -Wcast-qual \


# micro-ecc config - see ecc.h for details
MICROECCFLAGS=-DECC_CURVE=6

NRFFLAGS=-DBOARD_PCA10001 -DNRF51 -DDO_NOT_USE_DEPRECATED -D$(NRFREV)

###############################################
#                                             #
# YOU SHOULD NOT NEED TO EDIT PAST THIS POINT #
#                                             #
###############################################

ARCHFLAGS=-mcpu=cortex-m0 -mthumb -march=armv6-m

# keep intermediary files such as .elf (for symbols)
.SECONDARY:

# Tool setup
LIBGCC=$(shell find $(BIN)/.. -name libgcc.a|grep armv6-m)
SOFTDEV_SRC=$(shell find SoftDevice -iname "s110*.hex")
LIB=$(dir $(shell python -c 'import os; print os.path.realpath("$(LIBGCC)")'))
PREFIX=arm-none-eabi-
CC=$(BIN)/$(PREFIX)gcc
LD=$(BIN)/$(PREFIX)ld
AS=$(BIN)/$(PREFIX)as
OBJCOPY=$(BIN)/$(PREFIX)objcopy
STRIP=$(BIN)/$(PREFIX)strip
JPROG=$(JLINK_BIN)/JLinkExe.command
JGDBServer=$(JLINK_BIN)/JLinkGDBServer.command

ARCHFLAGS=-mcpu=cortex-m0 -mthumb -march=armv6-m
ASFLAGS := $(ARCHFLAGS)
CFLAGS := -fshort-enums -fdata-sections -ffunction-sections -MMD $(ARCHFLAGS) $(addprefix -I,$(INCS)) $(MICROECCFLAGS) $(NRFFLAGS) $(OPTFLAGS) $(WARNFLAGS)
LDFLAGS := -L$(LIB) -lgcc --gc-sections -Lstartup

ifneq ($(BLE_DEVICE_NAME),)
CFLAGS += -DBLE_DEVICE_NAME="\"$(BLE_DEVICE_NAME)\""
else
$(error Please specify a device name with BLE_DEVICE_NAME env variable)
endif

#-L/Users/jkelley/Downloads/gcc-arm-none-eabi-4_7-2013q1/lib/gcc/arm-none-eabi/4.7.3/armv6-m -lgcc
#-T./nRF51_SDK/nrf51822/Source/templates/gcc/gcc_nrf51_s110_xxaa.ld -I./nRF51_SDK/nrf51822/Source/templates/gcc/

ALL_SRCS = $(SRCS) $(APP_SRCS) $(BOOTLOADER_SRCS)
ALL_OBJS = $(patsubst %.c, %.o, $(patsubst %.s, %.o, $(ALL_SRCS)))
ALL_DEPS = $(addprefix $(BUILD_DIR)/, $(ALL_OBJS:.o=.d))

# all target (must be first rule): build app + bootloader

.PHONY: all jl
all: $(APP) $(BOOTLOADER) SoftDevice

jl: all $(BUILD_DIR)/jl.jlink $(BUILD_DIR)/app.sha1
	$(JLINK_COMMANDS)

# build dir

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

# jlink invocation

$(BUILD_DIR)/%.jlink: prog/%.jlink.in
	$(info [JLINK_SCRIPT] $@)
	sed -e 's,$$PWD,$(PWD),g' -e 's,$$BUILD_DIR,$(abspath $(BUILD_DIR)),g' < $< > $@

JLINK_COMMANDS = \
	$(info [JPROG] $@.jlink) \
	@$(JPROG) < $(BUILD_DIR)/$@.jlink

jlink:
	@$(JPROG)

reset:
	@/usr/bin/printf 'r\nq' | $(JPROG)

# sha1 invocation

%.sha1: %.bin
	$(info [SHA1] $@)
	@openssl sha1 -binary $< > $@
	@stat -f "%Xz" $< | xxd -r -p | dd conv=swab 2> /dev/null >> $@

# git description

GIT_DESCRIPTION = $(shell git describe --all --long --dirty)
GIT_DESCRIPTION_C_CONTENTS = const char* const GIT_DESCRIPTION = "$(GIT_DESCRIPTION)";
ifneq ($(GIT_DESCRIPTION_C_CONTENTS), $(shell cat $(BUILD_DIR)/git_description.c 2> /dev/null))
.PHONY: git_description.c
endif
$(BUILD_DIR)/git_description.c:
	$(info [GIT_DESCRIPTION] git_description.c)
	@echo '$(GIT_DESCRIPTION_C_CONTENTS)' > $@

# gdb

.PHONY: gdbs gdb blgdb cgdb

gdbs:
	$(JGDBServer) -if SWD -device nRF51822 -speed 4000

gdb:
	$(BIN)/arm-none-eabi-gdb $(BUILD_DIR)/app.elf -ex "tar remote :2331" -ex "mon reset"

blgdb:
	$(BIN)/arm-none-eabi-gdb $(BUILD_DIR)/bootloader.elf -ex "tar remote :2331" -ex "mon reset"

cgdb:
	cgdb -d $(BIN)/arm-none-eabi-gdb -- $(BUILD_DIR)/app.elf -ex "tar remote :2331" -ex "mon reset"


# building and debugging the app

.PHONY: prog app

app: $(APP)

APP_OBJS := $(addprefix $(BUILD_DIR)/, $(patsubst %.c, %.o, $(patsubst %.s, %.o, $(APP_SRCS))))
$(BUILD_DIR)/app.elf: $(APP_OBJS)

prog: app SoftDevice $(BUILD_DIR)/prog.jlink
	$(JLINK_COMMANDS)

# building and debugging the bootloader

.PHONY: bl blprog

bl: $(BOOTLOADER)

BOOTLOADER_OBJS = $(patsubst %, $(BUILD_DIR)/%, $(patsubst %.c, %.o, $(patsubst %.s, %.o, $(BOOTLOADER_SRCS))))
$(BUILD_DIR)/bootloader.elf: $(BOOTLOADER_OBJS)

blprog: bl SoftDevice $(BUILD_DIR)/blprog.jlink
	$(JLINK_COMMANDS)

# SoftDevice

.PHONY: SoftDevice
SoftDevice: $(SOFTDEVICE_BINARIES)

$(BUILD_DIR)/softdevice_main.bin: $(SOFTDEV_SRC)
	$(info [OBJCOPY] softdevice_main.bin)
	@$(OBJCOPY) -I ihex -O binary --remove-section .sec3 $< $@

$(BUILD_DIR)/softdevice_uicr.bin: $(SOFTDEV_SRC)
	$(info [OBJCOPY] softdevice_uicr.bin)
	@$(OBJCOPY) -I ihex -O binary --only-section .sec3 $< $@

# building the entire firmware

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(info [CC] $(notdir $<))
	@$(CC) $(CFLAGS) -MF $(basename $@).d -c $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(info [AS] $(notdir $<))
	@$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.elf: startup/%.ld | $(dir $@)
	$(info [LD] $@)
	@$(LD) $(LDFLAGS) -o $@ $(filter %.o, $^) -T$< $(LDFLAGS)

%.bin: %.elf
	$(info [BIN] $@)
	@$(OBJCOPY) -O binary $< $@
#-j .text -j .data

.PHONY: clean
clean:
	-rm -rf $(BUILD_DIR)

# dependency info
-include $(ALL_DEPS)
