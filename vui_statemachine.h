#ifndef VUI_STATEMACHINE_H
#define VUI_STATEMACHINE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "vui_string.h"
#include "vui_stack.h"

#include "vui_gc.h"

#include "vui_gc_ptr.h"

#define VUI_STATE_BITS 8
#define VUI_MAXSTATE (1 << VUI_STATE_BITS)


#define VUI_ACT_KILL    -2
#define VUI_ACT_GC_MARK -1
#define VUI_ACT_TEST     0
#define VUI_ACT_RUN      1
#define VUI_ACT_EMUL     2

typedef struct vui_state vui_state;

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
	vui_gc_header gc;

	vui_state* next;

	vui_transition_callback func;

	void* data;

	int iter_id;
	int iter_gen;
	union
	{
		vui_state* st;
		size_t off;
		void* data;
	} iter_data;

	int running;

} vui_transition;

typedef struct vui_state
{
	vui_gc_header gc;

	vui_transition* next[VUI_MAXSTATE];

	vui_stack* push;

	int gv_norank;

	void* data;

	vui_string name;

	int iter_id;
	int iter_gen;
	union
	{
		vui_state* st;
		size_t off;
		void* data;
	} iter_data;

} vui_state;

extern int vui_iter_gen;


vui_state* vui_state_new(void);
vui_state* vui_state_new_t(vui_transition* next);
vui_state* vui_state_new_t_self(vui_transition* transition);
vui_state* vui_state_new_s(vui_state* next);
vui_state* vui_state_dup(vui_state* parent);

void vui_state_replace(vui_state* state, vui_transition* search, vui_transition* replacement);

void vui_state_cp(vui_state* dest, const vui_state* src);
void vui_state_fill(vui_state* dest, vui_transition* t);

static inline char* vui_state_name(vui_state* st)
{
	if (st == NULL) return NULL;
	return vui_string_get(&st->name);
}


#define vui_state_index(st, c) (st->next[(unsigned char)c])



vui_transition* vui_transition_new(vui_state* next, vui_transition_callback func, void* data);
vui_transition* vui_transition_dup(vui_transition* parent);
void vui_transition_kill(vui_transition* t);

static inline vui_transition* vui_transition_new0(void)
{
	return vui_transition_new(NULL, NULL, NULL);
}

static inline vui_transition* vui_transition_new1(vui_state* next)
{
	return vui_transition_new(next, NULL, NULL);
}

static inline vui_transition* vui_transition_new2(vui_transition_callback func, void* data)
{
	return vui_transition_new(NULL, func, data);
}

static inline vui_transition* vui_transition_new3(vui_state* next, vui_transition_callback func, void* data)
{
	return vui_transition_new(next, func, data);
}


// Run multiple other transitions
// Takes a vui_gc_ptr to a vui_stack of transitions to run
vui_state* vui_tfunc_multi(vui_state* currstate, unsigned int c, int act, void* data);
vui_transition* vui_transition_multi(vui_gc_ptr* funcs, vui_state* next);

vui_transition* vui_transition_multi_stage(vui_transition* t);

vui_state* vui_tfunc_run_s_s(vui_state* currstate, unsigned int c, int act, void* data);
vui_transition* vui_transition_run_str_s(vui_state* st, vui_string* str);
vui_transition* vui_transition_run_s_s(vui_state* st, char* str);

// Do what another state would do
vui_state* vui_tfunc_run_c_s(vui_state* currstate, unsigned int c, int act, void* data);
static inline vui_transition* vui_transition_run_c_s(vui_state* other)
{
	return vui_transition_new2(vui_tfunc_run_c_s, other);
}

// Advance a different state machine
// st_ptr_ptr: a pointer to where vui_transition_run_c_p should put the pointer
//  to the transition being abused as a state pointer container
// Transitions are garbage collected and have a pointer to a state; instead of
//  making a new object that's garbage collected and has a pointer to a state,
//  I'm just reusing transitions.
// You should call vui_gc_incr, vui_gc_decr, and vui_gc_mark as appropriate if
//  you keep a reference to the transition.
vui_state* vui_tfunc_run_c_p(vui_state* currstate, unsigned int c, int act, void* data);
vui_transition* vui_transition_run_c_p(vui_transition** st_ptr_ptr);


// useless?
vui_state* vui_tfunc_run_c_t(vui_state* currstate, unsigned int c, int act, void* data);
static inline vui_transition* vui_transition_run_c_t(vui_transition* t)
{
	return vui_transition_new2(vui_tfunc_run_c_t, t);
}


