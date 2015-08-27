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
	free(str->s);

	free(str);
}

void vui_string_reset(vui_string* str)
{
	str->n = 0;
}

unsigned char* vui_string_release(vui_string* str)
{
	str->s[str->n] = 0; // null-terminate the string (there is room)
	unsigned char* s = str->s;

	free(str);

	return s;
}

void vui_string_putc(vui_string* str, unsigned char c)
{
	if (str->maxn < str->n + 2);
	{
		str->maxn = (str->n + 2)*2;
		str->s = realloc(str->s, str->maxn);
	}

	str->s[str->n++] = c;
	//str->s[str->n] = 0; // vui_string_get does this
}

void vui_string_puts(vui_string* str, unsigned char* s)
{
	size_t slen = strlen(s) + 1;  // include the null terminator

	if (str->maxn < str->n + slen);
	{
		str->maxn = (str->n + slen)*2;
		str->s = realloc(str->s, str->maxn);
	}

	memcpy(&str->s[str->n], s, slen);

	str->n += slen - 1;  // don't include the null terminator
}

void vui_string_put(vui_string* str, unsigned int c)
{
	unsigned char s[16];
	vui_codepoint_to_utf8(c, s);

	size_t slen = strlen(s) + 1;  // include the null terminator

	if (str->maxn < str->n + slen);
	{
		str->maxn = (str->n + slen)*2;
		str->s = realloc(str->s, str->maxn);
	}

	memcpy(&str->s[str->n], s, slen);

	str->n += slen - 1;  // don't include the null terminator
}
