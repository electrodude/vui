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


typedef struct vui_tr vui_tr;

typedef enum vui_tr_obj_type
{
	VUI_TR_OBJ_EMPTY,
	VUI_TR_OBJ_STRING,
	VUI_TR_OBJ_STACK,
	VUI_TR_OBJ_STATE,
	VUI_TR_OBJ_FRAG,
	VUI_TR_OBJ_TRANSLATOR,
} vui_tr_obj_type;

typedef union vui_tr_obj_union
{
	void* ptr;
	vui_string* str;
	vui_stack* stk;
	vui_state* st;
	vui_frag* frag;
	vui_tr* tr;
} vui_tr_obj_union;

typedef struct vui_tr_obj
{
	vui_tr_obj_type type;
	vui_tr_obj_union obj;
} vui_tr_obj;

typedef enum vui_tr_status
{
	VUI_TR_STATUS_OK = 0,        // success
	VUI_TR_STATUS_RUNNING,       // currently running
	VUI_TR_STATUS_BAD_NEXT,      // ended up at state NULL
	VUI_TR_STATUS_BAD_TYPE,      // unable to cast to required type
	VUI_TR_STATUS_UNDERFLOW,     // unable to cast to required type
	VUI_TR_STATUS_ERR,           // misc. error
} vui_tr_status;

typedef struct vui_tr
{
	vui_state* st_start;
	vui_stack* stk;
	vui_stack* st_stk;
	vui_tr_obj* tos;

	vui_tr_status status;

	vui_string* str;
	char* s_err;
} vui_tr;


// general translator methods
void vui_tr_init(void);

#define vui_tr_new() vui_tr_new_at(NULL)
vui_tr* vui_tr_new_at(vui_tr* tr);

static inline vui_tr* vui_tr_replace(vui_tr* tr, vui_state* st_start)
{
	vui_gc_decr(tr->st_start);
	tr->st_start = st_start;
	vui_gc_incr(tr->st_start);

	return tr;
}

void vui_tr_kill(vui_tr* tr);

vui_stack* vui_tr_run(vui_tr* tr, char* s);
vui_stack* vui_tr_run_str(vui_tr* tr, vui_string* str);


// translator stack object functions
void vui_tr_stack_push(vui_tr* tr, vui_tr_obj* obj);
vui_tr_obj* vui_tr_stack_pop(vui_tr* tr);
#define vui_tr_stack_peek(tr) (tr->tos)
vui_tr_obj* vui_tr_obj_cast(vui_tr_obj* obj, vui_tr_obj_type type);

#define vui_tr_obj_cast_string(obj) vui_tr_obj_cast(obj, VUI_TR_OBJ_STRING)
#define vui_tr_obj_cast_stack(obj) vui_tr_obj_cast(obj, VUI_TR_OBJ_STACK)
#define vui_tr_obj_cast_state(obj) vui_tr_obj_cast(obj, VUI_TR_OBJ_STATE)
#define vui_tr_obj_cast_frag(obj) vui_tr_obj_cast(obj, VUI_TR_OBJ_FRAG)
#define vui_tr_obj_cast_translator(obj) vui_tr_obj_cast(obj, VUI_TR_OBJ_TRANSLATOR)

#define vui_tr_obj_peek(tr, type) vui_tr_obj_cast(tr->tos, type)

#define vui_tr_obj_peek_string(tr) vui_tr_obj_peek(tr, VUI_TR_OBJ_STRING)
#define vui_tr_obj_peek_stack(tr) vui_tr_obj_peek(tr, VUI_TR_OBJ_STACK)
#define vui_tr_obj_peek_state(tr) vui_tr_obj_peek(tr, VUI_TR_OBJ_STATE)
#define vui_tr_obj_peek_frag(tr) vui_tr_obj_peek(tr, VUI_TR_OBJ_FRAG)
#define vui_tr_obj_peek_translator(tr) vui_tr_obj_peek(tr, VUI_TR_OBJ_TRANSLATOR)

#define vui_tr_obj_pop(tr, type) vui_tr_obj_cast(vui_tr_stack_pop(tr), type)

