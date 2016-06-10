#include "vui_gc.h"

#include "vui_stack_refcount.h"

void vui_stack_refcount_elem_on_push(vui_gc_header* obj)
{
	vui_gc_incr_header(obj);
}

void vui_stack_refcount_elem_on_pop(vui_gc_header* obj)
{
	vui_gc_decr_header(obj);
}

vui_stack* vui_stack_refcount_convert(vui_stack* stk)
{
	if (stk == NULL) return NULL;

	stk->on_push = (void(*)(void*))vui_stack_refcount_elem_on_push;
	stk->on_pop  = (void(*)(void*))vui_stack_refcount_elem_on_pop;

	for (size_t i = 0; i < stk->n; i++)
	{
		vui_gc_incr_header((vui_gc_header*)stk->s[i]);
	}

	if (stk->def != NULL)
	{
		vui_gc_incr_header((vui_gc_header*)stk->def);
	}

	return stk;
}

vui_stack* vui_stack_refcount_deconvert(vui_stack* stk)
{
	if (stk == NULL) return NULL;

	if (stk->on_pop == (void(*)(void*))vui_stack_refcount_elem_on_pop)
	{
		for (size_t i = 0; i < stk->n; i++)
		{
			vui_gc_decr_header((vui_gc_header*)stk->s[i]);
		}

		if (stk->def != NULL)
		{
			vui_gc_decr_header((vui_gc_header*)stk->def);
		}

		stk->on_push = NULL;
		stk->on_pop = NULL;
	}
}
