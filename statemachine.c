#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "statemachine.h"

vui_state* vui_curr_state;


vui_state* vui_state_new(vui_state* parent)
{
	vui_state* state = malloc(sizeof(vui_state));

	state->refs = 0;

	if (parent != NULL)
	{
		for (int i=0; i<MAXINPUT; i++)
		{
			vui_set_char_t(state, i, parent->next[i]);
		}
	}
	else
	{
		vui_transition next = vui_transition_new1(state);

		state->refs++;

		for (int i=0; i<MAXINPUT; i++)
		{
			vui_set_char_t(state, i, next);
		}
	}

	return state;
}

vui_state* vui_state_cow(vui_state* parent, int c)
{
	vui_transition t = parent->next[c];
	vui_state* state = t.next;

	if (state == NULL) return NULL;

	if (state->refs == 1) return state;

	vui_state* newstate = vui_state_new(state);

	t.next = newstate;

	vui_set_char_t(parent, c, t);

	return newstate;
}

int vui_state_kill(vui_state* state)
{
	if (state == NULL) return 0;

	if (--state->refs != 0) return 0;

#ifdef VUI_DEBUG
	char s[64];
	snprintf(s, 64, "Killing state 0x%lX\n", state);
	vui_debug(s);
#endif

	for (int i=0; i<MAXINPUT; i++)
	{
		vui_state* next = vui_next(state, i, 0);
		if (next != state)
		{
			vui_state_kill(next);
		}
	}

	free(state);
}

void vui_state_replace(vui_state* state, vui_transition search, vui_transition replacement)
{
	for (int i=0; i<MAXINPUT; i++)
	{
		vui_transition t = state->next[i];

		if (t.func == search.func && t.data == search.data)
		{
			vui_set_char_t(state, i, replacement);
		}
	}
}

void vui_set_char_t(vui_state* state, int c, vui_transition next)
{
	vui_state* oldnext = vui_next(state, c, 0);

	state->next[c] = next;

	vui_state* newnext = vui_next(state, c, 0);

	if (newnext != NULL && newnext != state)
	{
		newnext->refs++;
	}

	if (oldnext != state)
	{
		vui_state_kill(oldnext);
	}
}

void vui_set_range_t(vui_state* state, int c1, int c2, vui_transition next)
{
	for (int c = c1; c != c2; c = (c+1) % MAXINPUT)
	{
		vui_set_char_t(state, c, next);
	}
}

void vui_set_string_t(vui_state* state, char* s, vui_transition next)
{
	vui_transition t = state->next[*s];

	if (t.next == NULL)
	{
		printf("vui_set_string_t: bad transition! String: %s\n", s);
	}
	else
	{
		if (s[1])
		{
			vui_state* nextst = vui_state_cow(state, *s);
			vui_set_string_t(nextst, s+1, next);
		}
		else
		{
			vui_set_char_t(state, *s, next);
		}
	}
}

vui_state* vui_next(vui_state* s, int c, int act)
{
	vui_transition t = s->next[c];

	vui_state* retnext = NULL;

	if (t.func != NULL)
	{
		retnext = t.func(s, c, act, t.data);
		return t.next  != NULL ? t.next  :
		       retnext != NULL ? retnext :
		       s;
	}
	else
	{
		return t.next  != NULL ? t.next  :
		       s;
	}
}

void vui_input(int c)
{
	vui_curr_state = vui_next(vui_curr_state, c, 1);
}

void vui_runstring(vui_state** sp, char* s, int act)
{
	for (;*s;s++)
	{
		*sp = vui_next(*sp, *s, act);
	}
}

