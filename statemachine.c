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
		for (unsigned int i=0; i<VUI_MAXSTATE; i++)
		{
			vui_set_char_t_raw(state, i, parent->next[i]);
		}

		state->data = parent->data;
	}
	else
	{
		vui_transition next = vui_transition_new1(state);

		state->refs++;

		for (unsigned int i=0; i<VUI_MAXSTATE; i++)
		{
			vui_set_char_t_raw(state, i, next);
		}

		state->data = NULL;
	}

	return state;
}

vui_state* vui_state_new_t(vui_transition transition)
{
	vui_state* state = malloc(sizeof(vui_state));

	state->refs = 0;

	for (unsigned int i=0; i<VUI_MAXSTATE; i++)
	{
		vui_set_char_t_raw(state, i, transition);
	}

	state->data = NULL;

	return state;
}

vui_state* vui_state_cow(vui_state* parent, unsigned int c)
{
	vui_transition t = parent->next[c];
	vui_state* state = t.next;

	if (state == NULL) return NULL;

	if (state->refs == 1) return state;

	vui_state* newstate = vui_state_new(state);

	t.next = newstate;

	vui_set_char_t_raw(parent, c, t);

	return newstate;
}

int vui_state_kill(vui_state* state)
{
	if (state == NULL) return 0;

	if (--state->refs != 0) return 0;

#ifdef VUI_DEBUG
	char s[64];
	snprintf(s, 64, "Killing state 0x%lX\r\n", state);
	vui_debug(s);
#endif

	for (int i=0; i<VUI_MAXSTATE; i++)
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
	for (unsigned int i=0; i<VUI_MAXSTATE; i++)
	{
		vui_transition t = state->next[i];

		if (t.func == search.func && t.data == search.data)
		{
			vui_set_char_t(state, i, replacement);
		}
	}
}


static vui_state* tfunc_sameas_s(vui_state* currstate, unsigned int c, int act, void* data)
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


static vui_state* tfunc_sameas_t(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_transition* t = data;

	return vui_next_t(currstate, c, *t, act ? act + 1 : 0);
}

vui_transition vui_transition_sameas_t(vui_transition* t)
{
	return (vui_transition){.next = NULL, .func = tfunc_sameas_t, .data = t};
}



void vui_codepoint_to_utf8(unsigned int c, unsigned char* s)
{
	if (c < 0x80)
	{
		*s++ = c;
	}
	else if (c < 0x800)
	{
		*s++ = 192 | ((c >>  6) & 0x1F);
		*s++ = 128 | ((c >>  0) & 0x3F);
	}
	else if (c < 0x10000)
	{
		*s++ = 224 | ((c >> 12) & 0x0F);
		*s++ = 128 | ((c >>  6) & 0x3F);
		*s++ = 128 | ((c >>  0) & 0x3F);
	}
	else if (c < 0x200000)
	{
		*s++ = 240 | ((c >> 18) & 0x07);
		*s++ = 128 | ((c >> 12) & 0x3F);
		*s++ = 128 | ((c >>  6) & 0x3F);
		*s++ = 128 | ((c >>  0) & 0x3F);
	}
	else if (c < 0x4000000)
	{
		*s++ = 248 | ((c >> 24) & 0x03);
		*s++ = 128 | ((c >> 18) & 0x3F);
		*s++ = 128 | ((c >> 12) & 0x3F);
		*s++ = 128 | ((c >>  6) & 0x3F);
		*s++ = 128 | ((c >>  0) & 0x3F);
	}
	else if (c < 0x80000000)
	{
		*s++ = 252 | ((c >> 30) & 0x01);
		*s++ = 128 | ((c >> 24) & 0x3F);
		*s++ = 128 | ((c >> 18) & 0x3F);
		*s++ = 128 | ((c >> 12) & 0x3F);
		*s++ = 128 | ((c >>  6) & 0x3F);
		*s++ = 128 | ((c >>  0) & 0x3F);
	}
#if VUI_UTF8_EXTRA
	else if (c < 0x1000000000)
	{
		*s++ = 254 | ((c >> 36) & 0x00);
		*s++ = 128 | ((c >> 30) & 0x3F);
		*s++ = 128 | ((c >> 24) & 0x3F);
		*s++ = 128 | ((c >> 18) & 0x3F);
		*s++ = 128 | ((c >> 12) & 0x3F);
		*s++ = 128 | ((c >>  6) & 0x3F);
		*s++ = 128 | ((c >>  0) & 0x3F);
	}
	else if (c < 0x20000000000)
	{
		*s++ = 255 | ((c >> 40) & 0x00);
		*s++ = 128 | ((c >> 36) & 0x3F);
		*s++ = 128 | ((c >> 30) & 0x3F);
		*s++ = 128 | ((c >> 24) & 0x3F);
		*s++ = 128 | ((c >> 18) & 0x3F);
		*s++ = 128 | ((c >> 12) & 0x3F);
		*s++ = 128 | ((c >>  6) & 0x3F);
		*s++ = 128 | ((c >>  0) & 0x3F);
	}
#endif
#ifdef VUI_DEBUG
	else
	{
		char s[256];
		snprintf(s, 256, "Error: can't encode value as UTF-8: %d\r\n", c);
		vui_debug(s);
	}
#endif

	*s = 0;
}


