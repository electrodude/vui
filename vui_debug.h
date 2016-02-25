#ifndef VUI_DEBUG_H
#define VUI_DEBUG_H

#ifdef __cplusplus
extern "C"
{
#endif

//#define VUI_DEBUG

#ifdef VUI_DEBUG

#include <stdio.h>
#include <stdlib.h>

#define VUI_DEBUG_TEST
#define VUI_DEBUG_VUI
#define VUI_DEBUG_STATEMACHINE
//#define VUI_DEBUG_GC
//#define VUI_DEBUG_STACK
//#define VUI_DEBUG_STRING
//#define VUI_DEBUG_UTF8

extern void vui_debugf(const char* format, ...);  // user must declare this

#endif

#ifdef __cplusplus
}
#endif

#endif
