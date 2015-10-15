#include <stdlib.h>

#include "vui_debug.h"

#include "vui_utf8.h"


unsigned char* vui_utf8_encode(unsigned int c, unsigned char* s)
{
	if (c < 0x80)
	{
		*s++ = c;
	}
	else if (c < 0x800)
	{
		*s++ = 192 | ((c >>  6) & 0x1F);
		*s++ = 128 | ((c >>  0) & 0x3F);
	}
	else if (c < 0x10000)
	{
		*s++ = 224 | ((c >> 12) & 0x0F);
		*s++ = 128 | ((c >>  6) & 0x3F);
		*s++ = 128 | ((c >>  0) & 0x3F);
	}
	else if (c < 0x200000)
	{
		*s++ = 240 | ((c >> 18) & 0x07);
		*s++ = 128 | ((c >> 12) & 0x3F);
		*s++ = 128 | ((c >>  6) & 0x3F);
		*s++ = 128 | ((c >>  0) & 0x3F);
	}
	else if (c < 0x4000000)
	{
		*s++ = 248 | ((c >> 24) & 0x03);
		*s++ = 128 | ((c >> 18) & 0x3F);
		*s++ = 128 | ((c >> 12) & 0x3F);
		*s++ = 128 | ((c >>  6) & 0x3F);
		*s++ = 128 | ((c >>  0) & 0x3F);
	}
	else if (c < 0x80000000)
	{
		*s++ = 252 | ((c >> 30) & 0x01);
		*s++ = 128 | ((c >> 24) & 0x3F);
		*s++ = 128 | ((c >> 18) & 0x3F);
		*s++ = 128 | ((c >> 12) & 0x3F);
		*s++ = 128 | ((c >>  6) & 0x3F);
		*s++ = 128 | ((c >>  0) & 0x3F);
	}
#if VUI_UTF8_EXTRA
	else if (c < 0x1000000000)
	{
		*s++ = 254 | ((c >> 36) & 0x00);
		*s++ = 128 | ((c >> 30) & 0x3F);
		*s++ = 128 | ((c >> 24) & 0x3F);
		*s++ = 128 | ((c >> 18) & 0x3F);
		*s++ = 128 | ((c >> 12) & 0x3F);
		*s++ = 128 | ((c >>  6) & 0x3F);
		*s++ = 128 | ((c >>  0) & 0x3F);
	}
	else if (c < 0x20000000000)
	{
		*s++ = 255 | ((c >> 40) & 0x00);
		*s++ = 128 | ((c >> 36) & 0x3F);
		*s++ = 128 | ((c >> 30) & 0x3F);
		*s++ = 128 | ((c >> 24) & 0x3F);
		*s++ = 128 | ((c >> 18) & 0x3F);
		*s++ = 128 | ((c >> 12) & 0x3F);
		*s++ = 128 | ((c >>  6) & 0x3F);
		*s++ = 128 | ((c >>  0) & 0x3F);
	}
#endif
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_UTF8)
	else
	{
		vui_debugf("Error: can't encode value as UTF-8: %d\n", c);
	}
#endif

	*s = 0;

	return s;
}

unsigned char* vui_utf8_encode_alloc(unsigned int c)
{
	unsigned char* s = malloc(16);
	return realloc(s, vui_utf8_encode(c, s) - s);
}