#define vui_tr_obj_pop_string(tr) vui_tr_obj_pop(tr, VUI_TR_OBJ_STRING)
#define vui_tr_obj_pop_stack(tr) vui_tr_obj_pop(tr, VUI_TR_OBJ_STACK)
#define vui_tr_obj_pop_state(tr) vui_tr_obj_pop(tr, VUI_TR_OBJ_STATE)
#define vui_tr_obj_pop_frag(tr) vui_tr_obj_pop(tr, VUI_TR_OBJ_FRAG)
#define vui_tr_obj_pop_translator(tr) vui_tr_obj_pop(tr, VUI_TR_OBJ_TRANSLATOR)


// void* instead of vui_tr_obj_union primarily to save keystrokes
vui_tr_obj* vui_tr_obj_new(vui_tr_obj_type type, void* obj);
vui_tr_obj* vui_tr_obj_dup(vui_tr_obj* obj);

void vui_tr_obj_kill(vui_tr_obj* obj);

// specific translator constructors
vui_tr* vui_tr_new_identity(void);

// transition helpers

// object creation/deletion
vui_state* vui_tr_tfunc_new_string(vui_state* currstate, unsigned int c, int act, void* data);
vui_state* vui_tr_tfunc_new_stack(vui_state* currstate, unsigned int c, int act, void* data);
vui_state* vui_tr_tfunc_new_state(vui_state* currstate, unsigned int c, int act, void* data);
vui_state* vui_tr_tfunc_new_frag(vui_state* currstate, unsigned int c, int act, void* data);
vui_state* vui_tr_tfunc_new_tr(vui_state* currstate, unsigned int c, int act, void* data);

vui_state* vui_tr_tfunc_dup(vui_state* currstate, unsigned int c, int act, void* data);
vui_state* vui_tr_tfunc_drop(vui_state* currstate, unsigned int c, int act, void* data);


// vui_string
vui_state* vui_tr_tfunc_append(vui_state* currstate, unsigned int c, int act, void* data);

// vui_string, vui_frag
vui_state* vui_tr_tfunc_cat(vui_state* currstate, unsigned int c, int act, void* data);
vui_state* vui_tr_tfunc_union(vui_state* currstate, unsigned int c, int act, void* data);

// vui_state
vui_state* vui_tr_tfunc_call(vui_state* currstate, unsigned int c, int act, void* data);
vui_state* vui_tr_tfunc_return(vui_state* currstate, unsigned int c, int act, void* data);

vui_state* vui_tr_tfunc_apply(vui_state* currstate, unsigned int c, int act, void* data);

// vui_stack
vui_state* vui_tr_tfunc_push(vui_state* currstate, unsigned int c, int act, void* data);
vui_state* vui_tr_tfunc_pop(vui_state* currstate, unsigned int c, int act, void* data);
vui_state* vui_tr_tfunc_peek(vui_state* currstate, unsigned int c, int act, void* data);

static inline vui_transition* vui_transition_tr_new_string(vui_tr* tr, vui_state* next)
{
	return vui_transition_new3(next, vui_tr_tfunc_new_string, tr);
}

static inline vui_transition* vui_transition_tr_append(vui_tr* tr, vui_state* next)
{
	return vui_transition_new3(next, vui_tr_tfunc_append, tr);
}

static inline vui_transition* vui_transition_tr_drop(vui_tr* tr, vui_state* next)
{
	return vui_transition_new3(next, vui_tr_tfunc_drop, tr);
}

static inline vui_transition* vui_transition_tr_cat(vui_tr* tr, vui_state* next)
{
	return vui_transition_new3(next, vui_tr_tfunc_cat, tr);
}

// state constructors
static inline vui_state* vui_state_new_deadend(void)
{
	return vui_state_new_s(vui_state_new());
}

static inline vui_state* vui_state_new_putc(vui_tr* tr)
{
	return vui_state_new_t_self(vui_transition_tr_append(tr, NULL));
}


// fragment constructors
vui_frag* vui_frag_accept_escaped(vui_tr* tr);

vui_frag* vui_frag_deadend(void);

vui_frag* vui_frag_accept_any(vui_tr* tr);

#ifdef __cplusplus
}
#endif

#endif
