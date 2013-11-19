# tool locations
BIN=~/Downloads/gcc-arm-none-eabi-4_7-2013q3/bin
#BIN=~/Downloads/gcc-arm-none-eabi-4_7-2012q4/bin
JLINK_BIN=~/Work/jlink_462b

# enable debug mode?
DEBUG = 1

# target to build
APP = app.bin
BOOTLOADER = bootloader.bin
SOFTDEVICE_BINARIES = SoftDevice/softdevice_uicr.bin SoftDevice/softdevice_main.bin

# nRF51822 revision (for PAN workarounds in nRF SDK)
NRFREV = NRF51822_QFAA_CA

# don't mess with teh NULL!
NULL =

#	nRF51_SDK/nrf51822/Source/ble/ble_services/ble_bas.c \
#	$(wildcard micro-ecc/*.c) \
#	nRF51_SDK/nrf51822/Source/app_common/crc16.c \

SRCS = \
	error_handler.c \
	sha1.c \
	spi.c \
	util.c \
	watchdog.c \
	$(wildcard ble/*.c) \
	$(wildcard ble/services/*.c) \
	$(wildcard micro-ecc/*.c) \
	./gcc_startup_nrf51.s \
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
	$(NULL)

APP_SRCS = \
	$(SRCS) \
	hrs.c \
	mpu-6500.c \
	pwm.c \
	main.c \

BOOTLOADER_SRCS = \
	$(SRCS) \
	$(wildcard hello_bootloader/*.c) \
	hello_bootloader/ble/ble_services/ble_dfu.c \

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
	$(NULL)
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
CFLAGS := -fdata-sections -ffunction-sections -MMD $(ARCHFLAGS) $(addprefix -I,$(INCS)) $(MICROECCFLAGS) $(NRFFLAGS) $(OPTFLAGS) $(WARNFLAGS)
LDFLAGS := -L$(LIB) -lgcc --gc-sections

ifneq ($(BLE_DEVICE_NAME),)
CFLAGS += -DBLE_DEVICE_NAME="\"$(BLE_DEVICE_NAME)\""
endif

#-L/Users/jkelley/Downloads/gcc-arm-none-eabi-4_7-2013q1/lib/gcc/arm-none-eabi/4.7.3/armv6-m -lgcc
#-T./nRF51_SDK/nrf51822/Source/templates/gcc/gcc_nrf51_s110_xxaa.ld -I./nRF51_SDK/nrf51822/Source/templates/gcc/

ALL_SRCS = $(SRCS) $(APP_SRCS) $(BOOTLOADER_SRCS)
ALL_OBJS = $(patsubst %.c, %.o, $(patsubst %.s, %.o, $(ALL_SRCS)))
ALL_DEPS = $(ALL_OBJS:.o=.d)

# all target (must be first rule): build app + bootloader

.PHONY: all jl
all: $(APP) $(BOOTLOADER) SoftDevice

jl: all jl.jlink app.sha1
	$(JLINK_COMMANDS)

# jlink invocation

%.jlink: %.jlink.in
	$(info [JLINK_SCRIPT] $@)
	sed -e 's,$$PWD,$(PWD),g' < $< > $@

JLINK_COMMANDS = \
	$(info [JPROG] $@.jlink) \
	@$(JPROG) < $@.jlink

jlink:
	@$(JPROG)

# sha1 invocation

%.sha1: %.bin
	$(info [SHA1] $@)
	@openssl sha1 -binary $< > $@
	@stat -f "%Xz" $< | xxd -r -p | dd conv=swab 2> /dev/null >> $@

# gdb

.PHONY: gdbs gdb cgdb

gdbs:
	$(JGDBServer) -if SWD -device nRF51822 -speed 4000

gdb:
	$(BIN)/arm-none-eabi-gdb app.elf -ex "tar remote :2331" -ex "mon reset"

blgdb:
	$(BIN)/arm-none-eabi-gdb bootloader.elf -ex "tar remote :2331" -ex "mon reset"

cgdb:
	cgdb -d $(BIN)/arm-none-eabi-gdb -- app.elf -ex "tar remote :2331" -ex "mon reset"


# building and debugging the app

.PHONY: prog app

app: $(APP)

APP_OBJS := $(patsubst %.c, %.o, $(patsubst %.s, %.o, $(APP_SRCS)))
app.elf: $(APP_OBJS)

prog: app SoftDevice prog.jlink
	$(JLINK_COMMANDS)

# building and debugging the bootloader

.PHONY: bl blprog

bl: $(BOOTLOADER)

BOOTLOADER_OBJS = $(patsubst %.c, %.o, $(patsubst %.s, %.o, $(BOOTLOADER_SRCS)))
bootloader.elf: $(BOOTLOADER_OBJS)

blprog: bl SoftDevice blprog.jlink
	$(JLINK_COMMANDS)

# SoftDevice

.PHONY: SoftDevice
SoftDevice: $(SOFTDEVICE_BINARIES)

SoftDevice/softdevice_main.bin: $(SOFTDEV_SRC)
	$(info [OBJCOPY] softdevice_main.bin)
	@$(OBJCOPY) -I ihex -O binary --remove-section .sec3 $< $@

SoftDevice/softdevice_uicr.bin: $(SOFTDEV_SRC)
	$(info [OBJCOPY] softdevice_uicr.bin)
	@$(OBJCOPY) -I ihex -O binary --only-section .sec3 $< $@

# building the entire firmware

%.o: %.c
	$(info [CC] $(notdir $<))
	@$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(info [AS] $(notdir $<))
	@$(AS) $(ASFLAGS) $< -o $@

%.elf: %.ld
	$(info [LD] $@)
	@$(LD) -o $@ $(filter %.o, $^) -T$(@:.elf=.ld) $(LDFLAGS)

%.bin: %.elf
	$(info [BIN] $@)
	@$(OBJCOPY)  -O binary $< $@
#-j .text -j .data

ALL_PRODUCTS = $(ALL_OBJS) $(APP) $(APP:.bin=.elf) $(BOOTLOADER) $(BOOTLOADER:.bin=.elf) $(ALL_DEPS) $(SOFTDEVICE_BINARIES) all.jlink app.jlink bootloader.jlink app.sha1

.PHONY: clean
clean:
	@-rm -f $(ALL_PRODUCTS)

# dependency info
-include $(ALL_DEPS)
