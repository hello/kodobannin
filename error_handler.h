// vi:sw=4 ts=4

#pragma once

#include <stdint.h>

#include "util.h"

// An index value into a unsigned long array of stacked registers, which are generated on an ARM exception (such as a hard fault).
/** See  <http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0497a/Babefdjc.html> for more information. */
enum stacked_register_index {
    STACKED_R0 = 0,
    STACKED_R1 = 1,
    STACKED_R2 = 2,
    STACKED_R3 = 3,
    STACKED_R12 = 4,
    STACKED_LR = 5,
    STACKED_PC = 6,
    STACKED_XPSR = 7,
};

struct crash_log_hardfault {
	uint32_t stacked_registers[8];
} PACKED;

struct crash_log {
	uint32_t signature;
	union {
		struct crash_log_hardfault hardfault;
	} PACKED;
    uint16_t stack_size;
    uint8_t stack[];
} PACKED;

void crash_log_save();
