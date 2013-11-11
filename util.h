#pragma once

#ifdef DEBUG_SERIAL //=====================================
#include <simple_uart.h>
#define PRINT_HEX(a,b) serial_print_hex((uint8_t *)a,b)
#define PRINTS(a) simple_uart_putstring((const uint8_t *)a)
#define PRINTC(a) simple_uart_put(a)
#else //---------------------------------------------------
#define PRINT_HEX(a,b) {}
#define PRINTS(a) {}
#define PRINTC(a) {}
#endif //===================================================

#define DEBUG(a,b) {PRINTS(a); PRINT_HEX(&b, sizeof(b)); PRINTC('\r'); PRINTC('\n');}

void serial_print_hex(uint8_t *ptr, uint32_t len);
void binary_to_hex(uint8_t *ptr, uint32_t len, uint8_t* out);
