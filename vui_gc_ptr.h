#ifndef VUI_GC_PTR_H
#define VUI_GC_PTR_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "vui_gc.h"

#include "vui_stack.h"
#include "vui_string.h"

typedef struct vui_gc_ptr
{
	vui_gc_header gc;

	vui_gc_dtor dtor;

	void* ptr;
} vui_gc_ptr;


vui_gc_ptr* vui_gc_ptr_new(vui_gc_dtor dtor, void* ptr);


vui_gc_ptr* vui_gc_ptr_new_stack(vui_stack* stk);

vui_gc_ptr* vui_gc_ptr_new_string(vui_string* str);


#ifdef __cplusplus
}
#endif

#endif
