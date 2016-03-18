#include "vui_debug.h"

#include "vui_gc.h"

vui_gc_header* vui_gc_first = NULL;

static int vui_gc_gen;

size_t vui_gc_nlive = 0;


#define VUI_GC_FOREACH(curr) for (vui_gc_header** curr = &vui_gc_first; *curr != NULL; curr = &(*curr)->gc_next)


void vui_gc_register_header(vui_gc_header* obj, vui_gc_dtor dtor)
{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_GC)
	vui_debugf("vui_gc_register_header: %p\n", obj);
#endif

	obj->root = 0;

	obj->dtor = dtor;

	obj->gc_gen = vui_gc_gen;

	obj->gc_next = vui_gc_first;

	vui_gc_first = obj;

	vui_gc_nlive++;
}

void vui_gc_run(void)
{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_GC)
	vui_debugf("vui_gc_run: gc_gen = %d, vui_gc_first = %p\n", vui_gc_gen, vui_gc_first);
#endif

	// unmark all objects
	vui_gc_gen++;

	// mark
	size_t n = 0;

	VUI_GC_FOREACH(curr)
	{
		vui_gc_header* gc = *curr;

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_GC)
		vui_debugf("vui_gc: check %p: root = %d\n", gc, gc->root);
#endif

		if (gc->root)
		{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_GC)
			gc->dtor(gc, VUI_GC_DTOR_DESCRIBE);
#endif
			vui_gc_mark_header(gc);
		}

		n++;
	}

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_GC)
	vui_debugf("vui_gc: mark: checked %d objects\n", n);
#endif

	// sweep
	vui_gc_header** curr = &vui_gc_first;

	n = 0;

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

			vui_gc_nlive--;
		}
		else
		{
			vui_gc_header* kept = *curr;

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_GC)
			vui_debugf("vui_gc: keep %p\n", kept);

			kept->dtor(kept, VUI_GC_DTOR_DESCRIBE);
#endif

			curr = &(*curr)->gc_next;
		}

		n++;
	}

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_GC)
	vui_debugf("vui_gc: sweep: checked %d objects\n", n);
#endif

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_GC)
	// describe
	n = 0;

	VUI_GC_FOREACH(curr)
	{
		vui_gc_header* gc = *curr;

		gc->dtor(gc, VUI_GC_DTOR_DESCRIBE);

		n++;
	}

	vui_debugf("vui_gc: describe: %d objects still exist\n", n);
#endif

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

void vui_gc_incr_header(vui_gc_header* obj)
{
	obj->root++;
}

void vui_gc_decr_header(vui_gc_header* obj)
{
	if (obj->root <= 0)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_GC)
		vui_debugf("vui_gc_decr_header(%p): root < 0!\n", obj);
#endif

		return;
	}

	obj->root--;
}
