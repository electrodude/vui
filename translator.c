#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <string.h>

#include "utf8.h"
#include "vui.h"

#include "translator.h"

vui_state* vui_translator_deadend;

void vui_translator_init(void)
{
	vui_translator_deadend = vui_state_new();
	vui_translator_deadend->root++;
}

vui_translator* vui_translator_new(void)
{
	vui_translator* tr = malloc(sizeof(vui_translator));

	tr->st_start = vui_state_new_deadend();
	tr->st_start->root++;

	tr->str = NULL;
	tr->stk = vui_stack_new();
	tr->stk->dtor = (void (*)(void*))vui_string_kill;

	return tr;
}

void vui_translator_kill(vui_translator* tr)
{
	tr->st_start->root--;

	free(tr);
}

vui_stack* vui_translator_run(vui_translator* tr, char* in)
{
	vui_stack_reset(tr->stk);

	vui_stack_push(tr->stk, tr->str = vui_string_new(NULL));

	vui_next(vui_run_s(tr->st_start, in, 1), 0, 1);

	return tr->stk;
}


vui_state* vui_translator_tfunc_push(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_translator* tr = data;

	if (act <= 0) return NULL;

	vui_string_shrink(tr->str); // shrink the previous string

	vui_stack_push(tr->stk, tr->str = vui_string_new(NULL)); // push a new one

	return NULL;
}

vui_state* vui_translator_tfunc_putc(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_translator* tr = data;

	if (act <= 0) return NULL;

	vui_string_putc(tr->str, c);

	return NULL;
}



vui_state* vui_translator_key_escaper(vui_translator* tr, vui_state* next)
{
	vui_state* escaper = vui_state_new_putc(tr);

	escaper->name = strdup("escaper");

	escaper->gv_norank = 1;

	vui_transition leave = vui_transition_translator_push(tr, next);
	vui_set_char_t(escaper, ' ', leave);

	vui_state* eschelper = vui_state_new_t(vui_transition_translator_putc(tr, escaper));

	vui_map2(escaper, "\\<", eschelper, "<");

	vui_map2(escaper, "<cr>", eschelper, "\n");
	vui_map2(escaper, "<enter>", eschelper, "\n");
	vui_map2(escaper, "\\n", eschelper, "\n");
	vui_map2(escaper, "<tab>", eschelper, "\n");
	vui_map2(escaper, "\\t", eschelper, "\t");
	vui_map2(escaper, "<left>", eschelper, vui_utf8_encode_alloc(VUI_KEY_LEFT));
	vui_map2(escaper, "<l>", eschelper, vui_utf8_encode_alloc(VUI_KEY_LEFT));
	vui_map2(escaper, "<right>", eschelper, vui_utf8_encode_alloc(VUI_KEY_RIGHT));
	vui_map2(escaper, "<r>", eschelper, vui_utf8_encode_alloc(VUI_KEY_RIGHT));
	vui_map2(escaper, "<up>", eschelper, vui_utf8_encode_alloc(VUI_KEY_UP));
	vui_map2(escaper, "<u>", eschelper, vui_utf8_encode_alloc(VUI_KEY_UP));
	vui_map2(escaper, "<down>", eschelper, vui_utf8_encode_alloc(VUI_KEY_DOWN));
	vui_map2(escaper, "<d>", eschelper, vui_utf8_encode_alloc(VUI_KEY_DOWN));
	vui_map2(escaper, "<home>", eschelper, vui_utf8_encode_alloc(VUI_KEY_HOME));
	vui_map2(escaper, "<end>", eschelper, vui_utf8_encode_alloc(VUI_KEY_END));
	vui_map2(escaper, "<backspace>", eschelper, vui_utf8_encode_alloc(VUI_KEY_BACKSPACE));
	vui_map2(escaper, "<bs>", eschelper, vui_utf8_encode_alloc(VUI_KEY_BACKSPACE));
	vui_map2(escaper, "<delete>", eschelper, vui_utf8_encode_alloc(VUI_KEY_DELETE));
	vui_map2(escaper, "<del>", eschelper, vui_utf8_encode_alloc(VUI_KEY_DELETE));
	vui_map2(escaper, "<escape>", eschelper, "\x27");
	vui_map2(escaper, "<esc>", eschelper, "\x27");
	vui_map2(escaper, "<space>", eschelper, " ");
	vui_map2(escaper, "\\ ", eschelper, " ");
	vui_map2(escaper, "\\\\", eschelper, "\\");

	return escaper;
}
