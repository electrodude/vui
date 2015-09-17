#define _XOPEN_SOURCE 500

#include <string.h>

#include "debug.h"

#include "utf8.h"
#include "string.h"

#include "gc.h"

#include "statemachine.h"

int vui_iter_gen = 0;

static inline vui_state* vui_state_new_raw(void)
{
	vui_state* state = malloc(sizeof(vui_state));

	vui_gc_register(state);

	state->gv_norank = 0;

	return state;
}

vui_state* vui_state_new(void)
{
	vui_state* state = vui_state_new_raw();

	vui_transition next = vui_transition_new1(state);

	vui_state_fill(state, next);

	state->data = NULL;

	state->push = NULL;
	state->name = strdup("new");

	return state;
}

vui_state* vui_state_new_t(vui_transition transition)
{
	vui_state* state = vui_state_new_raw();

	vui_state_fill(state, transition);

	state->data = NULL;
	state->push = NULL;
	state->name = strdup("new_t");

	return state;
}

vui_state* vui_state_new_t_self(vui_transition transition)
{
	vui_state* state = vui_state_new_raw();

	transition.next = state;

	vui_state_fill(state, transition);

	state->data = NULL;
	state->push = NULL;
	state->name = strdup("new_t_self");

	return state;
}

vui_state* vui_state_new_s(vui_state* next)
{
	vui_state* state = vui_state_new_t(vui_transition_new1(next));

	free(state->name);

	vui_string name;
	vui_string_new(&name);
	vui_string_puts(&name, "to ");
	if (next != NULL)
	{
		vui_string_puts(&name, next->name);
	}
	else
	{
		vui_string_puts(&name, "NULL");
	}
	state->name = vui_string_get(&name);

	return state;
}

vui_state* vui_state_dup(vui_state* parent)
{
	vui_state* state = vui_state_new_raw();

	vui_state_cp(state, parent);

	return state;
}

void vui_state_kill(vui_state* state)
{
	if (state == NULL) return;

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STATEMACHINE)
	char s[64];
	snprintf(s, 64, "Killing state 0x%lX\r\n", state);
	vui_debug(s);
#endif

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

void vui_state_cp(vui_state* dest, const vui_state* src)
{
	for (unsigned int i=0; i<VUI_MAXSTATE; i++)
	{
		vui_set_char_t(dest, i, src->next[i]);
	}

	dest->data = src->data;
	dest->push = src->push;

	vui_string name;
	vui_string_new(&name);
	vui_string_puts(&name, "dup ");
	vui_string_puts(&name, src->name);
	dest->name = vui_string_get(&name);
}

void vui_state_fill(vui_state* dest, vui_transition t)
{
	for (unsigned int i=0; i<VUI_MAXSTATE; i++)
	{
		vui_set_char_t(dest, i, t);
	}
}

vui_state* vui_tfunc_multi(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_stack* funcs = data;

	for (size_t i = 0; i < funcs->n; i++)
	{
		vui_transition* t = funcs->s[i];
		t->func(currstate, c, act, t->data);
	}
}

vui_transition vui_transition_multi(vui_stack* funcs, vui_state* next)
{
	if (funcs->n > 1)
	{
		return (vui_transition){.next = next, .func = vui_tfunc_multi, .data = funcs};
	}
	else if (funcs->n == 1)
	{
		vui_transition t = *(vui_transition*)funcs->s[0];

		t.next = next;

		return t;
	}
	else
	{
		return vui_transition_new1(next);
	}
}

void vui_transition_multi_push(vui_stack* funcs, vui_transition t)
{
	vui_transition* t2 = malloc(sizeof(vui_transition));

	t2->next = NULL;
	t2->func = t.func;
	t2->data = t.data;

	vui_stack_push(funcs, t2);
}


typedef struct vui_transition_run_s_data
{
	vui_state* st;
	char* str;
} vui_transition_run_s_data;

static vui_state* vui_tfunc_run_s_s(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_transition_run_s_data* tdata = data;

	if (act <= 0)
	{
		if (act == VUI_ACT_GC)
		{
			vui_gc_mark(tdata->st);
		}

		return vui_run_s(tdata->st, tdata->str, act);
	}
	else
	{
		return vui_run_s(tdata->st, tdata->str, VUI_ACT_EMUL);
	}
}

