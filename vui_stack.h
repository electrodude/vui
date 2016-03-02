#ifndef VUI_STACK_H
#define VUI_STACK_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>

// Stack class

typedef struct vui_stack
{
	size_t n;                  // current size of stack
	size_t maxn;               // number of allocated slots
	void** s;                  // pointer to stack buffer
	void* def;                 // default in case of underflow
	void (*dtor)(void* elem);  // element destructor function
	                           //  (on trunc or reset)
} vui_stack;

// Create a new stack, given how many slots to preallocate
#define vui_stack_new_prealloc(maxn) vui_stack_new_prealloc_at(NULL, maxn)
vui_stack* vui_stack_new_prealloc_at(vui_stack* stk, size_t maxn);

// Create a new stack
#define vui_stack_new() vui_stack_new_at(NULL)
static inline vui_stack* vui_stack_new_at(vui_stack* stk)
{
	return vui_stack_new_prealloc_at(stk, 16);
}

// Create a new stack and fill it with the given data
#define vui_stack_new_v(n, ...) vui_stack_new_v_at(NULL, n, __VA_ARGS__)
vui_stack* vui_stack_new_v_at(vui_stack* stk, size_t n, ...);

// Create a new stack and fill it with the given data
#define vui_stack_new_array(n, elements) vui_stack_new_array_at(NULL, n, elements)
vui_stack* vui_stack_new_array_at(vui_stack* stk, size_t n, void** elements);

// Clone a stack
// As a hack, if the stack has a dtor, removes it.  This might cause a memory leak, but it elminates a double free.
#define vui_stack_dup(orig) vui_stack_dup_at(NULL, orig)
vui_stack* vui_stack_dup_at(vui_stack* stk, vui_stack* orig);

// Destroy given stack and its internal buffer
// Does not free any elements, but calling `vui_stack_reset` first does
void vui_stack_kill(vui_stack* stk);

// Reset stack size to n
// Does nothing if stack is already the same size or smaller
// Calls dtor(elem) on each element as it is removed, from top to bottom
void vui_stack_trunc(vui_stack* stk, size_t n);

// Reset stack size to 0
// Calls dtor(elem) on each element as it is removed, from top to bottom
static inline void vui_stack_reset(vui_stack* stk)
{
	vui_stack_trunc(stk, 0);
}

// Destroy given stack, returning its internal buffer and writing the number of elements to *n
// The caller assumes responsibilty for free()ing the returned buffer.
void** vui_stack_release(vui_stack* stk, size_t* n);

// realloc() internal buffer to be as small as possible
void* vui_stack_shrink(vui_stack* stk);

// Push an element onto the top of the stack
void vui_stack_push(vui_stack* stk, void* s);

// Push n elements onto the top of the stack
void vui_stack_pushn(vui_stack* stk, size_t n, void** elements);

// Append stk2 to the end of stk
void vui_stack_append(vui_stack* stk, vui_stack* stk2);

// Push an element onto the top of the stack
// Does nothing if `s == vui_stack_peek(stk)`.
void vui_stack_push_nodup(vui_stack* stk, void* s);

// Pop the top element off of the stack
void* vui_stack_pop(vui_stack* stk);

// Peek at the top element of the stack
void* vui_stack_peek(vui_stack* stk);

// Return the i-th element from the top of the stack, or NULL if out of range
// Is zero indexed, so vui_stack_index_end(stk, 0) == vui_stack_peek(stk)
void* vui_stack_index_end(vui_stack* stk, size_t i);

// Return the i-th element from the bottom of the stack, or NULL if out of range
void* vui_stack_index(vui_stack* stk, size_t i);

// Call given function on each element of the stack, from top to bottom
void vui_stack_foreach(vui_stack* stk, void (*func)(void* elem));

// Call given function on each element of the stack, from top to bottom
// Collects the return values in a new stack and returns it
vui_stack* vui_stack_map(vui_stack* stk, void* (*func)(void* elem));

#ifdef __cplusplus
}
#endif

#endif
