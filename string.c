#include <stdlib.h>
#include <string.h>

#include "utf8.h"

#include "string.h"


vui_string* vui_string_new(vui_string* str)
{
	if (str == NULL)
	{
		str = malloc(sizeof(vui_string));
	}

	str->n = 0;
	str->maxn = 16;
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

	unsigned char* s = vui_string_get(str);

	free(str);

	return s;
}


void vui_string_shrink(vui_string* str)
{
	if (str == NULL) return;

	str->s = realloc(str->s, str->maxn = str->n+1);
	str->s[str->n] = 0;
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

	for (;*s;s++)
	{
		vui_string_putc(str, *s);
	}
}

void vui_string_put(vui_string* str, unsigned int c)
{
	if (str == NULL) return;

	unsigned char s[16];
	vui_codepoint_to_utf8(c, s);

	vui_string_puts(str, s);
}