vui_transition vui_transition_run_s_s(vui_state* st, char* str)
{
	vui_transition_run_s_data* data = malloc(sizeof(vui_transition_run_s_data));
	data->st = st;
	data->str = str;

	return (vui_transition){.next = NULL, .func = vui_tfunc_run_s_s, .data = data};
}


static vui_state* vui_tfunc_run_c_s(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_state* other = data;

	if (other == currstate) // evade stack overflow
	{
		return currstate;
	}

	if (act <= 0)
	{
		if (act == VUI_ACT_GC)
		{
			vui_gc_mark(other);
		}

		return vui_next_t(currstate, c, other->next[c], act);
	}
	else
	{
		return vui_next_t(currstate, c, other->next[c], VUI_ACT_EMUL);
	}
}

vui_transition vui_transition_run_c_s(vui_state* other)
{
	return (vui_transition){.next = NULL, .func = vui_tfunc_run_c_s, .data = other};
}


static vui_state* vui_tfunc_run_c_t(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_transition* t = data;

	if (act <= 0)
	{
		return vui_next_t(currstate, c, *t, act);
	}
	else
	{
		return vui_next_t(currstate, c, *t, VUI_ACT_EMUL);
	}
}

vui_transition vui_transition_run_c_t(vui_transition* t)
{
	return (vui_transition){.next = NULL, .func = vui_tfunc_run_c_t, .data = t};
}



void vui_set_char_t(vui_state* state, unsigned char c, vui_transition next)
{
	state->next[c] = next;
}

void vui_set_char_t_u(vui_state* state, unsigned int c, vui_transition next)
{
	if (c >= 0 && c < 0x80)
	{
		vui_set_char_t(state, c, next);
	}
	else
	{
		unsigned char s[16];
		vui_utf8_encode(c, s);
		vui_set_string_t_nocall(state, s, next);
	}
}

void vui_set_range_t(vui_state* state, unsigned char c1, unsigned char c2, vui_transition next)
{
	for (unsigned char c = c1; c != c2+1; c++)
	{
		vui_set_char_t(state, c, next);
	}
}

void vui_set_range_t_u(vui_state* state, unsigned int c1, unsigned int c2, vui_transition next)
{
	for (unsigned int c = c1; c != c2+1; c++)
	{
		vui_set_char_t_u(state, c, next);
	}
}

void vui_set_string_t_nocall(vui_state* state, unsigned char* s, vui_transition next)
{
	vui_transition t = state->next[*s];

	if (t.next == NULL)
	{
#if defined(VUI_DEBUG)
		char s2[512];
		snprintf(s2, 512, "vui_set_string_t_nocall(0x%lX, \"%s\", {.next = 0x%lX, .func = 0x%lX, .data = 0x%lX}): bad transition!\r\n", state, s, next.next, next.func, next.data);
		vui_debug(s2);
#endif
	}
	else
	{
		if (s[1])
		{
			vui_transition t = state->next[*s];
			vui_state* state2 = t.next;

			if (state2 == NULL) return;

			vui_state* nextst = vui_state_dup(state2);

			vui_set_char_t(state, *s, vui_transition_new1(nextst));

			vui_string name;
			vui_string_new(&name);
			vui_string_puts(&name, "string ");
			if (s[1] < 128)
			{
				vui_string_putc(&name, s[1]);
			}
			else
			{
				vui_string_puts(&name, "??");
			}
			free(nextst->name);
			nextst->name = vui_string_get(&name);

			vui_set_string_t_nocall(nextst, s+1, next);
		}
		else
		{
			vui_set_char_t(state, *s, next);
		}
	}
}

void vui_set_string_t(vui_state* state, unsigned char* s, vui_transition next)
{
	vui_transition t = state->next[*s];

	if (t.next == NULL)
	{
#if defined(VUI_DEBUG)
		char s2[512];
		snprintf(s2, 512, "vui_set_string_t(0x%lX, \"%s\", {.next = 0x%lX, .func = 0x%lX, .data = 0x%lX}): bad transition!\r\n", state, s, next.next, next.func, next.data);
		vui_debug(s2);
#endif
	}
	else
	{
		if (s[1])
		{
			vui_transition t = state->next[*s];
			vui_state* state2 = t.next;

			if (state2 == NULL) return;

			vui_state* nextst = vui_state_dup(state2);

			t.next = nextst;

			vui_set_char_t(state, *s, t);

			vui_string name;
			vui_string_new(&name);
			if (s[1] < 128)
			{
				vui_string_putc(&name, s[1]);
			}
			else
			{
				vui_string_puts(&name, "??");
			}
			free(nextst->name);
			nextst->name = vui_string_get(&name);
			
			vui_set_string_t(nextst, s+1, next);
		}
		else
		{
			vui_set_char_t(state, *s, next);
		}
	}
}

