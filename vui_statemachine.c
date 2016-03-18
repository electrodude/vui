#define _XOPEN_SOURCE 500

#include <string.h>

#include "vui_debug.h"

#include "vui_utf8.h"
#include "vui_string.h"

#include "vui_statemachine.h"

int vui_iter_gen = 0;

static void vui_state_gc_dtor(void* obj, vui_gc_dtor_mode mode);

static inline vui_state* vui_state_new_raw(void)
{
	vui_state* state = malloc(sizeof(vui_state));

	state->gv_norank = 0;

	vui_string_new_at(&state->name);

	vui_gc_register(state, vui_state_gc_dtor);

	return state;
}

vui_state* vui_state_new(void)
{
	vui_state* state = vui_state_new_raw();

	vui_transition next = vui_transition_new1(state);

	vui_state_fill(state, next);

	state->data = NULL;

	state->push = NULL;
	vui_string_puts(&state->name, "new");

	return state;
}

vui_state* vui_state_new_t(vui_transition transition)
{
	vui_state* state = vui_state_new_raw();

	vui_state_fill(state, transition);

	state->data = NULL;
	state->push = NULL;
	vui_string_puts(&state->name, "new_t");

	return state;
}

vui_state* vui_state_new_t_self(vui_transition transition)
{
	vui_state* state = vui_state_new_raw();

	transition.next = state;

	vui_state_fill(state, transition);

	state->data = NULL;
	state->push = NULL;

	vui_string_puts(&state->name, "new_t_self");

	return state;
}

vui_state* vui_state_new_s(vui_state* next)
{
	vui_state* state = vui_state_new_t(vui_transition_new1(next));

	vui_string* name = &state->name;
	vui_string_reset(name);
	vui_string_puts(name, "to ");
	if (next != NULL)
	{
		vui_string_append(name, &next->name);
	}
	else
	{
		vui_string_puts(name, "NULL");
	}

	return state;
}

vui_state* vui_state_dup(vui_state* parent)
{
	vui_state* state = vui_state_new_raw();

	vui_state_cp(state, parent);

	return state;
}

static void vui_state_kill(vui_state* state)
{
	if (state == NULL) return;

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STATEMACHINE)
	vui_debugf("Killing state: \"%s\" (%p)\n", vui_state_name(state), state);
#endif

	vui_string_dtor(&state->name);

	free(state);
}

static void vui_state_gc_dtor(void* obj, vui_gc_dtor_mode mode)
{
	vui_state* st = (vui_state*)obj;

	if (mode == VUI_GC_DTOR_MARK)
	{
		for (unsigned int i=0; i < VUI_MAXSTATE; i++)
		{
			vui_gc_mark(vui_next(st, i, VUI_ACT_GC));
		}

		// if the gc is running, this state's name is probably final
		vui_string_shrink(&st->name);
	}
	else if (mode == VUI_GC_DTOR_SWEEP)
	{
		vui_state_kill(st);
	}
	else if (mode == VUI_GC_DTOR_DESCRIBE)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_GC)
		vui_debugf("vui_gc: \"%s\" (%p)\n", vui_state_name(st), st);
#endif
	}
}

void vui_state_replace(vui_state* state, vui_transition search, vui_transition replacement)
{
	for (unsigned int i=0; i<VUI_MAXSTATE; i++)
	{
		vui_transition t = vui_state_index(state, i);

		if (t.func == search.func && t.data == search.data)
		{
			vui_set_char_t(state, i, replacement);
		}
	}
}

static vui_state vui_dummy_zero_state = {};

void vui_state_cp(vui_state* dest, const vui_state* src)
{
	if (src == NULL)
	{
		src = &vui_dummy_zero_state;

		vui_string* name = &dest->name;
		vui_string_reset(name);
		vui_string_puts(name, "NULL");
	}
	else
	{
		vui_string* name = &dest->name;
		vui_string_reset(name);
		vui_string_append(name, &src->name);
	}

	for (unsigned int i=0; i<VUI_MAXSTATE; i++)
	{
		vui_set_char_t(dest, i, vui_state_index(src, i));
	}

	dest->data = src->data;
	dest->push = src->push;
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

	return NULL;
}