void vui_set_char_t_raw(vui_state* state, unsigned int c, vui_transition next)
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

void vui_set_char_t(vui_state* state, unsigned int c, vui_transition next)
{
	if (c >= 0 && c < 0x80)
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
	else
	{
		unsigned char s[16];
		vui_codepoint_to_utf8(c, s);
		vui_set_string_t(state, s, next);
	}
}

void vui_set_range_t(vui_state* state, unsigned int c1, unsigned int c2, vui_transition next)
{
	for (unsigned int c = c1; c != c2+1; c++)
	{
		vui_set_char_t(state, c, next);
	}
}

void vui_set_string_t(vui_state* state, unsigned char* s, vui_transition next)
{
	vui_transition t = state->next[*s];

	if (t.next == NULL)
	{
#ifdef VUI_DEBUG
		char s2[512];
		snprintf(s2, 256, "vui_set_string_t(0x%lX, \"%s\", {.next = 0x%lX, .func = 0x%lX, .data = 0x%lX}): bad transition!\r\n", state, s, next.next, next.func, next.data);
		vui_debug(s2);
#endif
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
			vui_set_char_t_raw(state, *s, next);
		}
	}
}

vui_state* vui_next(vui_state* currstate, unsigned int c, int act)
{
	if (c > 0 && c < 0x80)
	{
		return vui_next_t(currstate, c, currstate->next[c], act);
	}
	else
	{
		unsigned char s[16];
		vui_codepoint_to_utf8(c, s);
		return vui_run_s(currstate, s, act);
	}
}

vui_state* vui_next_t(vui_state* currstate, unsigned int c, vui_transition t, int act)
{
	vui_state* nextstate = NULL;

	if (act == -1)
	{
		if (t.next != NULL)
		{
			return t.next;
		}
		else
		{
			return t.next = vui_state_new(NULL);
		}
	}
	else if (t.func != NULL && (act || t.next == NULL))
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


vui_state* vui_next_ss(vui_state* st, unsigned char* s, int act)
{
	for (;*s;s++)
	{
		st = vui_next(st, *s, act);
	}

	return st;
}

vui_state* vui_run_c_p(vui_state** sp, unsigned int c, int act)
{
	*sp = vui_next(*sp, c, act);
}

vui_state* vui_run_s_p(vui_state** sp, unsigned char* s, int act)
{
	return *sp = vui_run_s(*sp, s, act);
}

vui_state* vui_run_s(vui_state* st, unsigned char* s, int act)
{
	for (;*s;s++)
	{
		st = vui_next_raw(st, *s, act);
	}

	return st;
}
