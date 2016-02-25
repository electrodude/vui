#ifndef VUI_TRANSLATOR_H
#define VUI_TRANSLATOR_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "vui_stack.h"
#include "vui_string.h"
#include "vui_statemachine.h"

#include "vui_fragment.h"

typedef struct vui_translator
{
	vui_state* st_start;
	vui_stack* stk;
	vui_string* str;
} vui_translator;

extern vui_state* vui_translator_deadend;

extern vui_translator* vui_translator_identity;

void vui_translator_init(void);
vui_translator* vui_translator_new(void);
static inline vui_translator* vui_translator_new2(vui_translator* tr, vui_state* st_start)
{
	tr->st_start = st_start;
	tr->st_start->root++;

	return tr;
}

void vui_translator_kill(vui_translator* tr);

vui_stack* vui_translator_run(vui_translator* tr, char* in);

// transition helpers
vui_state* vui_translator_tfunc_push(vui_state* currstate, unsigned int c, int act, void* data);
vui_state* vui_translator_tfunc_putc(vui_state* currstate, unsigned int c, int act, void* data);
vui_state* vui_translator_tfunc_pop(vui_state* currstate, unsigned int c, int act, void* data);
vui_state* vui_translator_tfunc_puts(vui_state* currstate, unsigned int c, int act, void* data);

static inline vui_transition vui_transition_translator_push(vui_translator* tr, vui_state* next)
{
	return vui_transition_new3(next, vui_translator_tfunc_push, tr);
}

static inline vui_transition vui_transition_translator_putc(vui_translator* tr, vui_state* next)
{
	return vui_transition_new3(next, vui_translator_tfunc_putc, tr);
}

static inline vui_transition vui_transition_translator_pop(vui_translator* tr, vui_state* next)
{
	return vui_transition_new3(next, vui_translator_tfunc_pop, tr);
}

static inline vui_transition vui_transition_translator_puts(vui_translator* tr, vui_state* next)
{
	return vui_transition_new3(next, vui_translator_tfunc_puts, tr);
}

// state constructors
static inline vui_state* vui_state_new_deadend(void)
{
	return vui_state_new_s(vui_translator_deadend);
}

static inline vui_state* vui_state_new_putc(vui_translator* tr)
{
	return vui_state_new_t_self(vui_transition_translator_putc(tr, NULL));
}


// fragment constructors
vui_frag* vui_frag_accept_escaped(vui_translator* tr);

vui_frag* vui_frag_deadend(void);

vui_frag* vui_frag_accept_any(vui_translator* tr);

#ifdef __cplusplus
}
#endif

#endif
