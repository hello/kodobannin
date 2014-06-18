
/*
 * Serial Port (uart output)
 */

#include <stdint.h>

#include <nrf_delay.h>
#include <nrf_gpio.h>
#include <nrf_sdm.h>
#include <softdevice_handler.h>

#include "platform.h"

#include "nrf.h"
#include "util.h"

#include "uart.h"
#include "ucli.h"

void
UART0_IRQHandler()
{
    char chr; // = simple_uart_get();
 
    chr = NRF_UART0->RXD; // chr = uartGet();
    chr &= 0x7F; // mask high bit key code
    NRF_UART0->EVENTS_RXDRDY = 0;

    ucliNext(chr); // form command line by appending this char
}

#define BAUD_RATE UART_BAUDRATE_BAUDRATE_Baud115200
#define BAUD_POS UART_BAUDRATE_BAUDRATE_Pos

void uartInit(void)
{
    simple_uart_config(SERIAL_RTS_PIN, SERIAL_TX_PIN, SERIAL_CTS_PIN, SERIAL_RX_PIN, false);

    NRF_UART0->BAUDRATE = (BAUD_RATE << BAUD_POS);
    NRF_UART0->EVENTS_RXDRDY    = 0;

    NRF_UART0->INTENSET = UART_INTENSET_RXDRDY_Enabled << UART_INTENSET_RXDRDY_Pos;
    NVIC_SetPriority(UART0_IRQn, APP_IRQ_PRIORITY_LOW);
    NVIC_EnableIRQ(UART0_IRQn); // start accepting uart char's
}

uint8_t uartKeyPress(void)
{
    return (NRF_UART0->EVENTS_RXDRDY == 1);
}

/* bit uartKeyboard(void)
   {
       return (UCA0IFG & UCRXIFG) ? 1 : 0;
   } */

uint8_t uartGet(void)
{
    uint8_t chr;

    if (NRF_UART0->EVENTS_RXDRDY == 1)
    {
        chr = NRF_UART0->RXD;
        NRF_UART0->EVENTS_RXDRDY = 0;
    }

    return chr;
}

/* char uartGet(void)
   {
       char c;

       while (!uartKbHit())
          ;
       c = UCA0RXBUF;

       return c;
   } */

void uartPut(char chr)
{
    NRF_UART0->TXD = chr;

    while (NRF_UART0->EVENTS_TXDRDY != 1)
    {
      // Wait for this TXD data to be sent
    }
    NRF_UART0->EVENTS_TXDRDY = 0;
}

/* static void uartPutChar(char c)
   {
       while (!(UCA0IFG & UCTXIFG));
       UCA0TXBUF = c;
   } */

void uartPutChar(char chr)
{
    if (chr == '\n')
    {
        uartPut('\r');
    }
    uartPut(chr);
}

/* void uartPutChar(char c)
   {
       if (c == '\n')
           uartPut('\r');
       uartPut(c);
   } */

void uartPutString(char *str)
{
    uint_fast8_t i = 0;
    uint8_t chr = str[i++];

    while (chr != '\0')
    {
        uartPutChar(chr);
        chr = str[i++];
    }
}

/* void uartPutString(char *s)
   {
       while (*s) {
           uartPutChar(*s);
           s++;
       }
   } */

void uartPutLine(char *s)
{
    uartPutString(s);
    uartPutChar('\n');
}

void uartPutNibble(uint8_t n)
{
    n += '0';
    if (n > '9')
    {
        n += 'a' - '0' - 10;
    }

    uartPut(n);
}

void uartPutDec(uint8_t d)
{
    uint8_t i;

    i = 0;
    while (d > 99) // thousand's
    {
        i++;
        d -= 100;
    }

    if (i) uartPutNibble(i); // hundred's

    i = 0;
    while (d > 9)
    {
        i++;
        d -= 10;
    }

    uartPutNibble(i); // ten's
    uartPutNibble(d); // one's
}

void uartPutByte(uint8_t d)
{
    uartPutNibble(d >> 4);
    uartPutNibble(d & 0x0f);
}

void uartPutWord(uint16_t d)
{
    uartPutByte(d >> 8);
    uartPutByte(d & 0xff);
}

void uartPutLong(uint32_t d)
{
    uartPutWord(d >> 16);
    uartPutWord(d & 0xffff);
}

