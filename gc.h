#pragma once

#include "stack.h"
#include "statemachine.h"

extern vui_stack* vui_gc_roots;

void vui_gc_init(void);

void vui_gc_register(vui_state* st);

void vui_gc_run();

void vui_gc_mark(vui_state* st);
