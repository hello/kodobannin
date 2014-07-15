/**
 * UART to message wrapper for app_uart module
 */
#ifndef MESSAGE_UART
#define MESSAGE_UART

#include <app_uart.h>
#include "message_base.h"


#define MSG_UART_COMMAND_MAX_SIZE 32

MSG_Base_t * MSG_Uart_Base(const app_uart_comm_params_t * params, const MSG_Central_t * parent);
//quick print function for debug
void MSG_Uart_Prints(const char * str);
void MSG_Uart_PrintHex(const uint8_t * ptr, uint32_t len);


#endif
