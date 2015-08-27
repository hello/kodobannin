/**
 * UART to message wrapper for app_uart module
 */
#ifndef MESSAGE_UART
#define MESSAGE_UART

#include <app_uart.h>
#include "message_base.h"

typedef enum{
    MSG_UART_DEFAULT = 0,
    MSG_UART_HEX,
    MSG_UART_SLIP,
    MSG_UART_STRING,
}MSG_Uart_Ports;

#define MSG_UART_COMMAND_MAX_SIZE 32

MSG_Base_t * MSG_Uart_Base(const app_uart_comm_params_t * params, const MSG_Central_t * parent);

//function for async print
void MSG_Uart_Prints(const char * str);
void MSG_Uart_Printc(char c);
void MSG_Uart_PrintDec(const int * ptr, uint32_t len);
void MSG_Uart_PrintHex(const uint8_t * ptr, uint32_t len);
void MSG_Uart_PrintByte(const uint8_t * ptr, uint32_t len);

void MSG_Uart_Printf(char * fmt, ... );

uint8_t MSG_Uart_GetLastChar(void);


#endif
