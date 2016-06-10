#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <string.h>

#include "vui_debug.h"

#include "vui_mem.h"

#include "vui_stack_refcount.h"

#include "vui_utf8.h"
#include "vui.h"

#include "vui_combinator.h"

#include "vui_translator.h"


#define VUI_TR_TFUNC_ASSERT(p, err) if (p == NULL) { tr->status = err; return NULL; }

// general translator methods
void vui_tr_init(void)
{
}

vui_tr* vui_tr_new_at(vui_tr* tr)
{
	if (tr == NULL)
	{
		tr = vui_new(vui_tr);
	}

	tr->st_start = vui_state_new_deadend();
	vui_gc_incr(tr->st_start);

	tr->stk = vui_stack_new();
	tr->stk->on_drop = (void (*)(void*))vui_tr_obj_kill;

	tr->tos = vui_stack_peek(tr->stk);


	tr->st_stk = vui_stack_refcount_new();

	return tr;
}

void vui_tr_kill(vui_tr* tr)
{
	vui_gc_decr(tr->st_start);

	vui_stack_kill(tr->st_stk);
	vui_stack_kill(tr->stk);	// kills tr->tos, which is vui_stack_peek(tr->stk)

	vui_free(tr);
}

vui_stack* vui_tr_run(vui_tr* tr, char* s)
{
	vui_string str;
	vui_string_new_at(&str);
	vui_string_puts(&str, s);

	vui_stack* stk = vui_tr_run_str(tr, &str);

	vui_string_dtor(&str);

	return stk;
}

vui_stack* vui_tr_run_str(vui_tr* tr, vui_string* str)
{
	// clear old stuff
	vui_stack_reset(tr->stk);

	vui_stack_reset(tr->st_stk);

	// reinitialize
	tr->tos = vui_tr_obj_new(VUI_TR_OBJ_EMPTY, NULL);

	vui_tr_stack_push(tr, tr->tos);


	vui_state* st = tr->st_start;

	tr->str = str;

	tr->s_err = NULL;

	tr->status = VUI_TR_STATUS_RUNNING;

	vui_string_get(str); // append null terminator

	// iterate over string
	// str->s[str->n] == 0, so going one past the end is safe
	for (size_t i = 0; i < str->n + 1 && tr->status == VUI_TR_STATUS_RUNNING; i++)
	{
		st = vui_next(st, str->s[i], VUI_ACT_RUN);

		if (st == NULL)
		{
			tr->status = VUI_TR_STATUS_BAD_NEXT;
		}
	}

	if (tr->status == VUI_TR_STATUS_RUNNING)
	{
		tr->status = VUI_TR_STATUS_OK;
	}

	tr->str = NULL;

	return tr->status == VUI_TR_STATUS_OK ? tr->stk : NULL;
}

vui_tr_obj* vui_tr_obj_new(vui_tr_obj_type type, void* data)
{
	vui_tr_obj* obj = vui_new(vui_tr_obj);

	obj->type = type;
	obj->obj.ptr = data;

	return obj;
}

vui_tr_obj* vui_tr_obj_dup(vui_tr_obj* orig)
{
	vui_tr_obj* obj = vui_tr_obj_new(orig->type, NULL);

	switch (obj->type)
	{
		case VUI_TR_OBJ_EMPTY:
			break;

		case VUI_TR_OBJ_STRING:
			obj->obj.str = vui_string_dup(orig->obj.str);
			break;

		case VUI_TR_OBJ_STACK:
			obj->obj.stk = vui_stack_dup(orig->obj.stk);
			break;

		case VUI_TR_OBJ_STATE:
			obj->obj.st = vui_state_dup(orig->obj.st);
			break;

		case VUI_TR_OBJ_FRAG:
			obj->obj.frag = vui_frag_dup(orig->obj.frag);
			break;

		case VUI_TR_OBJ_TRANSLATOR:
			//obj->obj.tr = vui_tr_dup(orig->obj.tr);
			break;
	}

	return obj;
}

static inline vui_tr_obj_union vui_tr_obj_release(vui_tr_obj* obj)
{
	vui_tr_obj_union data = obj->obj;

	vui_free(obj);

	return data;
}

