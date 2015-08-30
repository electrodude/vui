#pragma once

#include <stdlib.h>

// Stack class

typedef struct vui_stack
{
	size_t n;     // current size of stack
	size_t maxn;  // number of allocated slots
	void** s;     // pointer to stack buffer
	void* def;    // default in case of underflow
} vui_stack;

// Create a new stack
// Takes one argument: what to return on underflow
vui_stack* vui_stack_new(void* def);

// Destroys a stack and its internal buffer
// Does not free any elements, but calling `vui_stack_reset` first does
void vui_stack_kill(vui_stack* stk);

// Resets stack size to 0
// Calls dtor(elem) on each element as it is removed, from top to bottom
void vui_stack_reset(vui_stack* stk, void (*dtor)(void* stk));

// Destroys a stack, returning its internal buffer and writing the number of elements to *n
// The caller assumes responsibilty for free()ing the returned buffer.
void** vui_stack_release(vui_stack* stk, int* n);

// Pushes an element onto the top of the stack
void vui_stack_push(vui_stack* stk, void* s);

// Pushes an element onto the top of the stack
// Does nothing if `s == vui_stack_peek(stk)`.
void vui_stack_push_nodup(vui_stack* stk, void* s);

// Pops the top element off of the stack
void* vui_stack_pop(vui_stack* stk);

// Peeks at the top element of the stack
void* vui_stack_peek(vui_stack* stk);

// Returns the element at index `i`, or NULL if out of range
void* vui_stack_index(vui_stack* stk, unsigned int i);
