/**
 * UART to message wrapper for app_uart module
 */
#ifndef MESSAGE_UART
#define MESSAGE_UART

#include <app_uart.h>
#include "message_base.h"


MSG_Base_t * MSG_Uart_Init(const app_uart_comm_params_t * params, MSG_Base_t * central);
//quick print function for debug
void MSG_Uart_Prints(const char * str);


#endif
