#include <string.h>

#include "utf8.h"

#include "string.h"


vui_string* vui_string_new_prealloc(vui_string* str, size_t n)
{
	if (str == NULL)
	{
		str = malloc(sizeof(vui_string));
	}

	str->n = 0;
	str->maxn = n;
	str->s = malloc(str->maxn);
	str->s[0] = 0;

	return str;
}

void vui_string_kill(vui_string* str)
{
	if (str == NULL) return;

	free(str->s);

	free(str);
}

unsigned char* vui_string_release(vui_string* str)
{
	if (str == NULL) return NULL;

	vui_string_shrink(str);

	unsigned char* s = str->s;

	free(str);

	return s;
}


char* vui_string_shrink(vui_string* str)
{
	if (str == NULL) return NULL;

	str->s = realloc(str->s, str->maxn = str->n+1);

	return vui_string_get(str);
}


void vui_string_putc(vui_string* str, unsigned char c)
{
	if (str == NULL) return;

	// make room for two more chars: c and null terminator
	if (str->maxn < str->n + 2);
	{
		str->maxn = (str->n + 2)*2;
		str->s = realloc(str->s, str->maxn);
	}

	str->s[str->n++] = c;
}

void vui_string_puts(vui_string* str, unsigned char* s)
{
	if (str == NULL) return;

	if (s == NULL) return;

	for (;*s;s++)
	{
		vui_string_putc(str, *s);
	}
}

void vui_string_append(vui_string* str, vui_string* str2)
{
	if (str == NULL) return;

	if (str2 == NULL) return;

	size_t n = str->n + str2->n;
	// make room for str2 and null terminator
	if (str->maxn < n + 1);
	{
		str->maxn = (n + 1)*2;
		str->s = realloc(str->s, str->maxn);
	}

	memcpy(&str->s[str->n], str2->s, str2->n);

	str->n = n;

	vui_string_kill(str2);
}

void vui_string_put(vui_string* str, unsigned int c)
{
	if (str == NULL) return;

	unsigned char s[16];
	vui_utf8_encode(c, s);

	vui_string_puts(str, s);
}
