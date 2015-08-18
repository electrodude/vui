#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "statemachine.h"


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

vui_state* vui_state_new_t(vui_transition transition)
{
	vui_state* state = malloc(sizeof(vui_state));

	state->refs = 0;

	for (int i=0; i<MAXINPUT; i++)
	{
		vui_set_char_t(state, i, transition);
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


static vui_state* tfunc_sameas_s(vui_state* currstate, int c, int act, void* data)
{
	vui_state* other = data;

	if (other == currstate) // evade stack overflow
	{
		return currstate;
	}

	return vui_next_t(currstate, c, other->next[c], act ? act + 1 : 0);
}

vui_transition vui_transition_sameas_s(vui_state* other)
{
	return (vui_transition){.next = NULL, .func = tfunc_sameas_s, .data = other};
}


static vui_state* tfunc_sameas_t(vui_state* currstate, int c, int act, void* data)
{
	vui_transition* t = data;

	return vui_next_t(currstate, c, *t, act ? act + 1 : 0);
}

vui_transition vui_transition_sameas_t(vui_transition* t)
{
	return (vui_transition){.next = NULL, .func = tfunc_sameas_t, .data = t};
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
	for (int c = c1; c != c2+1; c = (c+1) % MAXINPUT)
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

vui_state* vui_next_t(vui_state* currstate, int c, vui_transition t, int act)
{
	vui_state* nextstate = NULL;

	if (t.func != NULL && (act || t.next == NULL))
	{
		vui_state* retnext = t.func(currstate, c, act, t.data);
		nextstate =  t.next  != NULL ? t.next  :
		             retnext != NULL ? retnext :
		             currstate;
	}
	else
	{
		nextstate = t.next  != NULL ? t.next  :
		            currstate;
	}


	return nextstate;
}

vui_state* vui_run_c(vui_state** sp, char c, int act)
{
	*sp = vui_next(*sp, c, act);
}

vui_state* vui_run_s(vui_state** sp, char* s, int act)
{
	vui_state* st = *sp;

	for (;*s;s++)
	{
		st = vui_next(st, *s, act);
	}

	*sp = st;

	return st;
}
