#include "vui_debug.h"

#include "vui_gc.h"

vui_gc_header* vui_gc_first = NULL;

static int vui_gc_gen;

void vui_gc_register_header(vui_gc_header* obj, vui_gc_dtor dtor)
{
	obj->dtor = dtor;

	obj->gc_gen = vui_gc_gen;

	obj->gc_next = vui_gc_first;

	vui_gc_first = obj;
}

void vui_gc_run(void)
{
	vui_gc_gen++;

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_GC)
	vui_debugf("vui_gc_run: gc_gen = %d, vui_gc_first = %p\n", vui_gc_gen, vui_gc_first);
#endif

	vui_gc_header** curr = &vui_gc_first;

	while (*curr != NULL)
	{

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_GC)
		vui_debugf("vui_gc: check %p: root = %d\n", *curr, (*curr)->root);
#endif

		if ((*curr)->root)
		{
			vui_gc_mark_header(*curr);
		}

		curr = &(*curr)->gc_next;
	}

	curr = &vui_gc_first;

	while (*curr != NULL)
	{
		if ((*curr)->gc_gen != vui_gc_gen)
		{
			vui_gc_header* condemned = *curr;
			*curr = condemned->gc_next;

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_GC)
			vui_debugf("vui_gc: sweep %p\n", condemned);
#endif

			condemned->dtor(condemned, VUI_GC_DTOR_SWEEP);
		}
		else
		{
			curr = &(*curr)->gc_next;
		}
	}

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_GC)
	vui_debugf("vui_gc_first = %p\n", vui_gc_first);
#endif
}

void vui_gc_mark_header(vui_gc_header* obj)
{
	if (obj == NULL) return;

	if (obj->gc_gen == vui_gc_gen) return;

	obj->gc_gen = vui_gc_gen;

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_GC)
	vui_debugf("vui_gc_mark(%p)\n", obj);
#endif

	obj->dtor(obj, VUI_GC_DTOR_MARK);
}
