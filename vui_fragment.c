#include "vui_fragment.h"

// much credit goes to https://swtch.com/~rsc/regexp/regexp1.html
//  although vui doesn't presently support NFAs

vui_frag* vui_frag_new(vui_state* entry, vui_stack* exits)
{
	vui_frag* frag = malloc(sizeof(vui_frag));

	frag->entry = entry;

	vui_gc_incr(frag->entry);

	frag->exits = exits;

	return frag;
}

void vui_frag_kill(vui_frag* frag)
{
	vui_gc_decr(frag->entry);

	vui_stack_kill(frag->exits);

	free(frag);
}

static vui_state* vui_frag_state_dup(vui_state* orig)
{
	if (orig->iter_gen == vui_iter_gen) return orig->iter_data.st;

	orig->iter_gen = vui_iter_gen;

	vui_state* state = vui_state_new();

	orig->iter_data.st = state;

	state->data = orig->data;

	state->push = orig->push;
	state->name = orig->name;

	for (unsigned int i=0; i<VUI_MAXSTATE; i++)
	{
		vui_transition t = orig->next[i];
		if (t.next != NULL)
		{
			t.next = vui_frag_state_dup(t.next);
		}
		vui_set_char_t(state, i, t);
	}

	return state;
}

vui_frag* vui_frag_dup(vui_frag* orig)
{
	vui_iter_gen++;

	vui_stack* newexits = vui_stack_map(orig->exits, (void* (*)(void*))vui_frag_state_dup);

	return vui_frag_new(vui_frag_state_dup(orig->entry), newexits);
}

vui_state* vui_frag_release(vui_frag* frag, vui_state* exit)
{
	for (size_t i=0; i<frag->exits->n; i++)
	{
		vui_state_cp(frag->exits->s[i], exit);
	}

	vui_state* entry = frag->entry;

	vui_frag_kill(frag);

	return entry;
}
