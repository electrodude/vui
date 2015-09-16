#pragma once

#include "stack.h"

#include "statemachine.h"

typedef struct vui_frag
{
	vui_state* entry;
	vui_state** exits;
	size_t n_exits;
} vui_frag;

vui_frag* vui_frag_new(vui_state* entry, vui_state** exits, size_t n_exits);
vui_frag* vui_frag_new_stk(vui_state* entry, vui_stack* exits);
void vui_frag_kill(vui_frag* frag);
vui_frag* vui_frag_dup(vui_frag* orig);

vui_state* vui_frag_release(vui_frag* frag, vui_state* exit);
