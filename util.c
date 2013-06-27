#include <stdlib.h>
#include <stdint.h>
/*
void *
memcpy(void *restrict s1, const void *restrict s2, size_t n)
{
	if (!s1 || !s2)
		return NULL;

	void *orig = s1;

	for (size_t i = 0; i < n; ++i, ++s1, ++s2)
		*(char *)s1 = *(char *)s2;

	return orig;
}
*/
void *
memset(void *b, int c, size_t len)
{
	void *orig = b;
	char val = (char)c;

	while (len-- > 0)
		*(char *)b++ = val;

	return orig;
}
