TARGET = bootloader.bin

SRCS = $(wildcard *.c) \
	$(wildcard micro-ecc/*.c) \
	nRF51_SDK/nrf51822/Source/templates/gcc/gcc_startup_nrf51.s \
	nRF51_SDK/nrf51822/Source/templates/system_nrf51.c \
	nRF51_SDK/nrf51822/Source/nrf_delay/nrf_delay.c 
#	nRF51_SDK/nrf51822/Source/app_common/app_timer.c 

OBJS = $(patsubst %.c, %.o, $(patsubst %.s, %.o, $(SRCS)))
DEPS = $(OBJS:.o=.d)

# tool location
BIN=~/Downloads/gcc-arm-none-eabi-4_7-2013q1/bin
PREFIX=arm-none-eabi-

# nRF51822 revision (for PAN workarounds in nRF SDK)
NRFREV = NRF51822_QFAA_CA

# micro-ecc config - see ecc.h for details
MICROECCFLAGS = -DECC_ASM=1 -DECC_CURVE=6

# optimization flags
OPTFLAGS =-Os -g

# compiler warnings
WARNFLAGS =-Wall

# keep intermediary files such as .elf (for symbols)
.SECONDARY:

CC=$(BIN)/$(PREFIX)gcc
LD=$(BIN)/$(PREFIX)ld
AS=$(BIN)/$(PREFIX)as
OBJCOPY=$(BIN)/$(PREFIX)objcopy
STRIP=$(BIN)/$(PREFIX)strip

NRFFLAGS = -DBOARD_PCA10001 -DNRF51 -DDO_NOT_USE_DEPRECATED -D$(NRFREV)
ASFLAGS=-mcpu=cortex-m0  -mthumb -march=armv6-m
CFLAGS := -MMD -mcpu=cortex-m0  -mthumb -march=armv6-m -I./micro-ecc -I./nRF51_SDK/nrf51822/Include -I./nRF51_SDK/nrf51822/Include/gcc -I./nRF51_SDK/nrf51822/Include/app_common -I./nRF51_SDK/nrf51822/Include/ble/softdevice $(MICROECCFLAGS) $(NRFFLAGS) $(OPTFLAGS)
LDFLAGS := -T./link.ld -Map code.map
#-T./nRF51_SDK/nrf51822/Source/templates/gcc/gcc_nrf51_s110_xxaa.ld -I./nRF51_SDK/nrf51822/Source/templates/gcc/

all: $(TARGET)

%.o: %.c
	$(info [CC] $(notdir $<))
	@$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(info [AS] $(notdir $<))
	@$(AS) $(ASFLAGS) $< -o $@

%.elf: $(OBJS) link.ld
	$(info [LD] $@)
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)

%.bin: %.elf
	$(info [BIN] $@)
	@$(OBJCOPY)  -O binary $< $@
#-j .text -j .data

.PHONY: clean
clean:
	-@rm -f $(OBJS) $(TARGET) $(TARGET:.bin=.elf) $(DEPS)

# dependency info
-include $(DEPS)
