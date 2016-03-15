#ifndef VUI_UTF8_H
#define VUI_UTF8_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "vui_string.h"

char* vui_utf8_encode(unsigned int c, char* s);

vui_string* vui_utf8_encode_str(unsigned int c);

char* vui_utf8_encode_alloc(unsigned int c);

char* vui_utf8_encode_static(unsigned int c);

#ifdef __cplusplus
}
#endif

#endif
