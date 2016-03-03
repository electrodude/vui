#include <stdarg.h>
#include <string.h>

// for vsnprintf
#include <stdio.h>

#include "vui_utf8.h"

#include "vui_debug.h"

#include "vui_string.h"


vui_string* vui_string_new_prealloc_at(vui_string* str, size_t maxn)
{
	if (str == NULL)
	{
		str = malloc(sizeof(vui_string));
	}

	str->n = 0;
	str->maxn = maxn;
	str->s = malloc(str->maxn);
	str->s[0] = 0;

	return str;
}

vui_string* vui_string_new_array_at(vui_string* str, size_t n, const unsigned char* s)
{
	str = vui_string_new_prealloc_at(str, n);

	vui_string_putn(str, n, s);

	return str;
}

vui_string* vui_string_new_str_at(vui_string* str, const unsigned char* s)
{
	str = vui_string_new_at(str);

	vui_string_puts(str, s);

	return str;
}

vui_string* vui_string_dup_at(vui_string* str, const vui_string* orig)
{
	str = vui_string_new_prealloc_at(str, orig->maxn);

	memcpy(str->s, orig->s, orig->n);

	str->n = orig->n;

	return str;
}

void vui_string_kill(vui_string* str)
{
	if (str == NULL) return;

	free(str->s);

	free(str);
}

void vui_string_dtor(vui_string* str)
{
	if (str == NULL) return;

	str->n = str->maxn = 0;

	if (str->s == NULL) return;

	free(str->s);

	str->s = NULL;
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

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STRING)
	vui_debugf("shrink(\"%s\")\n", vui_string_get(str));
#endif

	str->maxn = str->n+1;

	str->s = realloc(str->s, str->maxn);

	return vui_string_get(str);
}


void vui_string_putc(vui_string* str, unsigned char c)
{
	if (str == NULL) return;

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STRING)
	vui_debugf("putc(\"%s\", '%c')\n", vui_string_get(str), c);
#endif

	// make room for two more chars: c and null terminator
	if (str->maxn < str->n + 2)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STRING)
		printf("realloc: %ld, %ld\n", str->maxn, str->n);
#endif
		str->maxn = (str->n + 2)*2;
		str->s = realloc(str->s, str->maxn);
	}

	str->s[str->n++] = c;
}

void vui_string_puts(vui_string* str, const unsigned char* s)
{
	if (str == NULL) return;

	if (s == NULL) return;

	for (;*s;s++)
	{
		vui_string_putc(str, *s);
	}
}

void vui_string_putn(vui_string* str, size_t n, const unsigned char* s)
{
	if (str == NULL) return;

	if (s == NULL) return;

	for (size_t i = 0; i < n; i++)
	{
		vui_string_putc(str, *s);
	}
}

void vui_string_append_printf(vui_string* str, const unsigned char* fmt, ...)
{
	if (str == NULL) return;

	if (fmt == NULL) return;

	va_list argp;

	va_start(argp, fmt);
	size_t len = vsnprintf(NULL, 0, fmt, argp) + 2;
	va_end(argp);

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STRING)
	vui_debugf("printf(%s, %s, ...)\n", vui_string_get(str), fmt);
#endif

	size_t n = str->n + len;
	// make room for new stuff and null terminator
	if (str->maxn < n + 1)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STRING)
		printf("realloc: %ld, %ld\n", str->maxn, str->n);
#endif
		str->maxn = (n + 1)*2;
		str->s = realloc(str->s, str->maxn);
	}

	va_start(argp, fmt);
	str->n += vsnprintf(&str->s[str->n], len, fmt, argp);
	va_end(argp);
}

void vui_string_append(vui_string* str, const vui_string* str2)
{
	if (str == NULL) return;

	if (str2 == NULL) return;

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STRING)
	vui_debugf("append(\"%s\", \"%s\")\n", vui_string_get(str), vui_string_get(str));
#endif


	size_t n = str->n + str2->n;
	// make room for str2 and null terminator
	if (str->maxn < n + 1)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STRING)
		printf("realloc: %ld, %ld\n", str->maxn, str->n);
#endif
		str->maxn = (n + 1)*2;
		str->s = realloc(str->s, str->maxn);
	}

	memcpy(&str->s[str->n], str2->s, str2->n);

	str->n = n;
}

void vui_string_put(vui_string* str, unsigned int c)
{
	if (str == NULL) return;

	unsigned char s[16];
	vui_utf8_encode(c, s);

	vui_string_puts(str, s);
}

void vui_string_get_replace(vui_string* str, char** s)
{
	if (str == NULL) return;

	if (s == NULL) return;

	char* s_old = *s;

	*s = vui_string_get(str);

	if (s_old != NULL)
	{
		free(s_old);
	}
}
