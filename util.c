// vi:sw=4:ts=4

#include <app_error.h>
#include <app_timer.h>
#include <stdlib.h>
#include <stdint.h>
#include <simple_uart.h>

#include "device_params.h"
#include "util.h"

void *
memcpy(void *s1, const void *s2, size_t n)
{
	size_t i;
	if (!s1 || !s2)
		return NULL;

	void *orig = s1;

	for (i = 0; i < n; ++i, ++s1, ++s2)
		*(char *)s1 = *(char *)s2;

	return orig;
}

int
memcmp(const void *s1, const void *s2, size_t n)
{
	const char* a = s1;
	const char* b = s2;

	if (n == 0)
		return 0;

	while (n-- >0)
		if(*a++ != *b++)
			return 1;

	return 0;
}

void *
memset(void *b, int c, size_t len)
{
	void *orig = b;
	char val = (char)c;

	while (len-- > 0)
		*(char *)b++ = val;

	return orig;
}

uint8_t
memsum(void *start, unsigned len)
{
	uint8_t *p = start;
	uint8_t i = 0;

	while(len-- > 0) {
		i += *p++;
	}

	return ~i;
}

size_t
strlen(const char *a)
{
	size_t count = 0;

	while (*a++)
		++count;

	return count;
}

#ifdef DEBUG_SERIAL
const uint8_t hex[] = "0123456789ABCDEF";

void
serial_print_hex(uint8_t *ptr, uint32_t len) {
	while(len-- >0) {
		simple_uart_put(hex[0xF&(*ptr>>4)]);
		simple_uart_put(hex[0xF&*ptr++]);
		simple_uart_put(' ');
	}
}

void
binary_to_hex(uint8_t *ptr, uint32_t len, uint8_t* out) {
	while(len-- >0) {
		*(out++) = hex[0xF&(*ptr>>4)];
		*(out++) = hex[0xF&*ptr++];
	}
}

void
debug_print_ticks(const char* const message, uint32_t start_ticks, uint32_t stop_ticks)
{
	uint32_t diff_ticks;

	uint32_t err = app_timer_cnt_diff_compute(stop_ticks, start_ticks, &diff_ticks);
	APP_ERROR_CHECK(err);

	DEBUG(message, diff_ticks);
}
#else
void
serial_print_hex(uint8_t *ptr, uint32_t len)
{
}

void
debug_print_ticks(const char* const message, uint32_t start_ticks, uint32_t stop_ticks)
{
}
#endif
