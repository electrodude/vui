#include "gc.h"

// not in statemachine.h so people don't call it
void vui_state_kill(vui_state* state);

vui_state vui_gc_dummystate;

vui_stack* vui_gc_roots;

void vui_gc_init(void)
{
	vui_gc_roots = vui_stack_new();
}

void vui_gc_register(vui_state* st)
{
	if (vui_gc_dummystate.gc_next == NULL)
	{
		vui_gc_dummystate.gc_next = st;
	}
	else
	{
		st->gc_next = vui_gc_dummystate.gc_next;
		vui_gc_dummystate.gc_next = st;
	}
}

void vui_gc_run(void)
{
	vui_iter_gen++;

	for (int i=0; i < vui_gc_roots->n; i++)
	{
		vui_state* root = vui_gc_roots->s[i];

		vui_gc_mark(root);
	}

	vui_state* curr = &vui_gc_dummystate;

	while (curr->gc_next != NULL)
	{
		if (curr->gc_next->iter_gen != vui_iter_gen)
		{
			vui_state* condemned = curr->gc_next;
			curr->gc_next = condemned->gc_next;

			vui_state_kill(condemned);
		}
		else
		{
			curr = curr->gc_next;
		}
	}
}

void vui_gc_mark(vui_state* st)
{
	if (st == NULL) return;

	if (st->iter_gen == vui_iter_gen) return;

	st->iter_gen = vui_iter_gen;

#ifdef VUI_DEBUG
	char s[256];
	snprintf(s, 256, "vui_gc_mark(0x%lX)\r\n", st);
	vui_debug(s);
#endif

	for (int i=0; i < VUI_MAXSTATE; i++)
	{
		vui_gc_mark(vui_next(st, i, VUI_ACT_GC));
	}
}
