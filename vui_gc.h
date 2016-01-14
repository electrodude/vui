#ifndef VUI_GC_H
#define VUI_GC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "vui_stack.h"
#include "vui_statemachine.h"

void vui_gc_register(vui_state* st);

void vui_gc_run();

void vui_gc_mark(vui_state* st);

#ifdef __cplusplus
}
#endif

#endif
