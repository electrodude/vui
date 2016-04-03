#ifndef VUI_GRAPHVIZ_H
#define VUI_GRAPHVIZ_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>

#include "vui_string.h"
#include "vui_stack.h"
#include "vui_statemachine.h"

void vui_gv_write(FILE* f, vui_stack* roots);
void vui_gv_write_str(vui_string* out, vui_stack* roots);

void vui_gv_print_t(vui_string* out, vui_state* currstate, vui_transition* t);
void vui_gv_print_s(vui_string* out, vui_state* s);

#ifdef __cplusplus
}
#endif

#endif