void vui_set_string_t_mid(vui_state* state, unsigned char* s, vui_transition mid, vui_transition next)
{
	vui_transition t = state->next[*s];

	if (t.next == NULL)
	{
#if defined(VUI_DEBUG)
		char s2[512];
		snprintf(s2, 512, "vui_set_string_t_mid(0x%lX, \"%s\", {.next = 0x%lX, .func = 0x%lX, .data = 0x%lX}): bad transition!\r\n", state, s, next.next, next.func, next.data);
		vui_debug(s2);
#endif
	}
	else
	{
		if (s[1])
		{
			vui_transition t = state->next[*s];
			vui_state* state2 = t.next;

			if (state2 == NULL) return;

			vui_state* nextst = vui_state_dup(state2);

			t = mid;

			t.next = nextst;

			vui_set_char_t(state, *s, t);

			vui_string name;
			vui_string_new(&name);
			if (s[1] < 128)
			{
				vui_string_putc(&name, s[1]);
			}
			else
			{
				vui_string_puts(&name, "??");
			}
			free(nextst->name);
			nextst->name = vui_string_get(&name);

			vui_set_string_t_mid(nextst, s+1, mid, next);
		}
		else
		{
			vui_set_char_t(state, *s, next);
		}
	}
}

vui_state* vui_next_u(vui_state* currstate, unsigned int c, int act)
{
	if (c >= 0 && c < 0x80)
	{
		return vui_next(currstate, c, act);
	}
	else
	{
		unsigned char s[16];
		vui_utf8_encode(c, s);
#if 1
		unsigned char* s2 = s;

		for (;*s2;s2++)
		{
			currstate = vui_next_t(currstate, c, currstate->next[*s2], act);
		}

		return currstate;
#else
		return vui_run_s(currstate, s, act);
#endif
	}
}

vui_state* vui_next_t(vui_state* currstate, unsigned int c, vui_transition t, int act)
{
	vui_state* nextstate = NULL;


	if (act == VUI_ACT_NOCALL)
	{
		if (t.next != NULL)
		{
			return t.next;
		}
		else
		{
			return t.next = vui_state_new();
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

	if (nextstate != NULL && act > 0)
	{
		if (nextstate->push != NULL)
		{
			vui_stack_push_nodup(nextstate->push, nextstate);
		}
	}

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STATEMACHINE)
	if (act == VUI_ACT_RUN)
	{
		char s[64];
		snprintf(s, 64, "destination: (0x%lX)\r\n", nextstate);
		vui_debug(s);
	}
#endif

	return nextstate;
}


vui_state* vui_run_c_p(vui_state** sp, unsigned char c, int act)
{
	return *sp = vui_next(*sp, c, act);
}

vui_state* vui_run_c_p_u(vui_state** sp, unsigned int c, int act)
{
	return *sp = vui_next_u(*sp, c, act);
}

vui_state* vui_run_s_p(vui_state** sp, unsigned char* s, int act)
{
	return *sp = vui_run_s(*sp, s, act);
}

vui_state* vui_run_s(vui_state* st, unsigned char* s, int act)
{
	for (;*s;s++)
	{
		st = vui_next(st, *s, act);
	}

	return st;
}


// state stack

static vui_state* vui_tfunc_stack_push(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_stack* stk = data;

	if (act <= 0) return NULL;

	vui_stack_push(stk, currstate);

	return NULL;
}

vui_transition vui_transition_stack_push(vui_stack* stk, vui_state* next)
{
	return (vui_transition){.next = next, .func = vui_tfunc_stack_push, .data = stk};
}

static vui_state* vui_tfunc_stack_pop(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_stack* stk = data;

	if (act <= 0)
	{
		return vui_stack_peek(stk);
	}
	else
	{
		return vui_stack_pop(stk);
	}
}

vui_transition vui_transition_stack_pop(vui_stack* stk)
{
	return (vui_transition){.next = NULL, .func = vui_tfunc_stack_pop, .data = stk};
}
