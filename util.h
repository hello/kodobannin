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
#define simple_uart_config(a,b,c,d,e) {}
#endif //===================================================

#define DEBUG(a,b) {PRINTS(a); PRINT_HEX(&b, sizeof(b)); PRINTC('\r'); PRINTC('\n');}

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define TRACE(...) do { PRINTS(__FILE__ ":" STR(__LINE__) " ("); PRINTS(__func__); PRINTS(")\r\n"); } while (0)

void serial_print_hex(uint8_t *ptr, uint32_t len);
void binary_to_hex(uint8_t *ptr, uint32_t len, uint8_t* out);
