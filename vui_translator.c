#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <string.h>

#include "vui_utf8.h"
#include "vui.h"

#include "vui_translator.h"


// general methods
void vui_translator_init(void)
{
}

vui_translator* vui_translator_new(void)
{
	vui_translator* tr = malloc(sizeof(vui_translator));

	tr->st_start = vui_state_new_deadend();
	vui_gc_incr(tr->st_start);

	tr->str = NULL;
	tr->stk = vui_stack_new();
	tr->stk->dtor = (void (*)(void*))vui_string_kill;

	return tr;
}

void vui_translator_kill(vui_translator* tr)
{
	vui_gc_decr(tr->st_start);

	vui_stack_kill(tr->stk);	// kills tr->str, which is vui_stack_peek(tr->stk)

	free(tr);
}

vui_stack* vui_translator_run(vui_translator* tr, char* s)
{
	vui_stack_reset(tr->stk);

	vui_stack_push(tr->stk, tr->str = vui_string_new());

	vui_next(vui_run_s(tr->st_start, s, VUI_ACT_RUN), 0, VUI_ACT_RUN);

	return tr->stk;
}

vui_stack* vui_translator_run_str(vui_translator* tr, vui_string* str)
{
	vui_stack_reset(tr->stk);

	vui_stack_push(tr->stk, tr->str = vui_string_new());

	vui_next(vui_run_str(tr->st_start, str, VUI_ACT_RUN), 0, VUI_ACT_RUN);

	return tr->stk;
}


vui_state* vui_translator_tfunc_push(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_translator* tr = data;

	if (act <= 0) return NULL;

	vui_string_shrink(tr->str); // shrink the previous string

	vui_stack_push(tr->stk, tr->str = vui_string_new()); // push a new one

	return NULL;
}

// specific translator constructors
vui_translator* vui_translator_new_identity(void)
{
	vui_translator* tr = vui_translator_new();

	vui_translator_replace(tr, vui_state_new_putc(tr));

	return tr;
}


// transition helpers
vui_state* vui_translator_tfunc_putc(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_translator* tr = data;

	if (act <= 0) return NULL;

	vui_string_putc(tr->str, c);

	return NULL;
}

vui_state* vui_translator_tfunc_pop(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_translator* tr = data;

	if (act <= 0) return NULL;

	vui_string_kill(vui_stack_pop(tr->stk));
	tr->str = vui_stack_peek(tr->stk);

	return NULL;
}

vui_state* vui_translator_tfunc_puts(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_translator* tr = data;

	if (act <= 0) return NULL;

	vui_string* str = vui_stack_pop(tr->stk);
	tr->str = vui_stack_peek(tr->stk);
	vui_string_append(tr->str, str);
	vui_string_kill(str);

	return NULL;
}



// fragment constructors
static inline void t_escaper_end(vui_state* lt, vui_translator* tr, vui_state* returnto, char* action, char* reaction)
{
	vui_transition lt_mid = vui_transition_translator_putc(tr, NULL);

	vui_stack* lt_end_funcs = vui_stack_new_v(2,
			vui_transition_multi_stage(vui_transition_translator_pop(tr, NULL)),
			vui_transition_multi_stage(vui_transition_run_s_s(vui_state_new_t_self(vui_transition_translator_putc(tr, NULL)), reaction)));

	vui_set_string_t_mid(lt, action, lt_mid, vui_transition_multi(lt_end_funcs, returnto));
}

vui_frag* vui_frag_accept_escaped(vui_translator* tr)
{
	vui_state* escaper = vui_state_new_putc(tr);

	vui_string_reset(&escaper->name);
	vui_string_puts(&escaper->name, "escaper");

	escaper->gv_norank = 1;

	vui_stack* exits = vui_stack_new();

	vui_state* exit = vui_state_new();

	vui_transition leave = vui_transition_translator_push(tr, exit);
	vui_set_char_t(escaper, ' ', leave);
	vui_set_char_t(escaper, 0, leave);

	vui_stack_push(exits, exit);

	vui_stack* lt_abort_funcs = vui_stack_new_v(2,
		vui_transition_multi_stage(vui_transition_translator_putc(tr, NULL)),
		vui_transition_multi_stage(vui_transition_translator_puts(tr, NULL)));

	vui_stack* lt_enter_funcs = vui_stack_new_v(2,
		vui_transition_multi_stage(vui_transition_translator_push(tr, NULL)),
		vui_transition_multi_stage(vui_transition_translator_putc(tr, NULL)));

	vui_state* lt = vui_state_new_t(vui_transition_multi(lt_abort_funcs, escaper));
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

	vui_state* bsl = vui_state_new_t(vui_transition_multi(lt_abort_funcs, escaper));
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

vui_frag* vui_frag_accept_any(vui_translator* tr)
{
	return vui_frag_new(vui_state_new_t_self(vui_transition_translator_putc(tr, NULL)), NULL);
}
