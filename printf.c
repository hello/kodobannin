// vi:sw=4:ts=4

#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "util.h"
//#include <simple_uart.h>

#define simple_uart_put(x) putc(x, stdout)
#undef snprintf

const uint8_t hex[] = "0123456789ABCDEF";

static inline void
_putc(char out) {
	simple_uart_put(out);
}

static inline uint32_t
_char_out(char *dest, size_t index, size_t len, char out)
{
	if (dest) {
		if (index < len) {
			dest[index++] = out;
		} else {
			return 0;
		}
	} else {
		_putc(out);
	}

	return 1;
}

static inline uint32_t
_hexify(char *dest, size_t index, size_t len, va_list args)
{
	uint32_t val;
	char tmp;
	uint32_t i = 0;
	uint32_t used = 0;

	val = (uint32_t) va_arg(args, uint32_t);
	
	for (i=0; i < 4; i++) {
		uint8_t out = (val >> 8*(3-i)) & 0xFF;
		tmp = hex[0xF & (out >> 4)];
		if (!_char_out(dest, index, len, tmp))
			goto out;
		++used;
		tmp = hex[0xF & out];
		if (!_char_out(dest, index, len, tmp))
			goto out;
		++used;
	}
out:
	return used;
}

static inline uint32_t
_intify(char *dest, size_t index, size_t len, va_list args)
{
	return 0;
}

static uint32_t
_vsnprintf(char *dest, size_t len, const char *fmt, va_list args)
{
	char tok;
	uint32_t index = 0;
	uint32_t used;

	if (!fmt || (dest && !len))
		return 0;

	do {
		tok = *fmt++;
		switch (tok) {
			case '%':
				switch (fmt[0]) {
					case 'd':
					case 'D':
						used = _intify(dest, index, len, args);
						if (!used)
							return index;
						index += used-1;
						++fmt;
						break;

					case 'x':
					case 'X':
						used = _hexify(dest, index, len, args);
						if (!used)
							return index;
						index += used-1;
						++fmt;
						break;

					default:
						if (!_char_out(dest, index, len, '%'))
							return index;

				};
				break;

			case '\0':
				return index;
				break;

			default:
				if(!_char_out(dest, index, len, tok))
					return index;
		};

		index++;
	} while (*fmt && (!len || index < len));

	return index;
}

__printflike(1, 2) int
printf(const char *fmt, ...)
{
	va_list args;
	uint32_t ret;

	va_start(args, fmt);

	ret = _vsnprintf(NULL, 0, fmt, args);

	va_end(args);

	return ret;	
}

__printflike(3, 4) uint32_t
_snprintf(char *dest, size_t len, const char *fmt, ...)
{
	va_list args;
	uint32_t ret;

	va_start(args, fmt);

	ret = _vsnprintf(dest, len, fmt, args);

	va_end(args);

	return ret;		
}