void vui_tr_obj_kill(vui_tr_obj* obj)
{
	switch (obj->type)
	{
		case VUI_TR_OBJ_EMPTY:
			break;

		case VUI_TR_OBJ_STRING:
			vui_string_kill(obj->obj.str);
			break;

		case VUI_TR_OBJ_STACK:
			vui_stack_kill(obj->obj.stk);
			break;

		case VUI_TR_OBJ_STATE:
			vui_gc_decr(obj->obj.st);
			break;

		case VUI_TR_OBJ_FRAG:
			vui_frag_kill(obj->obj.frag);
			break;

		case VUI_TR_OBJ_TRANSLATOR:
			vui_tr_kill(obj->obj.tr);
			break;
	}

	vui_tr_obj_release(obj);
}

// vui_stack of vui_tr_obj helpers
void vui_tr_stack_push(vui_tr* tr, vui_tr_obj* obj)
{
	if (tr->tos != NULL)
	{
		vui_tr_obj* tos = tr->tos;

		switch (tos->type)
		{
			case VUI_TR_OBJ_STRING:
				vui_string_shrink(tos->obj.str);
				break;

			case VUI_TR_OBJ_STACK:
				vui_stack_shrink(tos->obj.stk);
				break;

			case VUI_TR_OBJ_STATE:
				vui_gc_incr(obj->obj.st);
				break;

			default:
				break;
		}
	}

	vui_stack_push(tr->stk, tr->tos = obj);
}

vui_tr_obj* vui_tr_stack_pop(vui_tr* tr)
{
	vui_tr_obj* obj = vui_stack_pop(tr->stk);

	tr->tos = vui_stack_peek(tr->stk);

	switch (obj->type)
	{
		case VUI_TR_OBJ_STATE:
			vui_gc_decr(obj->obj.st);
			break;

		default:
			break;
	}

	return obj;
}

vui_tr_obj* vui_tr_obj_cast(vui_tr_obj* obj, vui_tr_obj_type type)
{
	if (obj == NULL)
	{
		return NULL;
	}

	if (obj->type == type)
	{
		return obj;
	}

	vui_tr* tr;

	switch (obj->type) // what we have
	{
		case VUI_TR_OBJ_EMPTY:
			switch (type) // what we want
			{
				case VUI_TR_OBJ_EMPTY:
					break;

				case VUI_TR_OBJ_STRING:
					obj->obj.str = vui_string_new();
					break;

				case VUI_TR_OBJ_STACK:
					obj->obj.stk = vui_stack_new();
					obj->obj.stk->on_drop = (void(*)(void*))vui_tr_obj_kill;
					break;

				case VUI_TR_OBJ_STATE:
					obj->obj.st = vui_state_new_t(NULL);
					break;

				case VUI_TR_OBJ_FRAG:
					goto fail;
					break;

				case VUI_TR_OBJ_TRANSLATOR:
					goto fail;
					break;
			}
			break;

		case VUI_TR_OBJ_STRING:
			switch (type)
			{
				case VUI_TR_OBJ_EMPTY:
					break;
				case VUI_TR_OBJ_STRING:
					break;

				case VUI_TR_OBJ_STACK:
					goto fail;
					break;

				case VUI_TR_OBJ_STATE:
					goto fail;
					break;

				case VUI_TR_OBJ_FRAG:
					obj->obj.frag = vui_frag_new_string(vui_string_get(obj->obj.str));
					break;

				case VUI_TR_OBJ_TRANSLATOR:
					goto fail;
					break;

			}
			break;

		case VUI_TR_OBJ_STACK:
			switch (type)
			{
				case VUI_TR_OBJ_EMPTY:
					break;
				case VUI_TR_OBJ_STRING:
					goto fail;
					break;

				case VUI_TR_OBJ_STACK:
					break;

				case VUI_TR_OBJ_STATE:
					goto fail;
					break;

				case VUI_TR_OBJ_FRAG:
					goto fail;
					break;

				case VUI_TR_OBJ_TRANSLATOR:
					goto fail;
					break;

			}
			break;

		case VUI_TR_OBJ_STATE:
			switch (type)
			{
				case VUI_TR_OBJ_EMPTY:
					break;
				case VUI_TR_OBJ_STRING:
					goto fail;
					break;

				case VUI_TR_OBJ_STACK:
					goto fail;
					break;

				case VUI_TR_OBJ_STATE:
					break;

				case VUI_TR_OBJ_FRAG:
					obj->obj.frag = vui_frag_new(obj->obj.st, vui_stack_refcount_new());
					break;

				case VUI_TR_OBJ_TRANSLATOR:
					tr = vui_tr_new();
					vui_tr_replace(tr, obj->obj.st);
					obj->obj.tr = tr;
					break;
			}
			break;

		case VUI_TR_OBJ_FRAG:
			switch (type)
			{
				case VUI_TR_OBJ_EMPTY:
					break;
				case VUI_TR_OBJ_STRING:
					goto fail;
					break;

				case VUI_TR_OBJ_STACK:
					goto fail;
					break;

				case VUI_TR_OBJ_STATE:
					//obj->obj.st = vui_frag_release(obj->obj.frag, /* ? */);
					goto fail;
					break;

				case VUI_TR_OBJ_FRAG:
					break;

				case VUI_TR_OBJ_TRANSLATOR:
					goto fail;
					break;

			}
			break;

		case VUI_TR_OBJ_TRANSLATOR:
			switch (type)
			{
				case VUI_TR_OBJ_EMPTY:
					break;

				case VUI_TR_OBJ_STRING:
					goto fail;
					break;

				case VUI_TR_OBJ_STACK:
					goto fail;
					break;

				case VUI_TR_OBJ_STATE:
					goto fail;
					break;

				case VUI_TR_OBJ_FRAG:
					goto fail;
					break;

				case VUI_TR_OBJ_TRANSLATOR:
					break;
			}
			break;
	}


	obj->type = type;

	return obj;

fail:

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_TR)
	vui_debugf("vui_tr_stack_cast: can't cast %d to %d!\n", obj->type, type);
