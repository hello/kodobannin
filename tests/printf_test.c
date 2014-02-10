// clang -c printf.c -g -fno-builtin && clang -g printf_test.c printf.o && ./a.out
#include <stdint.h>
#include <stdio.h>

extern int printf(const char *, ...);

int main() {
	uint32_t i = 0xDEADBEEF;
	uint16_t k = 0xFFFF;
	uint32_t j = 257;
	printf("Hello there\n");
	printf("%");
	printf("\n");
	printf("%%\n");
	printf("%d\n", j);
	j = 0;
	printf("%D\n", j);
	printf("%x\n", i);
	printf("%x\n", k);
	i = 0xBEEF;
	printf("%d\n", i);
	printf("<%x> <%X>\n", i, k);
	printf("%n %f\n");

	//snprintf tests
	uint8_t buf[5];
	uint32_t len;
	len = _snprintf(buf, sizeof(buf), "%x\n", i);
	printf("%d: '%s'\n", len, buf);
}
