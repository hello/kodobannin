// vi:noet:sw=4 ts=4

#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "util.h"
//#include <simple_uart.h>

#ifndef __printflike
/* 
 * Compiler-dependent macros to declare that functions take printf-like 
 * or scanf-like arguments.  They are null except for versions of gcc
 * that are known to support the features properly (old versions of gcc-2   
 * didn't permit keeping the keywords out of the application namespace). 
 */
#    define __printflike(fmtarg, firstvararg)       \
            __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#    define __scanflike(fmtarg, firstvararg)        \
            __attribute__((__format__ (__scanf__, fmtarg, firstvararg)))
#    define __format_arg(fmtarg)    __attribute__((__format_arg__ (fmtarg)))
#else
// Bad John, hiding crap in here for the test harness. For shame!
const uint8_t hex[] = "0123456789ABCDEF";
#define simple_uart_put(x) putc(x, stdout)
#undef snprintf

#endif


static inline void
_putc(char out) {
	simple_uart_put(out);
}

static inline uint32_t
_char_out(char *dest, size_t index, size_t len, char out)
{
	if (dest) {
		if (index < len) {
			dest[index] = out;
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
	uint8_t non_zero = 0;

	val = (uint32_t) va_arg(args, uint32_t);
	
	for (i=0; i < 4; i++) {
		uint8_t out = (val >> 8*(3-i)) & 0xFF;
		tmp = hex[0xF & (out >> 4)];
		if (tmp!=hex[0] || (tmp == hex[0] && non_zero)) {
			non_zero = 1;

			if (!_char_out(dest, index, len, tmp)) {
				goto out;
			}
			++used;
			++index;
		}
		tmp = hex[0xF & out];
		if (tmp!=hex[0] || (tmp == hex[0] && non_zero)) {
			non_zero = 1;
			if (!_char_out(dest, index, len, tmp)) {
				goto out;
			}
			++used;
			++index;
		}
	}
out:
	return used;
}

static inline uint32_t
_intify(char *dest, size_t index, size_t len, va_list args)
{
	uint32_t val;
	uint32_t val_p;
	uint8_t rem;
	uint32_t used = 0;
	uint32_t output = 0;
	uint8_t buf[11]; // INT32_MAX needs 10 digits

	val = (uint32_t) va_arg(args, uint32_t);

	do {
		val_p = val/10;

		rem = val - (val_p*10);
				
		buf[used++] = hex[rem];

		val = val_p;
	} while (val != 0);

	// print buffer backwards
	do {
		rem = _char_out(dest, index++, len, buf[--used]);
	} while (rem && used >0 && ++output);

	return output;
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
							goto out;
						index += used;
						++fmt;
						break;

					case 'x':
					case 'X':
						used = _hexify(dest, index, len, args);
						if (!used)
							goto out;
						index += used;
						++fmt;
						break;

					case 's':
					case 'S':
					{
						uint8_t *arg = (uint8_t *)va_arg(args, uint8_t *);
					
						while (*arg && (!dest || (index < len))) {
							if (!_char_out(dest, index, len, *arg++))
								goto out;
							++index;
						}
						++fmt;
						break;
					}

					default:
						if (!_char_out(dest, index, len, '%'))
							goto out;

				};
				break;

			case '\0':
				goto out;
				break;

			default:
				if(!_char_out(dest, index, len, tok))
					goto out;
				index++;
		};
	} while (*fmt && (!dest || index < len));

out:
	if (dest) {
		dest[index-1] = '\0';
	}

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