vui_transition vui_transition_multi(vui_stack* funcs, vui_state* next)
{
	if (funcs->n > 1)
	{
		return vui_transition_new3(next, vui_tfunc_multi, funcs);
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

vui_transition* vui_transition_multi_stage(vui_transition t)
{
	vui_transition* t2 = malloc(sizeof(vui_transition));

	t2->next = NULL;
	t2->func = t.func;
	t2->data = t.data;

	return t2;
}


typedef struct vui_transition_run_s_data
{
	vui_state* st;
	vui_string str;
} vui_transition_run_s_data;

vui_state* vui_tfunc_run_s_s(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_transition_run_s_data* tdata = data;

	if (act == VUI_ACT_GC)
	{
		vui_gc_mark(tdata->st);
	}

	if (act <= 0)
	{
		return vui_run_str(tdata->st, &tdata->str, act);
	}
	else
	{
		return vui_run_str(tdata->st, &tdata->str, VUI_ACT_EMUL);
	}
}

vui_transition vui_transition_run_str_s(vui_state* st, vui_string* str)
{
	vui_transition_run_s_data* data = malloc(sizeof(vui_transition_run_s_data));
	data->st = st;
	vui_string_dup_at(&data->str, str);
	vui_string_shrink(&data->str);

	return vui_transition_new2(vui_tfunc_run_s_s, data);
}

vui_transition vui_transition_run_s_s(vui_state* st, char* str)
{
	vui_transition_run_s_data* data = malloc(sizeof(vui_transition_run_s_data));
	data->st = st;
	vui_string_new_str_at(&data->str, str);
	vui_string_shrink(&data->str);

	return vui_transition_new2(vui_tfunc_run_s_s, data);
}


vui_state* vui_tfunc_run_c_s(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_state* other = data;

	if (act == VUI_ACT_GC)
	{
		vui_gc_mark(other);
	}

	if (other == currstate) // evade stack overflow
	{
		return currstate;
	}

	if (act <= 0)
	{
		return vui_next_t(currstate, c, vui_state_index(other, c), act);
	}
	else
	{
		return vui_next_t(currstate, c, vui_state_index(other, c), VUI_ACT_EMUL);
	}
}


vui_state* vui_tfunc_run_c_t(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_transition* t = data;

	if (act == VUI_ACT_GC)
	{
		if (t->next != NULL)
		{
			vui_gc_mark(t->next);
		}
	}

	if (act <= 0)
	{
		return vui_next_t(currstate, c, *t, act);
	}
	else
	{
		return vui_next_t(currstate, c, *t, VUI_ACT_EMUL);
	}
}



void vui_set_char_t(vui_state* state, unsigned char c, vui_transition next)
{
	vui_state_index(state, c) = next;
}

void vui_set_char_t_u(vui_state* state, unsigned int c, vui_transition next)
{
	if (c >= 0 && c < 0x80)
	{
		vui_set_char_t(state, c, next);
	}
	else
	{
		char s[16];
		vui_utf8_encode(c, s);
		vui_set_string_t(state, s, next);
	}
}

void vui_set_range_t(vui_state* state, unsigned char c1, unsigned char c2, vui_transition next)
{
	for (unsigned int c = c1; c != c2+1; c++)
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

void vui_set_buf_t(vui_state* state, char* s, size_t len, vui_transition next)
{
	while (len > 1)
	{
		vui_transition t = vui_state_index(state, *s);
		vui_state* state2 = t.next;

		if (state2 == NULL) state2 = state;

		vui_state* nextst = vui_state_dup(state2);

		nextst->push = NULL;



		t.next = nextst;

		vui_set_char_t(state, *s, t);

		vui_string* name = &nextst->name;
		vui_string_reset(name);
		vui_string_puts(name, "string ");
		vui_string_putq(name, s[1]);

		s++;
		len--;
		state = nextst;
	}

	vui_set_char_t(state, *s, next);
}

void vui_set_string_t_preserve(vui_state* state, char* s, vui_transition next)
{
	while (s[1])
	{
		vui_transition t = vui_state_index(state, *s);
		vui_state* state2 = t.next;

		if (state2 == NULL) state2 = state;

		vui_state* nextst = vui_state_dup(state2);

		nextst->push = NULL;



		t.next = nextst;

		vui_set_char_t(state, *s, t);

		vui_string* name = &nextst->name;
		vui_string_reset(name);
		vui_string_puts(name, "string ");
		vui_string_putq(name, s[1]);

		s++;
		state = nextst;
	}

	vui_set_char_t(state, *s, next);
}

void vui_set_string_t_mid(vui_state* state, char* s, vui_transition mid, vui_transition next)
{
	while (s[1])
	{
		vui_transition t = vui_state_index(state, *s);
		vui_state* state2 = t.next;

		if (state2 == NULL) state2 = state;

		vui_state* nextst = vui_state_dup(state2);

		nextst->push = NULL;

		t = mid;

		t.next = nextst;

		vui_set_char_t(state, *s, t);

		vui_string* name = &nextst->name;
		vui_string_reset(name);
		vui_string_puts(name, "string ");
		vui_string_putq(name, s[1]);

		s++;
		state = nextst;
	}

	vui_set_char_t(state, *s, next);
}

vui_state* vui_next_u(vui_state* currstate, unsigned int c, int act)
{
	if (c >= 0 && c < 0x80)
	{
		return vui_next(currstate, c, act);
	}
	else
	{
		char s[16];
		vui_utf8_encode(c, s);
#if 1
		char* s2 = s;

		// give the full Unicode codepoint to every byte's callback
		//  function, not just the one byte
		for (;*s2;s2++)
		{
			currstate = vui_next_t(currstate, c, vui_state_index(currstate, *s2), act);
		}

		return currstate;
#else
		return vui_run_s(currstate, s, act);
#endif
	}
}

vui_state* vui_next_t(vui_state* currstate, unsigned int c, vui_transition t, int act)
{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STATEMACHINE)
	if (act == VUI_ACT_RUN)
	{
		vui_debugf("source: \"%s\" (%p)\n", vui_state_name(currstate), currstate);
	}
#endif

	vui_state* nextstate = NULL;


	if (t.func != NULL && (act > 0 || t.next == NULL))
	{
		vui_state* retnext = t.func(currstate, c, act, t.data);
		nextstate =  t.next != NULL ? t.next : retnext;
	}
	else
	{
		nextstate = t.next;
	}

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STATEMACHINE)
	if (act == VUI_ACT_RUN)
	{
		vui_debugf("destination: \"%s\" (%p)\n", vui_state_name(nextstate), nextstate);
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

vui_state* vui_run_s_p(vui_state** sp, char* s, int act)
{
	return *sp = vui_run_s(*sp, s, act);
}

vui_state* vui_run_s(vui_state* st, char* s, int act)
{
	for (;*s && st != NULL;s++)
	{
		st = vui_next(st, (unsigned char)*s, act);
	}

	return st;
}

vui_state* vui_run_buf_p(vui_state** sp, unsigned char* buf, size_t len, int act)
{
	return *sp = vui_run_buf(*sp, buf, len, act);
}

vui_state* vui_run_buf(vui_state* st, unsigned char* buf, size_t len, int act)
{
	for (size_t i = 0; i < len; i++)
	{
		st = vui_next(st, buf[i], act);
	}

	return st;
}


vui_state* vui_lookup_s(vui_state* st, char* s)
{
	for (;*s;s++)
	{
		vui_state* nextst = vui_next(st, (unsigned char)*s, VUI_ACT_RUN);

		if (nextst == NULL)
		{
			nextst = vui_state_new_s(NULL);
			vui_string_reset(&st->name);
			vui_string_puts(&st->name, "lookup ");
			vui_string_puts(&st->name, s);

			vui_set_char_s(st, (unsigned char)*s, nextst);
		}

		st = nextst;
	}

	return st;
}

vui_state* vui_lookup_buf(vui_state* st, unsigned char* buf, size_t len)
{
	for (size_t i = 0; i < len; i++)
	{
		vui_state* nextst = vui_next(st, buf[i], VUI_ACT_RUN);

		if (nextst == NULL)
		{
			nextst = vui_state_new_s(NULL);
			vui_string_reset(&st->name);
			vui_string_puts(&st->name, "lookup ");
			vui_string_putq(&st->name, buf[i]);

			vui_set_char_s(st, buf[i], nextst);
		}

		st = nextst;
	}

	return st;
}


// state stack

vui_state* vui_tfunc_stack_push(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_stack* stk = data;

	if (act <= 0) return NULL;

	vui_state_stack_push(stk, currstate);

	return NULL;
}

vui_state* vui_tfunc_stack_pop(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_stack* stk = data;

	if (act <= 0)
	{
		return vui_stack_peek(stk);
	}
	else
	{
		return vui_state_stack_pop(stk);
	}
}

void vui_state_stack_elem_dtor(vui_state* st)
{
	vui_gc_decr(st);
}

vui_stack* vui_state_stack_convert(vui_stack* stk)
{
	for (size_t i = 0; i < stk->n; i++)
	{
		vui_gc_incr((vui_state*)stk->s[i]);
	}

	stk->dtor = (void(*)(void*))vui_state_stack_elem_dtor;

	if (vui_stack_def_get(stk) != NULL)
	{
		vui_gc_incr((vui_state*)vui_stack_def_get(stk));
	}

	return stk;
}

void vui_state_stack_push(vui_stack* stk, vui_state* st)
{
	if (st != NULL)
	{
		vui_gc_incr(st);
	}

	vui_stack_push(stk, st);
}

void vui_state_stack_push_nodup(vui_stack* stk, vui_state* st)
{
	if (st == vui_stack_peek(stk)) return;

	vui_state_stack_push(stk, st);
}

vui_state* vui_state_stack_pop(vui_stack* stk)
{
	vui_state* st = vui_stack_pop(stk);

	if (st != NULL)
	{
		vui_gc_decr(st);
	}

	return st;
}

void vui_state_stack_set_def(vui_stack* stk, vui_state* def)
{
	vui_state* def_old = vui_stack_def_get(stk);

	if (def_old != NULL)
	{
		vui_gc_decr(def_old);
	}

	vui_stack_def_set(stk, def);

	if (def != NULL)
	{
		vui_gc_incr(def);
	}
}
