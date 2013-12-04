// vi:sw=4:ts=4

#pragma once

#define APP_ASSERT(condition) APP_ERROR_CHECK(!(condition))

#ifdef DEBUG_SERIAL //=====================================
#include <simple_uart.h>
#define PRINT_HEX(a,b) serial_print_hex((uint8_t *)a,b)
#define PRINTS(a) simple_uart_putstring((const uint8_t *)a)
#define PRINTC(a) simple_uart_put(a)
#else //---------------------------------------------------
#define PRINT_HEX(a,b) {}
#define PRINTS(a) {}
#define PRINTC(a) {}
#define simple_uart_config(a,b,c,d,e) {}
#endif //===================================================

#define DEBUG(a,b) {PRINTS(a); PRINT_HEX(&b, sizeof(b)); PRINTC('\r'); PRINTC('\n');}

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define TRACE(...) do { PRINTS(__FILE__ ":" STR(__LINE__) " ("); PRINTS(__func__); PRINTS(")\r\n"); } while (0)

void serial_print_hex(uint8_t *ptr, uint32_t len);
void binary_to_hex(uint8_t *ptr, uint32_t len, uint8_t* out);

union int16_bits {
	int16_t value;
	uint8_t bytes[sizeof(int16_t)];
};

union uint16_bits {
	uint16_t value;
	uint8_t bytes[sizeof(uint16_t)];
};

union int32_bits {
	int32_t value;
	uint8_t bytes[sizeof(int32_t)];
};

union uint32_bits {
	uint32_t value;
	uint8_t bytes[sizeof(uint32_t)];
};

union generic_pointer {
	uint8_t* p8;
	uint16_t* p16;
	uint32_t* p32;

	int8_t* pi8;
	int16_t* pi16;
	int32_t* pi32;
};

static inline uint16_t __attribute__((const)) bswap16(uint16_t x)
{
	__asm__ ("rev16 %0, %1" : "=r" (x) : "r" (x));

	return x;
}

static inline uint32_t __attribute__((const)) bswap32(uint32_t x)
{
	__asm__ ("rev %0, %1" : "=r" (x) : "r" (x));

	return x;
}
