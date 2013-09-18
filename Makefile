# tool location
BIN=~/Work/gcc-arm-none-eabi-4_7-2013q1/bin
#BIN=~/Downloads/gcc-arm-none-eabi-4_7-2013q1/bin
#BIN=~/Downloads/gcc-arm-none-eabi-4_7-2012q4/bin
JLINK_BIN=~/Work/jlink_462b

# target to build
TARGET = bootloader.bin

# nRF51822 revision (for PAN workarounds in nRF SDK)
NRFREV = NRF51822_QFAA_CA

SRCS =  $(wildcard *.c) \
	$(wildcard ble/*.c) \
	$(wildcard micro-ecc/*.c) \
	nRF51_SDK/nrf51822/Source/templates/gcc/gcc_startup_nrf51.s \
	nRF51_SDK/nrf51822/Source/templates/system_nrf51.c \
	nRF51_SDK/nrf51822/Source/app_common/app_timer.c \
	nRF51_SDK/nrf51822/Source/ble/ble_advdata.c \
	nRF51_SDK/nrf51822/Source/ble/ble_bondmngr.c \
	nRF51_SDK/nrf51822/Source/ble/ble_conn_params.c \
	nRF51_SDK/nrf51822/Source/ble/ble_flash.c \
	nRF51_SDK/nrf51822/Source/ble/ble_stack_handler.c \
	nRF51_SDK/nrf51822/Source/ble/ble_radio_notification.c \
	nRF51_SDK/nrf51822/Source/ble/ble_services/ble_bas.c \
	nRF51_SDK/nrf51822/Source/ble/ble_services/ble_dis.c \
	nRF51_SDK/nrf51822/Source/ble/ble_services/ble_srv_common.c \
	nRF51_SDK/nrf51822/Source/nrf_delay/nrf_delay.c \
	nRF51_SDK/nrf51822/Source/simple_uart/simple_uart.c

INCS =  ./ \
	./ble \
	./micro-ecc \
	./nRF51_SDK/nrf51822/Include \
	./nRF51_SDK/nrf51822/Include/app_common \
	./nRF51_SDK/nrf51822/Include/gcc \
	./nRF51_SDK/nrf51822/Include/ble \
	./nRF51_SDK/nrf51822/Include/ble/ble_services/ \
	./SoftDevice/s110_nrf51822_5.2.0_API/include

# optimization flags
OPTFLAGS=-Os -g

# compiler warnings
WARNFLAGS=-Wall
#-std=c99
#    -Wno-missing-braces -Wno-missing-field-initializers -Wformat=2 \
#    -Wswitch-default -Wswitch-enum -Wcast-align -Wpointer-arith \
#    -Wundef -Wcast-qual -Wshadow -Wunreachable-code \
#    -Wlogical-op -Wfloat-equal -Wstrict-aliasing=2 -Wredundant-decls \
#    -Werror

    #-Wbad-function-cast -Wstrict-overflow=5 \
#-Wall -pedantic -Wall -Wshadow -Wpointer-arith -Wcast-qual \
        -Wstrict-prototypes -Wmissing-prototypes

# micro-ecc config - see ecc.h for details
MICROECCFLAGS=-DECC_ASM=1 -DECC_CURVE=6

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
CFLAGS := -MMD $(ARCHFLAGS) $(addprefix -I,$(INCS)) $(MICROECCFLAGS) $(NRFFLAGS) $(OPTFLAGS) $(WARNFLAGS)
LDFLAGS := -T./link.ld -L$(LIB) -lgcc
#-L/Users/jkelley/Downloads/gcc-arm-none-eabi-4_7-2013q1/lib/gcc/arm-none-eabi/4.7.3/armv6-m -lgcc
#-T./nRF51_SDK/nrf51822/Source/templates/gcc/gcc_nrf51_s110_xxaa.ld -I./nRF51_SDK/nrf51822/Source/templates/gcc/

OBJS = $(patsubst %.c, %.o, $(patsubst %.s, %.o, $(SRCS)))
DEPS = $(OBJS:.o=.d)

all: $(TARGET)

prog: $(TARGET)
	$(JPROG) < prog.jlink
	$(JGDBServer) -if SWD -device nRF51822 -speed 4000

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

.PHONY: clean
clean:
	-@rm -f $(OBJS) $(TARGET) $(TARGET:.bin=.elf) $(DEPS)

# dependency info
-include $(DEPS)
