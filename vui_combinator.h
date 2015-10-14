#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "vui_fragment.h"

// N.B. for any binary combinator: You must not use lhs or rhs after calling
//  this; feed fragments through vui_frag_dup() before calling this function if
//  you want to keep fragments.  

// Unions two fragments
vui_frag* vui_frag_union(vui_frag* lhs, vui_frag* rhs);

// Concatenates two fragments
vui_frag* vui_frag_cat(vui_frag* lhs, vui_frag* rhs);

vui_frag* vui_frag_catv(vui_frag** frags);

#ifdef __cplusplus
}
#endif
