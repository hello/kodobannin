#include <stdlib.h>
#include <stdint.h>

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
	if (n == 0)
		return 0;

	while (n-- >0)
		if (s1++ != s2++)
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

size_t
strlen(const char *a)
{
	size_t count = 0;

	while (*a++)
		++count;

	return count;
}
