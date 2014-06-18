
/*
 * Serial Port User Interface (command line interpreter)
 */

#include <stdint.h>
#include <stdio.h>

#include <nrf_delay.h>
#include <nrf_gpio.h>
#include <nrf_sdm.h>
#include <softdevice_handler.h>

#include "util.h"
#include "platform.h"

// #include <spi_master.h>
// #include "imu.h"

#include <twi_master.h>
#include "rtc.h"

#include "uart.h"
#include "ucli.h"

struct rtc_time_t time;

enum {init,idle,exec};

static struct
{
    uint8_t state;
    uint8_t line[16];
    uint8_t index;
} ucli;

static struct
{
    uint8_t index;
    uint8_t line[16];
} uclr;

void ucliInit(void)
{
    uint8_t i;

 // spiInit();
 // imuInit();

 // twiInit();
 // rtcInit();

 // uartInit(); // 115,200 baud rate

    ucli.index = 0;
    for (i=0;i<16;i++) ucli.line[i] = '\0';

    uclr.index = 0;
    for (i=0;i<16;i++) uclr.line[i] = '\0';

    ucli.state = init;
    ucliExec(); // display banner

 // NRF_UART0->INTENSET = UART_INTENSET_RXDRDY_Enabled << UART_INTENSET_RXDRDY_Pos;
 // NVIC_SetPriority(UART0_IRQn, APP_IRQ_PRIORITY_LOW);
 // NVIC_EnableIRQ(UART0_IRQn);
}

uint8_t ucliNibble(uint8_t chr)
{
    if (chr > '9')
        chr -= 'a' - 10;
    else
        chr -= '0';
    return (uint8_t) chr;
}

uint8_t ucliByte(uint8_t *str, int8_t len)
{
    uint8_t res;

    res = 0;
    if (len > 0)
    {
        res = ucliNibble(str[0]);
        if (len > 1)
        {
            res = (res << 4) | ucliNibble(str[1]);
        }
    }

    return res;
}

uint16_t ucliWord(uint8_t *str, uint8_t len)
{
    uint16_t res;

    res = 0;
    if (len)
    {
        res = ucliNibble(str[0]);
        if (len > 1)
            res = (res << 4) | ucliNibble(str[1]);
        if (len > 2)
            res = (res << 4) | ucliNibble(str[2]);
        if (len > 3)
            res = (res << 4) | ucliNibble(str[3]);
    }

    return res;
}

void ucliNext(char chr)
{
    uint8_t i;

    if (ucli.index == sizeof(ucli.line) - 1)
    {
        uartPutString("\ntoo long");
        ucli.index = 0;
        ucliExec(); // force new prompt
    }
    else
    switch (chr) //
    {
    case '\t': // tab
        chr = ' ';
        break;

    case '\r':
    case '\n': // exec line so far
        uartPutChar('\n');
        ucliExec(); // in line for now
        break;

    case '\b':
    case '\177': // backspace
        if (ucli.index)
        {
            uartPutChar('\b');
            uartPutChar(' ');
            uartPutChar('\b');
            ucli.index--;
        }
        break;

    case ' ':
        ucli.line[ucli.index++] = chr;
        uartPutChar(chr);
        break;

    case '\e': // recall previous line
        ucli.index = uclr.index;
        for (i=0;i<uclr.index;i++) ucli.line[i] = uclr.line[i];
        uartPutString("\n>");
        for (i=0;i<ucli.index;i++) uartPutChar(ucli.line[i]);
        break;
    default:
        if (chr >= ' ')
        {
            ucli.line[ucli.index++] = chr;
            uartPutChar(chr);
        }
        break;
    }
}

void ucliExec(void)
{
    uint16_t r16,d16;
    uint8_t i;

    switch (ucli.state)
    {
    case init:

        /* power up debug banner goes here */
        uartPutString("\n\n===( CLI )===\n\n>");

        ucli.state = idle;
        break;

    case idle:

        // lookup cmd in map or fall thru

    case exec:
        if (ucli.index)
        {
            uclr.index = ucli.index; // save in recall buffer
            for (i=0;i<ucli.index;i++) uclr.line[i] = ucli.line[i];

            /* command parser goes here */

            switch (ucli.line[0]) // leading char
            {
            case 's': // enter sleep
                uartPutLine("power off being entered");
             // APP_OK(sd_power_system_off());
                break;

            case 'i': // inertial_motion_detector
                uartPutLine("<imu>");
                break;

            case 't': // rtc
                uartPutString("<rtc> ");
                rtc_read(&time);

             // PRINT_HEX(time.bytes, sizeof(time.bytes));

                uartPutByte(time.bytes[0]);uartPutChar(' ');

                for (i=1;i<8;i++)
                {
                    uartPutChar(' ');
                    uartPutByte(time.bytes[i]);
                }

                uartPutString(" | ");

                printf("%d/%d/%d%d %d:%d:%d.%d%d\r\n",
                rtc_bcd_decode(time.month),
                rtc_bcd_decode(time.date),
                            20+time.century,
                rtc_bcd_decode(time.year),
                rtc_bcd_decode(time.hours),
                rtc_bcd_decode(time.minutes),
                rtc_bcd_decode(time.seconds),
                rtc_bcd_decode(time.tenths),
                rtc_bcd_decode(time.hundredths));

                nrf_delay_ms(100);
                break;

            case 'v':
                uartPutLine("<code_version>");
                break;
            }
        }
        uartPutChar('>');
        ucli.index = 0;
        ucli.state = idle;
        break;
    }
}

