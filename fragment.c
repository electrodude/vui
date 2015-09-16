#include "fragment.h"

// much credit goes to https://swtch.com/~rsc/regexp/regexp1.html
//  although vui doesn't presently support NFAs

vui_frag* vui_frag_new(vui_state* entry, vui_state** exits, size_t n_exits)
{
	vui_frag* frag = malloc(sizeof(vui_frag));

	frag->entry = entry;

	frag->entry->root++;

	frag->exits = exits;
	frag->n_exits = n_exits;

	return frag;
}

vui_frag* vui_frag_new_stk(vui_state* entry, vui_stack* exits)
{
	vui_frag* frag = malloc(sizeof(vui_frag));

	frag->entry = entry;

	frag->entry->root++;

	frag->exits = (vui_state**)vui_stack_release(exits, &frag->n_exits);

	return frag;
}

void vui_frag_kill(vui_frag* frag)
{
	frag->entry->root--;

	free(frag);
}

vui_state* vui_frag_state_dup(vui_state* orig)
{
	if (orig->iter_gen == vui_iter_gen) return orig->iter_other;

	orig->iter_gen = vui_iter_gen;

	vui_state* state = vui_state_new();

	orig->iter_other = state;

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

	vui_state** newexits = malloc(orig->n_exits*sizeof(vui_state*));

	for (unsigned int i=0; i < orig->n_exits; i++)
	{
		newexits[i] = vui_frag_state_dup(orig->exits[i]);
	}

	return vui_frag_new(vui_frag_state_dup(orig->entry), newexits, orig->n_exits);
}

vui_state* vui_frag_release(vui_frag* frag, vui_state* exit)
{
	for (int i=0; i<frag->n_exits; i++)
	{
		vui_state_cp(frag->exits[i], exit);
	}

	vui_state* entry = frag->entry;

	free(frag);

	return entry;
}