void vui_codepoint_to_utf8(unsigned int c, unsigned char* s);

void vui_set_char_t(vui_state* state, unsigned char c, vui_transition* next);
static inline void vui_set_char_s(vui_state* state, unsigned char c, vui_state* next)
{
	vui_set_char_t(state, c, vui_transition_new1(next));
}

void vui_set_char_t_u(vui_state* state, unsigned int c, vui_transition* next);
static inline void vui_set_char_s_u(vui_state* state, unsigned int c, vui_state* next)
{
	vui_set_char_t_u(state, c, vui_transition_new1(next));
}

void vui_set_range_t(vui_state* state, unsigned char c1, unsigned char c2, vui_transition* next);
static inline void vui_set_range_s(vui_state* state, unsigned char c1, unsigned char c2, vui_state* next)
{
	vui_set_range_t(state, c1, c2, vui_transition_new1(next));
}

void vui_set_range_t_u(vui_state* state, unsigned int c1, unsigned int c2, vui_transition* next);
static inline void vui_set_range_s_u(vui_state* state, unsigned int c1, unsigned int c2, vui_state* next)
{
	vui_set_range_t_u(state, c1, c2, vui_transition_new1(next));
}

void vui_set_string_t_preserve(vui_state* state, const char* s, vui_transition* next);
static inline void vui_set_string_s_preserve(vui_state* state, const char* s, vui_state* next)
{
	vui_set_string_t_preserve(state, s, vui_transition_new1(next));
}

void vui_set_buf_t(vui_state* state, const char* s, size_t len, vui_transition* next);
static inline void vui_set_buf_s(vui_state* state, const char* s, size_t len, vui_state* next)
{
	vui_set_buf_t(state, s, len, vui_transition_new1(next));
}

void vui_set_string_t_mid(vui_state* state, const char* s, vui_transition* mid, vui_transition* next);

//void vui_set_string_t(vui_state* state, char* s, vui_transition* next);
#define vui_set_string_t(state, s, next) vui_set_string_t_mid(state, s, vui_transition_new0(), next)
static inline void vui_set_string_s(vui_state* state, const char* s, vui_state* next)
{
	vui_set_string_t(state, s, vui_transition_new1(next));
}



vui_state* vui_next_t(vui_state* currstate, unsigned int c, vui_transition* t, int act);
static inline vui_state* vui_next(vui_state* currstate, unsigned char c, int act)
{
	return vui_next_t(currstate, c, vui_state_index(currstate, c), act);
}

vui_state* vui_next_u(vui_state* currstate, unsigned int c, int act);

vui_state* vui_run_c_p(vui_state** sp, unsigned char c, int act);
vui_state* vui_run_c_p_u(vui_state** sp, unsigned int c, int act);

vui_state* vui_run_s_p(vui_state** sp, char* s, int act);
vui_state* vui_run_s(vui_state* st, char* s, int act);

vui_state* vui_run_buf_p(vui_state** sp, unsigned char* buf, size_t len, int act);
vui_state* vui_run_buf(vui_state* st, unsigned char* buf, size_t len, int act);

static inline vui_state* vui_run_str_p(vui_state** sp, vui_string* str, int act)
{
	return vui_run_buf_p(sp, (unsigned char*)str->s, str->n, act);
}

static inline vui_state* vui_run_str(vui_state* st, vui_string* str, int act)
{
	return vui_run_buf(st, (unsigned char*)str->s, str->n, act);
}

vui_state* vui_lookup_s(vui_state* st, char* s);

vui_state* vui_lookup_buf(vui_state* st, unsigned char* buf, size_t len);

static inline vui_state* vui_lookup_str(vui_state* st, vui_string* str)
{
	return vui_lookup_buf(st, (unsigned char*)str->s, str->n);
}


// state stack
vui_state* vui_tfunc_stack_push(vui_state* currstate, unsigned int c, int act, void* data);
static inline vui_transition* vui_transition_stack_push(vui_stack* stk, vui_state* next)
{
	return vui_transition_new3(next, vui_tfunc_stack_push, stk);
}

vui_state* vui_tfunc_stack_pop(vui_state* currstate, unsigned int c, int act, void* data);
static inline vui_transition* vui_transition_stack_pop(vui_stack* stk)
{
	return vui_transition_new2(vui_tfunc_stack_pop, stk);
}


#ifdef __cplusplus
}
#endif

#endif
