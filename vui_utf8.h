#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

unsigned char* vui_utf8_encode(unsigned int c, unsigned char* s);

unsigned char* vui_utf8_encode_alloc(unsigned int c);

#ifdef __cplusplus
}
#endif
