#include "debug.h"

#include "gc.h"

// not in statemachine.h so people don't call it
void vui_state_kill(vui_state* state);

vui_state* vui_gc_first;

void vui_gc_register(vui_state* st)
{
	if (vui_gc_first == NULL)
	{
		vui_gc_first = st;
	}
	else
	{
		st->gc_next = vui_gc_first;
		vui_gc_first = st;
	}
}

void vui_gc_run(void)
{
	vui_iter_gen++;

	vui_state** curr = &vui_gc_first;

	while (*curr != NULL)
	{
		if ((*curr)->root)
		{
			vui_gc_mark(*curr);
		}

		*curr = (*curr)->gc_next;
	}

	curr = &vui_gc_first;

	while (*curr != NULL)
	{
		if ((*curr)->iter_gen != vui_iter_gen)
		{
			vui_state* condemned = *curr;
			*curr = condemned->gc_next;

			vui_state_kill(condemned);
		}
		else
		{
			*curr = (*curr)->gc_next;
		}
	}
}

void vui_gc_mark(vui_state* st)
{
	if (st == NULL) return;

	if (st->iter_gen == vui_iter_gen) return;

	st->iter_gen = vui_iter_gen;

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_GC)
	char s[256];
	snprintf(s, 256, "vui_gc_mark(0x%lX)\r\n", st);
	vui_debug(s);
#endif

	for (int i=0; i < VUI_MAXSTATE; i++)
	{
		vui_gc_mark(vui_next(st, i, VUI_ACT_GC));
	}
}
