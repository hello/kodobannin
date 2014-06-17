#pragma once

/*
 * Serial Port (uintart command line interpreter)
 */

void uartInit(void);

uint8_t uartKeyPress(void);
uint8_t uartGetChar(void);

void uartPutChar(char chr);

void uartPutString(char *str);
void uartPutLine(char *str);

void uartPutNibble(uint8_t d);

void uartPutDec(uint8_t d);

void uartPutByte(uint8_t d);
void uartPutWord(uint16_t d);
void uartPutLong(uint32_t d);