#endif

	return NULL;
}


// specific translator constructors
vui_tr* vui_tr_new_identity(void)
{
	vui_tr* tr = vui_tr_new();

	vui_tr_replace(tr, vui_state_new_putc(tr));

	return tr;
}

// TODO
vui_tr* vui_tr_new_tr(char* from, char* to, char* delete)
{
	vui_tr* tr = vui_tr_new();

	vui_state* st = tr->st_start;

	if (from != NULL && to != NULL)
	{
		for (; *from && *to; from++)
		{
			char f = *from;
			char t = *to;

			if (f != t)
			{
				// TODO
			}
		}
	}

	if (delete != NULL)
	{
		for (; *delete; delete++)
		{
			// TODO
		}
	}

	return tr;
}


// transition helpers

vui_state* vui_tr_tfunc_new_string(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_tr* tr = data;

	if (act <= 0) return NULL;

	vui_tr_stack_push(tr, vui_tr_obj_new(VUI_TR_OBJ_STRING, vui_string_new())); // push a new string

	return NULL;
}

vui_state* vui_tr_tfunc_new_stack(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_tr* tr = data;

	if (act <= 0) return NULL;

	vui_tr_stack_push(tr, vui_tr_obj_new(VUI_TR_OBJ_STACK, vui_stack_new())); // push a new stack

	return NULL;
}

vui_state* vui_tr_tfunc_append(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_tr* tr = data;

	if (act <= 0) return NULL;

	vui_tr_obj* strobj = vui_tr_obj_peek_string(tr);

	VUI_TR_TFUNC_ASSERT(strobj, VUI_TR_STATUS_BAD_TYPE);

	vui_string_putc(strobj->obj.str, c);

	return NULL;
}

vui_state* vui_tr_tfunc_dup(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_tr* tr = data;

	if (act <= 0) return NULL;

	vui_tr_stack_push(tr, vui_tr_obj_dup(tr->tos));

	return NULL;
}

