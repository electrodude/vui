#pragma once

#include "stack.h"
#include "statemachine.h"

extern vui_state vui_gc_dummystate;

void vui_gc_register(vui_state* st);

void vui_gc_run();

void vui_gc_mark(vui_state* st);
