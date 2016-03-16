#ifndef VUI_COMBINATOR_H
#define VUI_COMBINATOR_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdarg.h>

#include "vui_fragment.h"

// N.B. for any binary combinator: You must not use lhs or rhs after calling
//  this; feed fragments through vui_frag_dup() before calling this function if
//  you want to keep fragments.  

// Unions two fragments
// rhs's transitions win any conflicts
vui_frag* vui_frag_union(vui_frag* lhs, vui_frag* rhs);

vui_frag* vui_frag_unionv(size_t n, ...);


// Merges a fragment into an already-existing state machine
// This is mostly like calling:
//  vui_frag_release(vui_frag_union(vui_frag_new(lhs, vui_stack_new()), rhs), vui_state* exit)
vui_state* vui_frag_merge(vui_state* lhs, vui_frag* rhs, vui_state* exit);

// Concatenates two fragments
vui_frag* vui_frag_cat(vui_frag* lhs, vui_frag* rhs);

vui_frag* vui_frag_catv(size_t n, ...);

#ifdef __cplusplus
}
#endif

#endif