vui_state* vui_tr_tfunc_drop(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_tr* tr = data;

	if (act <= 0) return NULL;

	vui_tr_obj_kill(vui_tr_stack_pop(tr));

	return NULL;
}

vui_state* vui_tr_tfunc_cat(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_tr* tr = data;

	if (act <= 0) return NULL;

	vui_tr_obj* b = vui_tr_stack_pop(tr);

	VUI_TR_TFUNC_ASSERT(b, VUI_TR_STATUS_UNDERFLOW);

	vui_tr_obj* a = vui_tr_stack_pop(tr);

	VUI_TR_TFUNC_ASSERT(a, VUI_TR_STATUS_UNDERFLOW);

	b = vui_tr_obj_cast(b, a->type);

	VUI_TR_TFUNC_ASSERT(b, VUI_TR_STATUS_BAD_TYPE);

	switch (tr->tos->type)
	{
		case VUI_TR_OBJ_STRING:
			vui_string_append(a->obj.str, b->obj.str);
			vui_tr_stack_push(tr, a);
			break;

		case VUI_TR_OBJ_STACK:
			vui_stack_append(a->obj.stk, b->obj.stk);
			vui_tr_stack_push(tr, a);
			break;

		case VUI_TR_OBJ_FRAG:
			a->obj.frag = vui_frag_cat(a->obj.frag, b->obj.frag);
			vui_tr_stack_push(tr, a);
			break;

		default:
			tr->status = VUI_TR_STATUS_BAD_TYPE;
			vui_tr_stack_push(tr, a);
			vui_tr_stack_push(tr, b);
			goto fail;
			break;
	}

	vui_tr_obj_release(b);

fail:

	return NULL;
}

vui_state* vui_tr_tfunc_union(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_tr* tr = data;

	if (act <= 0) return NULL;

	vui_tr_obj* b = vui_tr_obj_pop_frag(tr);

	VUI_TR_TFUNC_ASSERT(b, VUI_TR_STATUS_BAD_TYPE);

	vui_tr_obj* a = vui_tr_obj_pop_frag(tr);

	VUI_TR_TFUNC_ASSERT(a, VUI_TR_STATUS_BAD_TYPE);

	a->obj.frag = vui_frag_cat(a->obj.frag, b->obj.frag);
	vui_tr_stack_push(tr, a);

	vui_tr_obj_release(b);

	return NULL;
}

vui_state* vui_tr_tfunc_call(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_tr* tr = data;

	if (act <= 0) return NULL;

	//vui_stack_push(tr->stk, /* what? */);

	return NULL;
}

vui_state* vui_tr_tfunc_return(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_tr* tr = data;

	if (act <= 0) return vui_stack_peek(tr->st_stk);

	return vui_stack_pop(tr->st_stk);
}

vui_state* vui_tr_tfunc_apply(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_tr* tr = data;

	if (act <= 0) return NULL;


	return NULL;
}


// fragment constructors
static inline void t_escaper_end(vui_state* lt, vui_tr* tr, vui_state* returnto, char* action, char* reaction)
{
	vui_transition* lt_mid = vui_transition_tr_append(tr, NULL);

	vui_gc_ptr* lt_end_funcs = vui_gc_ptr_new_stack(vui_stack_new_v(2,
			vui_transition_multi_stage(vui_transition_tr_drop(tr, NULL)),
			vui_transition_multi_stage(vui_transition_run_s_s(vui_state_new_t_self(vui_transition_tr_append(tr, NULL)), reaction))));

	vui_set_string_t_mid(lt, action, lt_mid, vui_transition_multi(lt_end_funcs, returnto));
}

