#ifndef VUI_STACK_REFCOUNT_H
#define VUI_STACK_REFCOUNT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "vui_gc.h"

#include "vui_stack.h"

// Convert the given stack to a reference counting stack
// Sets stk->on_push and stk->on_pop to call vui_gc_incr and vui_gc_decr
// You must only put subclasses of vui_gc_header on this stack
vui_stack* vui_stack_refcount_convert(vui_stack* stk);

// Constructors like those for vui_stack
#define vui_stack_refcount_new_at(stk) vui_stack_refcount_convert(vui_stack_new_at(stk))
#define vui_stack_refcount_new() vui_stack_refcount_convert(vui_stack_new())
#define vui_stack_refcount_new_v(n, ...) vui_stack_refcount_convert(vui_stack_new_v(n, __VA_ARGS__))

// Remove reference counting hooks from stack, and decrement reference counts of objects
//  Call this before giving this stack to something that
vui_stack* vui_stack_refcount_deconvert(vui_stack* stk);

#ifdef __cplusplus
}
#endif

#endif
