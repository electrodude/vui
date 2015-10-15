#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "vui_stack.h"

#define VUI_STATE_BITS 8
#define VUI_MAXSTATE (1 << VUI_STATE_BITS)

typedef struct vui_state vui_state;

#define VUI_ACT_GC     -2
#define VUI_ACT_NOCALL -1
#define VUI_ACT_TEST    0
#define VUI_ACT_RUN     1
#define VUI_ACT_EMUL    2

/*
 * \param currstate previous state
 * \param act whether side effects should happen (as opposed to just returning the next state)
 * \param data extra data (next state, etc.)
 * \return next state (ignored if transition.next != NULL)
 *
 */
typedef vui_state* (*vui_transition_callback)(vui_state* currstate, unsigned int c, int act, void* data);

typedef struct vui_transition
{
	vui_state* next;

	vui_transition_callback func;

	void* data;

} vui_transition;

typedef struct vui_state
{
	vui_transition next[VUI_MAXSTATE];

	unsigned int root;

	vui_stack* push;

	int gv_norank;

	void* data;

	char* name;

	int iter_id;
	int iter_gen;
	vui_state* gc_next;
	vui_state* iter_other;

} vui_state;

extern int vui_iter_gen;


vui_state* vui_state_new(void);
vui_state* vui_state_new_t(vui_transition next);
vui_state* vui_state_new_t_self(vui_transition transition);
vui_state* vui_state_new_s(vui_state* next);
vui_state* vui_state_dup(vui_state* parent);

void vui_state_replace(vui_state* state, vui_transition search, vui_transition replacement);

void vui_state_cp(vui_state* dest, const vui_state* src);
void vui_state_fill(vui_state* dest, vui_transition t);


static inline vui_transition vui_transition_new1(vui_state* next)
{
	return (vui_transition){.next = next, .func = NULL, .data = NULL};
}

static inline vui_transition vui_transition_new2(vui_transition_callback func, void* data)
{
	return (vui_transition){.next = NULL, .func = func, .data = data};
}

static inline vui_transition vui_transition_new3(vui_state* next, vui_transition_callback func, void* data)
{
	return (vui_transition){.next = next, .func = func, .data = data};
}

vui_state* vui_tfunc_multi(vui_state* currstate, unsigned int c, int act, void* data);
vui_transition vui_transition_multi(vui_stack* funcs, vui_state* next);

void vui_transition_multi_push(vui_stack* funcs, vui_transition t);

vui_state* vui_tfunc_run_s_s(vui_state* currstate, unsigned int c, int act, void* data);
vui_transition vui_transition_run_s_s(vui_state* st, char* str);

vui_state* vui_tfunc_run_c_s(vui_state* currstate, unsigned int c, int act, void* data);
static inline vui_transition vui_transition_run_c_s(vui_state* other)
{
	return (vui_transition){.next = NULL, .func = vui_tfunc_run_c_s, .data = other};
}

vui_state* vui_tfunc_run_c_t(vui_state* currstate, unsigned int c, int act, void* data);
static inline vui_transition vui_transition_run_c_t(vui_transition* t)
{
	return (vui_transition){.next = NULL, .func = vui_tfunc_run_c_t, .data = t};
}


void vui_codepoint_to_utf8(unsigned int c, unsigned char* s);

void vui_set_char_t(vui_state* state, unsigned char c, vui_transition next);
static inline void vui_set_char_s(vui_state* state, unsigned char c, vui_state* next)
{
	vui_set_char_t(state, c, vui_transition_new1(next));
}

void vui_set_char_t_u(vui_state* state, unsigned int c, vui_transition next);
static inline void vui_set_char_s_u(vui_state* state, unsigned int c, vui_state* next)
{
	vui_set_char_t_u(state, c, vui_transition_new1(next));
}

void vui_set_range_t(vui_state* state, unsigned char c1, unsigned char c2, vui_transition next);
static inline void vui_set_range_s(vui_state* state, unsigned char c1, unsigned char c2, vui_state* next)
{
	vui_set_range_t(state, c1, c2, vui_transition_new1(next));
}

void vui_set_range_t_u(vui_state* state, unsigned int c1, unsigned int c2, vui_transition next);
static inline void vui_set_range_s_u(vui_state* state, unsigned int c1, unsigned int c2, vui_state* next)
{
	vui_set_range_t_u(state, c1, c2, vui_transition_new1(next));
}

void vui_set_string_t_nocall(vui_state* state, unsigned char* s, vui_transition next);
static inline void vui_set_string_s_nocall(vui_state* state, unsigned char* s, vui_state* next)
{
	vui_set_string_t_nocall(state, s, vui_transition_new1(next));
}

void vui_set_string_t(vui_state* state, unsigned char* s, vui_transition next);
static inline void vui_set_string_s(vui_state* state, unsigned char* s, vui_state* next)
{
	vui_set_string_t(state, s, vui_transition_new1(next));
}

void vui_set_buf_t(vui_state* state, unsigned char* s, size_t len, vui_transition next);
static inline void vui_set_buf_s(vui_state* state, unsigned char* s, size_t len, vui_state* next)
{
	vui_set_buf_t(state, s, len, vui_transition_new1(next));
}

void vui_set_string_t_mid(vui_state* state, unsigned char* s, vui_transition mid, vui_transition next);



vui_state* vui_next_t(vui_state* currstate, unsigned int c, vui_transition t, int act);
static inline vui_state* vui_next(vui_state* currstate, unsigned char c, int act)
{
	return vui_next_t(currstate, c, currstate->next[c], act);
}

vui_state* vui_next_u(vui_state* currstate, unsigned int c, int act);

vui_state* vui_run_c_p(vui_state** sp, unsigned char c, int act);
vui_state* vui_run_c_p_u(vui_state** sp, unsigned int c, int act);

vui_state* vui_run_s_p(vui_state** sp, unsigned char* s, int act);
vui_state* vui_run_s(vui_state* st, unsigned char* s, int act);

vui_state* vui_run_buf_p(vui_state** sp, unsigned char* buf, size_t len, int act);
vui_state* vui_run_buf(vui_state* st, unsigned char* buf, size_t len, int act);

vui_state* vui_tfunc_stack_push(vui_state* currstate, unsigned int c, int act, void* data);
static inline vui_transition vui_transition_stack_push(vui_stack* stk, vui_state* next)
{
	return (vui_transition){.next = next, .func = vui_tfunc_stack_push, .data = stk};
}

vui_state* vui_tfunc_stack_pop(vui_state* currstate, unsigned int c, int act, void* data);
static inline vui_transition vui_transition_stack_pop(vui_stack* stk)
{
	return (vui_transition){.next = NULL, .func = vui_tfunc_stack_pop, .data = stk};
}


#ifdef __cplusplus
}
#endif
