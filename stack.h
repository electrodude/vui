#pragma once

typedef struct vui_stack
{
	size_t n;
	size_t maxn;
	void** s;
	void* def; // default
} vui_stack;

vui_stack* vui_stack_new(void* def);
void vui_stack_kill(vui_stack* stk);
void vui_stack_reset(vui_stack* stk, void (*dtor)(void* stk));
void* vui_stack_release(vui_stack* stk, int* n);

void vui_stack_push(vui_stack* stk, void* s);
void* vui_stack_pop(vui_stack* stk);
void* vui_stack_peek(vui_stack* stk);
