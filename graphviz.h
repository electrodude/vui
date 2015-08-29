#pragma once

#include <stdio.h>

#include "stack.h"
#include "statemachine.h"

void vui_gv_write(FILE* f, vui_stack* roots);

void vui_gv_print_s(FILE* f, vui_state* s);
