#include "vui_debug.h"

#include "vui_mem.h"

#include "vui_stack_refcount.h"

#include "vui_gc_ptr.h"

static void vui_gc_ptr_kill(vui_gc_ptr* gcptr)
{
	vui_free(gcptr);
}

void vui_gc_ptr_gc_dtor(void* obj, vui_gc_dtor_mode mode)
{
	vui_gc_ptr* gcptr = (vui_gc_ptr*)obj;

	if (gcptr->dtor != NULL)
	{
		gcptr->dtor(gcptr->ptr, mode);
	}

	if (mode == VUI_GC_DTOR_MARK)
	{
	}
	else if (mode == VUI_GC_DTOR_SWEEP)
	{
		vui_gc_ptr_kill(gcptr);
	}
	else if (mode == VUI_GC_DTOR_DESCRIBE)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_GC)
		//vui_debugf("vui_gc: ptr        %p\n", st);
#endif
	}
}

vui_gc_ptr* vui_gc_ptr_new(vui_gc_dtor dtor, void* ptr)
{
	vui_gc_ptr* gcptr = vui_new(vui_gc_ptr);

	gcptr->dtor = dtor;
	gcptr->ptr = ptr;

	vui_gc_register(gcptr, vui_gc_ptr_gc_dtor);

	return gcptr;
}



void vui_gc_ptr_stack_gc_dtor(void* obj, vui_gc_dtor_mode mode)
{
	vui_stack* stk = (vui_stack*)obj;

	if (mode == VUI_GC_DTOR_MARK)
	{
		if (stk != NULL)
		{
			if (stk->def)
			{
				vui_gc_mark_header(stk->def);
			}

			for (size_t i = 0; i < stk->n; i++)
			{
				vui_gc_mark_header(stk->s[i]);
			}
		}
	}
	else if (mode == VUI_GC_DTOR_SWEEP)
	{
		vui_stack_kill(stk);
	}
	else if (mode == VUI_GC_DTOR_DESCRIBE)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_GC)
		vui_debugf("vui_gc: *stack     %p\n", stk);
#endif
	}
}

vui_gc_ptr* vui_gc_ptr_new_stack(vui_stack* stk)
{
	vui_stack_refcount_deconvert(stk); // make sure the stack isn't reference counted;
	                                   //  decrement counts if it was
	return vui_gc_ptr_new(vui_gc_ptr_stack_gc_dtor, stk);
}



void vui_gc_ptr_string_gc_dtor(void* obj, vui_gc_dtor_mode mode)
{
	vui_string* str = (vui_string*)obj;

	if (mode == VUI_GC_DTOR_MARK)
	{
	}
	else if (mode == VUI_GC_DTOR_SWEEP)
	{
		vui_string_kill(str);
	}
	else if (mode == VUI_GC_DTOR_DESCRIBE)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_GC)
		vui_debugf("vui_gc: *string    %p\n", str);
#endif
	}
}

vui_gc_ptr* vui_gc_ptr_new_string(vui_string* str)
{
	return vui_gc_ptr_new(vui_gc_ptr_string_gc_dtor, str);
}


