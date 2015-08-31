#pragma once

#include "stack.h"
#include "string.h"
#include "statemachine.h"

typedef struct vui_translator
{
	vui_state* st_start;
	vui_stack* stk;
	vui_string* str;
} vui_translator;

extern vui_state* vui_translator_deadend;

void vui_translator_init(void);
vui_translator* vui_translator_new(void);
void vui_translator_kill(vui_translator* tr);

vui_stack* vui_translator_run(vui_translator* tr, char* in);

// transition helpers
vui_state* vui_translator_tfunc_push(vui_state* currstate, unsigned int c, int act, void* data);
vui_state* vui_translator_tfunc_putc(vui_state* currstate, unsigned int c, int act, void* data);

static inline vui_transition vui_transition_translator_push(vui_translator* tr, vui_state* next)
{
	return vui_transition_new3(next, vui_translator_tfunc_push, tr);
}

static inline vui_transition vui_transition_translator_putc(vui_translator* tr, vui_state* next)
{
	return vui_transition_new3(next, vui_translator_tfunc_putc, tr);
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


vui_state* vui_translator_key_escaper(vui_translator* tr, vui_state* next);
