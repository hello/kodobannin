# tool location
BIN=~/Downloads/gcc-arm-none-eabi-4_7-2013q3/bin
#BIN=~/Downloads/gcc-arm-none-eabi-4_7-2012q4/bin
JLINK_BIN=~/Work/jlink_462b

# enable debug mode?
DEBUG = 1

# target to build
TARGET = demo.bin

# nRF51822 revision (for PAN workarounds in nRF SDK)
NRFREV = NRF51822_QFAA_CA

# don't mess with teh NULL!
NULL =

#	nRF51_SDK/nrf51822/Source/ble/ble_services/ble_bas.c \
#	$(wildcard micro-ecc/*.c) \
#	nRF51_SDK/nrf51822/Source/app_common/crc16.c \

SRCS =  $(wildcard *.c) \
	$(wildcard ble/*.c) \
	$(wildcard ble/services/*.c) \
	./gcc_startup_nrf51.s \
	nRF51_SDK/nrf51822/Source/templates/system_nrf51.c \
	nRF51_SDK/nrf51822/Source/app_common/app_timer.c \
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
LDFLAGS := -T./link.ld -L$(LIB) -lgcc --gc-sections
#-L/Users/jkelley/Downloads/gcc-arm-none-eabi-4_7-2013q1/lib/gcc/arm-none-eabi/4.7.3/armv6-m -lgcc
#-T./nRF51_SDK/nrf51822/Source/templates/gcc/gcc_nrf51_s110_xxaa.ld -I./nRF51_SDK/nrf51822/Source/templates/gcc/

OBJS = $(patsubst %.c, %.o, $(patsubst %.s, %.o, $(SRCS)))
DEPS = $(OBJS:.o=.d)

all: $(TARGET) SoftDevice

SoftDevice:
	$(info [OBJCOPY] softdevice_main.bin)
	@$(OBJCOPY) -I ihex -O binary --remove-section .sec3 $(SOFTDEV_SRC) SoftDevice/softdevice_main.bin
	$(info [OBJCOPY] softdevice_uicr.bin)
	@$(OBJCOPY) -I ihex -O binary --only-section .sec3 $(SOFTDEV_SRC) SoftDevice/softdevice_uicr.bin

prog: all
	$(JPROG) < prog.jlink
	$(JGDBServer) -if SWD -device nRF51822 -speed 4000

gdbs:
	$(JGDBServer) -if SWD -device nRF51822 -speed 4000

gdb:
	$(BIN)/arm-none-eabi-gdb demo.elf  -ex "tar remote :2331" -ex "mon reset"

cgdb:
	cgdb -d $(BIN)/arm-none-eabi-gdb -- demo.elf  -ex "tar remote :2331" -ex "mon reset"

%.o: %.c
	$(info [CC] $(notdir $<))
	@$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(info [AS] $(notdir $<))
	@$(AS) $(ASFLAGS) $< -o $@

%.elf: $(OBJS) link.ld
	$(info [LD] $@)
	@$(LD) -o $@ $(OBJS) $(LDFLAGS)

%.bin: %.elf
	$(info [BIN] $@)
	@$(OBJCOPY)  -O binary $< $@
#-j .text -j .data

.PHONY: clean SoftDevice
clean:
	@-rm -f $(OBJS) $(TARGET) $(TARGET:.bin=.elf) $(DEPS)

# dependency info
-include $(DEPS)
