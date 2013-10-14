#pragma once

#ifdef DEBUG_SERIAL
#define PRINT_HEX(a,b) serial_print_hex((uint8_t *)a,b)
#define PRINTS(a) simple_uart_putstring((const uint8_t *)a)
#define PRINTC(a) simple_uart_put(a)
#else
#define PRINT_HEX(a,b) {}
#define PRINTS(a) {}
#define PRINTC(a) {}
#endif

void serial_print_hex(uint8_t *ptr, uint32_t len);
