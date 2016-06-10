#include "vui_mem.h"

#include "vui_stack_refcount.h"

#include "vui_fragment.h"

// much credit goes to https://swtch.com/~rsc/regexp/regexp1.html
//  although vui doesn't presently support NFAs

// basic fragment functions
vui_frag* vui_frag_new(vui_state* entry, vui_stack* exits)
{
	vui_frag* frag = vui_new(vui_frag);

	frag->entry = entry;

	if (frag->entry)
	{
		vui_gc_incr(frag->entry);
	}

	frag->exits = exits;

	return frag;
}

void vui_frag_kill(vui_frag* frag)
{
	vui_gc_decr(frag->entry);

	vui_stack_kill(frag->exits);

	vui_free(frag);
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
		vui_transition* t = vui_state_index(orig, i);
		if (t->next != NULL)
		{
			t = vui_transition_dup(t);
			t->next = vui_frag_state_dup(t->next);
		}
		vui_set_char_t(state, i, t);
	}

	return state;
}

vui_frag* vui_frag_dup(vui_frag* orig)
{
	vui_iter_gen++;

	vui_stack* newexits = vui_stack_map(orig->exits, (void* (*)(void*))vui_frag_state_dup);

	vui_frag* frag = vui_frag_new(vui_frag_state_dup(orig->entry), newexits);

	return frag;
}

vui_state* vui_frag_release(vui_frag* frag, vui_state* exit)
{
	if (exit != NULL)
	{
		for (size_t i = 0; i < frag->exits->n; i++)
		{
			vui_state_cp(frag->exits->s[i], exit);
		}
	}

	vui_state* entry = frag->entry;

	vui_frag_kill(frag);

	return entry;
}


// constructors for various useful fragments

vui_frag* vui_frag_new_string_t(char* s, vui_transition* t)
{
	vui_stack* exits = vui_stack_refcount_new();

	vui_state* st_start = vui_state_new_t(NULL);

	vui_string_reset(&st_start->name);
	vui_string_puts(&st_start->name, "\"");
	vui_string_puts(&st_start->name, s);
	vui_string_putc(&st_start->name, '"');

	vui_state* st_curr = st_start;

	for (;*s;s++)
	{
		vui_state* st_next = vui_state_new_t(NULL);
		vui_transition* t2 = vui_transition_dup(t);
		t2->next = st_next;
		vui_set_char_t(st_curr, *s, t2);
		st_curr = st_next;

		vui_string_reset(&st_curr->name);
		vui_string_putc(&st_curr->name, *s);
	}

	vui_stack_push(exits, st_curr);

	return vui_frag_new(st_start, exits);
}

static int vui_getchar_escaped(char** s, int* escaped)
{
	*escaped = 0;

	switch (**s)
	{
		case '\\':
			(*s)++;
			*escaped = 1;
			switch (**s)
			{
				case 'n':
					return '\n';
					break;
				case 'r':
					return '\r';
					break;
				case 't':
					return '\t';
					break;
				case '0':
					return '\0';
					break;
				case 'x':
				{
					char a = *(*s)++;
					char b = *(*s)++;
					return (a << 4) + b;
					break;
				}
				default:
					return *(*s)++;
					break;
			}
			break;
		default:
			return *(*s)++;
	}
}

vui_frag* vui_frag_new_regexp_t(char* s, vui_transition* t)
{
	vui_stack* exits = vui_stack_refcount_new();

	vui_state* st_start = vui_state_new_t(NULL);

	vui_string_reset(&st_start->name);
	vui_string_puts(&st_start->name, "/");
	vui_string_puts(&st_start->name, s);
	vui_string_putc(&st_start->name, '/');

	vui_state* st_curr = st_start;

	while (*s)
	{
		vui_state* st_next = vui_state_new_t(NULL);
		vui_transition* t2 = vui_transition_dup(t);
		t2->next = st_next;

		char* sb = s;

		int escaped = 0;
		int c = vui_getchar_escaped(&s, &escaped);

		if (!escaped)
		{
			switch (c)
			{
				case '.':
					vui_set_range_t(st_curr, 0, 255, t2);
					break;
				case '[':
					c = 0;
					int pc = 0;
					for (;;)
					{
						pc = c;
						escaped = 0;
						c = vui_getchar_escaped(&s, &escaped);
						if (!escaped)
						{
							if (c == ']')
							{
								break;
							}
							else if (c == '-')
							{
								c = vui_getchar_escaped(&s, &escaped);
								vui_set_range_t(st_curr, pc, c, t2);
								continue;
							}
						}

						vui_set_char_t(st_curr, c, t2);
					}
					break;
				default:
					vui_set_char_t(st_curr, c, t2);
					break;
			}
		}
		else
		{
			vui_set_char_t(st_curr, c, t);
		}

		st_curr = st_next;

		vui_string_reset(&st_curr->name);
		vui_string_append_stretch(&st_curr->name, sb, s);
	}

	vui_stack_push(exits, st_curr);

	return vui_frag_new(st_start, exits);
}

vui_frag* vui_frag_new_any_t(vui_transition* t)
{
	vui_stack* exits = vui_stack_refcount_new();

	vui_state* st_start = vui_state_new_t(NULL);

	vui_string_reset(&st_start->name);
	vui_string_puts(&st_start->name, "/./");

	vui_state* st_curr = st_start;

	vui_state* st_next = vui_state_new_t(NULL);
	vui_string_reset(&st_next->name);
	vui_string_putc(&st_next->name, '.');

	vui_transition* t2 = vui_transition_dup(t);
	t2->next = st_next;
	vui_set_range_t(st_curr, 0, 255, t2);
	st_curr = st_next;

	vui_stack_push(exits, st_curr);

	return vui_frag_new(st_start, exits);
}