vui_frag* vui_frag_accept_escaped(vui_tr* tr)
{
	vui_state* escaper = vui_state_new_putc(tr);

	vui_string_sets(&escaper->name, "escaper");

	escaper->gv_norank = 1;

	vui_stack* exits = vui_stack_refcount_new();

	vui_state* exit = vui_state_new();

	vui_transition* leave = vui_transition_tr_new_string(tr, exit);
	vui_set_char_t(escaper, ' ', leave);
	vui_set_char_t(escaper, 0, leave);

	vui_stack_push(exits, exit);

	vui_gc_ptr* lt_abort_funcs = vui_gc_ptr_new_stack(vui_stack_new_v(2,
		vui_transition_multi_stage(vui_transition_tr_append(tr, NULL)),
		vui_transition_multi_stage(vui_transition_tr_cat(tr, NULL))));

	vui_gc_ptr* lt_enter_funcs = vui_gc_ptr_new_stack(vui_stack_new_v(2,
		vui_transition_multi_stage(vui_transition_tr_new_string(tr, NULL)),
		vui_transition_multi_stage(vui_transition_tr_append(tr, NULL))));

	vui_transition* t_abort = vui_transition_multi(lt_abort_funcs, escaper);
	vui_state* lt = vui_state_new_t(t_abort);
	vui_set_char_t(escaper, '<', vui_transition_multi(lt_enter_funcs, lt));

	t_escaper_end(lt, tr, escaper, "cr>", "\n");
	t_escaper_end(lt, tr, escaper, "enter>", "\n");
	t_escaper_end(lt, tr, escaper, "tab>", "\t");
	t_escaper_end(lt, tr, escaper, "lt>", "<");
	t_escaper_end(lt, tr, escaper, "left>", vui_utf8_encode_static(VUI_KEY_LEFT));
	t_escaper_end(lt, tr, escaper, "l>", vui_utf8_encode_static(VUI_KEY_LEFT));
	t_escaper_end(lt, tr, escaper, "right>", vui_utf8_encode_static(VUI_KEY_RIGHT));
	t_escaper_end(lt, tr, escaper, "r>", vui_utf8_encode_static(VUI_KEY_RIGHT));
	t_escaper_end(lt, tr, escaper, "up>", vui_utf8_encode_static(VUI_KEY_UP));
	t_escaper_end(lt, tr, escaper, "u>", vui_utf8_encode_static(VUI_KEY_UP));
	t_escaper_end(lt, tr, escaper, "down>", vui_utf8_encode_static(VUI_KEY_DOWN));
	t_escaper_end(lt, tr, escaper, "d>", vui_utf8_encode_static(VUI_KEY_DOWN));
	t_escaper_end(lt, tr, escaper, "home>", vui_utf8_encode_static(VUI_KEY_HOME));
	t_escaper_end(lt, tr, escaper, "end>", vui_utf8_encode_static(VUI_KEY_END));
	t_escaper_end(lt, tr, escaper, "backspace>", vui_utf8_encode_static(VUI_KEY_BACKSPACE));
	t_escaper_end(lt, tr, escaper, "bs>", vui_utf8_encode_static(VUI_KEY_BACKSPACE));
	t_escaper_end(lt, tr, escaper, "delete>", vui_utf8_encode_static(VUI_KEY_DELETE));
	t_escaper_end(lt, tr, escaper, "del>", vui_utf8_encode_static(VUI_KEY_DELETE));
	t_escaper_end(lt, tr, escaper, "escape>", "\x27");
	t_escaper_end(lt, tr, escaper, "esc>", "\x27");
	t_escaper_end(lt, tr, escaper, "space>", " ");
	t_escaper_end(lt, tr, escaper, "sp>", " ");

	vui_state* bsl = vui_state_new_t(t_abort);
	vui_set_char_t(escaper, '\\', vui_transition_multi(lt_enter_funcs, bsl));

	t_escaper_end(bsl, tr, escaper, "n", "\n");
	t_escaper_end(bsl, tr, escaper, "t", "\t");
	t_escaper_end(bsl, tr, escaper, "\\", "\n");
	t_escaper_end(bsl, tr, escaper, " ", " ");
	t_escaper_end(bsl, tr, escaper, "<", "<");

	return vui_frag_new(escaper, exits);
}

vui_frag* vui_frag_deadend(void)
{
	return vui_frag_new(vui_state_new_deadend(), NULL);
}

vui_frag* vui_frag_accept_any(vui_tr* tr)
{
	return vui_frag_new(vui_state_new_t_self(vui_transition_tr_append(tr, NULL)), NULL);
}
