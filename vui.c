#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <string.h>

#include "vui_debug.h"

#include "vui_mem.h"

#include "vui_stack_refcount.h"

#include "vui_utf8.h"
#include "vui_gc.h"

#include "vui_fragment.h"
#include "vui_combinator.h"

#include "vui.h"

// extern globals
char* vui_bar;
int vui_crsrx;

int vui_count;

vui_stack* vui_state_stack;

vui_state* vui_curr_state;

vui_mode* vui_normal_mode;
vui_state* vui_count_mode;
vui_state* vui_register_select_mode;

vui_state* vui_register_container = NULL; // register container state

vui_string* vui_register_recording;

vui_string* vui_register_curr;

// non-extern globals
int cols;

char* vui_cmd;
char* vui_status;

char* vui_cmd_base;

int vui_showcmd_start = -1;
int vui_showcmd_end;
int vui_showcmd_cursor;

size_t cmd_base;
size_t cmd_crsrx;
size_t cmd_len;

// cmdline history functions
static vui_cmdline_hist_entry* vui_cmdline_hist_entry_new(vui_cmdline* cmdline)
{
	vui_cmdline_hist_entry* entry = vui_new(vui_cmdline_hist_entry);

	entry->prev = cmdline->hist_last_entry;

	if (cmdline->hist_last_entry != NULL)
	{
		cmdline->hist_last_entry->next = entry;
	}

	cmdline->hist_curr_entry = cmdline->hist_last_entry = entry;

	entry->next = NULL;

	vui_string_new_prealloc_at(&entry->line, cols*2);

	return entry;
}

static void vui_cmdline_hist_entry_kill(vui_cmdline_hist_entry* entry)
{
	// make prev and next point to each other instead of entry
	vui_cmdline_hist_entry* oldprev = entry->prev;
	vui_cmdline_hist_entry* oldnext = entry->next;

	if (oldnext != NULL)
	{
		oldnext->prev = oldprev;
	}

	if (oldprev != NULL)
	{
		oldprev->next = oldnext;
	}

	// free entry's line
	vui_string_dtor(&entry->line);

	// free entry
	vui_free(entry);
}

static void vui_cmdline_hist_entry_set(vui_cmdline_hist_entry* entry, char* str, size_t len)
{
	vui_string_setn(&entry->line, len, str);
	vui_string_get(&entry->line);
}

static void vui_cmdline_hist_entry_get(vui_cmdline_hist_entry* entry, char* str, size_t *len)
{
	size_t n = entry->line.n;
	memcpy(str, entry->line.s, n);

	*len = n;
}

static void vui_cmdline_hist_entry_shrink(vui_cmdline_hist_entry* entry)
{
	vui_string_shrink(&entry->line);
}

static void hist_last_entry_edit(vui_cmdline* cmdline)
{
	cmdline->cmd_modified = 1;

	if (cmdline->hist_curr_entry == cmdline->hist_last_entry)
	{
		return;
	}

	vui_cmdline_hist_entry_set(cmdline->hist_last_entry, cmdline->hist_curr_entry->line.s, cmdline->hist_curr_entry->line.n);

	cmdline->hist_curr_entry = cmdline->hist_last_entry;
}

static void hist_last_entry_clear(vui_cmdline* cmdline)
{
	cmdline->cmd_modified = 0;

	vui_string_reset(&cmdline->hist_last_entry->line);
}



// showcmd functions

static void vui_showcmd_putc(char c)
{
	if (vui_showcmd_start == -1) return;

	if (vui_showcmd_cursor <= vui_showcmd_end)
	{
		vui_status[vui_showcmd_cursor++] = c;
	}
	else
	{
		memmove(&vui_status[vui_showcmd_start], &vui_status[vui_showcmd_start + 1], vui_showcmd_end - vui_showcmd_start);
		vui_status[vui_showcmd_end] = c;
	}
}

void vui_showcmd_put(int c)
{
	if (c < 32)
	{
		vui_showcmd_putc('^');
		vui_showcmd_putc(c+'@');
	}
	else if (c >= 32 && c < 256)
	{
		vui_showcmd_putc(c);
	}
	else
	{
		vui_showcmd_putc('^');
		vui_showcmd_putc('?');
	}
}

void vui_showcmd_reset(void)
{
	if (vui_showcmd_start == -1) return;

	memset(&vui_status[vui_showcmd_start], ' ', vui_showcmd_end - vui_showcmd_start + 1);
	vui_showcmd_cursor = vui_showcmd_start;
}

void vui_showcmd_setup(int start, int length)
{
	int end = start + length - 1;

	if (vui_showcmd_start != -1)
	{
		int oldlen = vui_showcmd_end - vui_showcmd_start + 1;
		int newlen = end - start + 1;

		// TODO: fix me
	}


	vui_showcmd_cursor += start - vui_showcmd_start;
	vui_showcmd_start = start;
	vui_showcmd_end = end;

	vui_showcmd_reset();
}

vui_state* vui_tfunc_showcmd_put(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (act <= 0) return NULL;

	vui_showcmd_put(c);

	return NULL;
}


// misc callbacks
vui_state* vui_tfunc_normal(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (act <= 0) return NULL;

	if (act == VUI_ACT_RUN)
	{
		vui_reset();
	}
	else if (act == VUI_ACT_EMUL)
	{
		vui_showcmd_put(c);
	}

	return NULL;
}

vui_state* vui_tfunc_status_set(vui_state* currstate, unsigned int c, int act, void* data)
{
	char* s = data;

	if (act <= 0) return NULL;

	vui_status_set(s);

	return NULL;
}

vui_state* vui_tfunc_status_clear(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (act <= 0) return NULL;

	vui_status_clear();

	return NULL;
}



// init/resize
void vui_init(int width)
{
	vui_normal_mode = vui_mode_new(NULL, "normal", NULL, 0, NULL, NULL, NULL);
	vui_curr_state = vui_normal_mode->state;


	vui_transition* transition_normal = vui_transition_new_normal();

	for (unsigned int i=0; i<VUI_MAXSTATE; i++)
	{
		vui_set_char_t(vui_normal_mode->state, i, transition_normal);
	}

	vui_state_stack = vui_stack_refcount_new();
	vui_stack_def_set(vui_state_stack, vui_normal_mode->state);

	vui_normal_mode->state->push = vui_state_stack;

	cols = width;

	vui_cmd = vui_new_array(char, cols+1);
	vui_status = vui_new_array(char, cols+1);

	memset(vui_cmd, ' ', cols);
	memset(vui_status, ' ', cols);

	vui_cmd[cols] = 0;
	vui_status[cols] = 0;

	vui_crsrx = -1;

	vui_bar = vui_status;
}

void vui_deinit(void)
{
	vui_crsrx = -1;
	vui_bar = NULL;

	if (vui_register_container != NULL)
	{
		vui_gc_decr(vui_register_container);
	}

	vui_stack_refcount_deconvert(vui_state_stack);

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
	vui_debugf("vui_normal_mode->state->root = %d\n", vui_normal_mode->state->gc.root);
#endif

	vui_mode_kill(vui_normal_mode);

	vui_gc_run();
	vui_gc_run();

	vui_stack_kill(vui_state_stack);

	vui_free(vui_cmd);
	vui_free(vui_status);
}

void vui_resize(int width)
{
	// if the number of columns actually changed
	if (width != cols)
	{
		vui_cmd = vui_resize_array(vui_cmd, char, width + 1);
		vui_status = vui_resize_array(vui_status, char, width + 1);

		int minw, maxw;
		if (width > cols)
		{
			minw = cols;
			maxw = width;
		}
		else
		{
			minw = width;
			maxw = cols;
		}

		memset(&vui_cmd[minw], ' ', maxw-minw);
		memset(&vui_status[minw], ' ', maxw-minw);

		vui_cmd[cols] = 0;
		vui_status[cols] = 0;
		cols = width;
	}

}

// count

static vui_state* vui_tfunc_count_enter(vui_state* currstate, unsigned int c, int act, void* data)
{
	int* count = data;

	if (act <= 0) return NULL;

	vui_showcmd_put(c);

	*count = c - '0';

	return NULL;
}

static vui_state* vui_tfunc_count(vui_state* currstate, unsigned int c, int act, void* data)
{
	int* count = data;

	if (act <= 0) return NULL;

	vui_showcmd_put(c);

	*count = *count*10 + c - '0';

	return NULL;
}

void vui_count_init(void)
{
	vui_transition* transition_count_leave = vui_transition_run_c_s(vui_normal_mode->state);

	vui_count_mode = vui_state_new_t(transition_count_leave);
	vui_string_sets(&vui_count_mode->name, "vui_count_mode");

	vui_transition* transition_count = vui_transition_new3(vui_count_mode, vui_tfunc_count, &vui_count);
	vui_set_range_t(vui_count_mode, '0', '9', transition_count);

	vui_transition* transition_count_enter = vui_transition_new3(vui_count_mode, vui_tfunc_count_enter, &vui_count);
	vui_set_range_t(vui_normal_mode->state, '1', '9', transition_count_enter);
}

// macros

vui_state* state_macro_record;

static vui_state* vui_tfunc_macro_record(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (act <= 0) return vui_return(act);

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
	vui_debugf("record %c\n", c);
#endif

	vui_reset();

	vui_register_record(c);

	return vui_return(act);
}

unsigned int lastmacro = 0;

static vui_state* vui_tfunc_macro_execute(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (act <= 0) return vui_return(act);

	unsigned int c_bak = c;

	if (c == '@')
	{
		c = lastmacro;
	}

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
	vui_debugf("execute %c\n", c);
#endif

	int count = vui_count;
	vui_reset();

	if (count < 1) count = 1;

	vui_state* st = vui_return(VUI_ACT_TEST);

	while (count--)
	{
		st = vui_register_execute(st, c, act);
	}

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
	vui_debugf("execute %c done\n", c);
#endif

	if (c_bak != '@')
	{
		lastmacro = c;
	}

	vui_return(act);

	return st;
}

static vui_state* vui_tfunc_record_enter(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (act > 0)
	{
		vui_showcmd_put(c);
	}
	else if (act == VUI_ACT_GC_MARK)
	{
		vui_gc_mark(state_macro_record);
	}

	if (vui_register_recording == NULL) // begin recording
	{
		return state_macro_record;
	}
	else // end recording
	{
		if (act > 0)
		{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
			vui_debugf("end record\n");
#endif
			vui_register_endrecord();

			vui_reset();
		}

		return currstate;
	}
}

void vui_macro_init(const char* record, const char* execute)
{
	vui_transition* transition_macro_record = vui_transition_new3(vui_normal_mode->state, vui_tfunc_macro_record, NULL);
	state_macro_record = vui_state_new_t(transition_macro_record);

	vui_string_sets(&state_macro_record->name, record);

	vui_transition* transition_record_enter = vui_transition_new2(vui_tfunc_record_enter, NULL);

	vui_set_string_t(vui_normal_mode->state, record, transition_record_enter);

	vui_transition* transition_macro_execute = vui_transition_new3(vui_normal_mode->state, vui_tfunc_macro_execute, NULL);

	vui_state* state_macro_execute = vui_state_new_t(transition_macro_execute);

	vui_string_sets(&state_macro_execute->name, execute);

	vui_set_string_t(vui_normal_mode->state, execute, vui_transition_new_showcmd_put(state_macro_execute));
}


// registers

static vui_state* vui_tfunc_register_select(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_string** reg = data;

	if (act <= 0) return vui_return(act);

	vui_showcmd_put(c);

	*reg = vui_register_get(c);

	return vui_return(act);
}

void vui_register_init(void)
{
	if (vui_register_container != NULL) return;

	vui_register_container = vui_state_new_t(NULL);
	vui_register_recording = NULL;

	vui_gc_incr(vui_register_container);

	vui_register_select_mode = vui_state_new_t(vui_transition_new2(vui_tfunc_register_select, &vui_register_curr));
	vui_string_sets(&vui_register_select_mode->name, "vui_register_select_mode");

	vui_set_char_t(vui_normal_mode->state, '"', vui_transition_new_showcmd_put(vui_register_select_mode));
}

vui_string* vui_variable_check(char* s)
{
	return vui_lookup_s(vui_register_container, s)->data;
}

vui_string* vui_variable_get(char* s)
{
	vui_state* st = vui_lookup_s(vui_register_container, s);

	vui_string* reg = st->data;

	if (reg == NULL)
	{
		reg = vui_string_new();

		st->data = reg;
	}

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
	vui_string str;
	vui_string_new_at(&str);
	vui_string_append_quote(&str, reg);

	vui_debugf("variable %s: %s\n", s, vui_string_get(&str));

	vui_string_dtor(&str);
#endif

	return reg;
}

vui_string* vui_register_check(unsigned int c)
{
	char s[16];
	s[0] = '@';

	vui_utf8_encode(c, &s[1]);

	return vui_variable_check(s);
}

vui_string* vui_register_get(unsigned int c)
{
	char s[16];
	s[0] = '@';

	vui_utf8_encode(c, &s[1]);

	return vui_variable_get(s);
}

void vui_register_record(unsigned int c)
{
	vui_string* reg = vui_register_get(c);
	vui_string_reset(reg);

	vui_register_recording = reg;

	vui_string str;
	vui_string_new_at(&str);

	vui_string_puts(&str, "recording @");
	vui_string_putq(&str, c);

	vui_status_set(vui_string_get(&str));

	vui_string_dtor(&str);
}

void vui_register_endrecord(void)
{
	if (vui_register_recording != NULL)
	{
		vui_register_recording->n--;
		vui_string_get(vui_register_recording);	// append null terminator

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
		vui_string str;
		vui_string_new_at(&str);
		vui_string_append_quote(&str, vui_register_recording);

		vui_debugf("finished recording macro: %s\n", vui_string_get(&str));

		vui_string_dtor(&str);
#endif

		vui_register_recording = NULL;
	}

	vui_status_clear();
}

vui_state* vui_register_execute(vui_state* currstate, unsigned int c, int act)
{
	char s[16];
	s[0] = '@';

	vui_utf8_encode(c, &s[1]);

	vui_state* st = vui_lookup_s(vui_register_container, s);
	vui_string* reg = st->data;

	if (reg == NULL)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
		vui_debugf("nonexistent macro %c\n", c);
#endif
		return currstate;
	}

	st->data = NULL;

	vui_state* nextstate = vui_run_str(currstate, reg, act);

	st->data = reg;

	return nextstate;
}


// keybinds

// internal
void vui_bind_u(vui_mode* mode, unsigned int c, vui_transition* t)
{
	vui_set_char_t_u(mode->state_internal, c, t);
}

void vui_bind(vui_mode* mode, char* s, vui_transition* t)
{
	vui_set_string_t_mid(mode->state_internal, s, vui_transition_new_showcmd_put(NULL), t);
}

void vui_bind_str(vui_mode* mode, vui_string* s, vui_transition* t)
{
	vui_set_string_t_mid(mode->state_internal, vui_string_get(s), vui_transition_new_showcmd_put(NULL), t);
}

// external
void vui_bind_external_u(vui_mode* mode, unsigned int c, vui_transition* t)
{
	vui_set_char_t_u(mode->state, c, t);
}

void vui_bind_external(vui_mode* mode, char* s, vui_transition* t)
{
	vui_set_string_t_mid(mode->state, s, vui_transition_new_showcmd_put(NULL), t);
}

void vui_bind_external_str(vui_mode* mode, vui_string* s, vui_transition* t)
{
	vui_set_string_t_mid(mode->state, vui_string_get(s), vui_transition_new_showcmd_put(NULL), t);
}


// mappings
void vui_map(vui_mode* mode, vui_string* action, vui_string* reaction)
{
	vui_bind_external_str(mode, action, vui_transition_run_str_s(mode->state, reaction));
}

void vui_noremap(vui_mode* mode, vui_string* action, vui_string* reaction)
{
	vui_bind_external_str(mode, action, vui_transition_run_str_s(mode->state_internal, reaction));
}



// new modes

vui_mode* vui_mode_new(char* cmd, char* name, char* label, int flags, vui_transition* func_enter, vui_transition* func_in, vui_transition* func_exit)
{
	vui_mode* mode = vui_new(vui_mode);

	vui_state* state_internal;

	if (flags & VUI_MODE_NEW_INHERIT)
	{
		state_internal = vui_state_dup(vui_normal_mode->state);
	}
	else
	{
		state_internal = vui_state_new();
	}

	vui_string_sets(&state_internal->name, name);
	vui_string_puts(&state_internal->name, "_internal");
	vui_string_get(&state_internal->name); // append NULL terminator

	vui_gc_incr(state_internal);

	mode->state_internal = state_internal;

	vui_state* state = vui_state_new_t_self(vui_transition_run_c_p(&mode->state_curr_ptr));

	mode->state_curr_ptr->next = state_internal;

	vui_gc_incr(mode->state_curr_ptr);

	vui_string_sets(&state->name, name);
	vui_string_get(&state->name); // append NULL terminator

	vui_gc_incr(state);

	mode->state = state;

	vui_string_new_str_at(&mode->name, name);
	vui_string_new_str_at(&mode->label, label);

	if (func_in != NULL)
	{
		for (unsigned int i=0; i<VUI_MAXSTATE; i++)
		{
			vui_set_char_t(state_internal, i, func_in);
		}
	}

	if (func_enter != NULL)
	{
		if (func_enter->next == NULL) func_enter->next = state;

		if (func_enter->func == NULL)
		{
			func_enter->func = vui_tfunc_status_set;
			func_enter->data = label;
		}

		vui_set_string_t(state_internal, cmd, func_enter);
	}

	if (func_exit != NULL)
	{
		if (func_exit->next == NULL) func_exit->next = vui_normal_mode->state;

		if (func_exit->func == NULL)
		{
			func_exit->func = vui_tfunc_status_clear;
		}

		vui_set_char_t_u(state_internal, VUI_KEY_ESCAPE, func_exit);
	}

	state->push = vui_state_stack;

	return mode;
}


// cmdline callbacks
static vui_state* vui_tfunc_normal_to_cmd(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline* cmdline = data;

	if (act <= 0) return currstate;


	vui_cmd_base = vui_cmd;
	cmd_base = 0;

	vui_string* label = &cmdline->mode->label;
	for (size_t i = 0; i < label->n; i++)
	{
		*vui_cmd_base++ = label->s[i];
		cmd_base++;
	}

	cmd_len = 0;
	cmd_crsrx = 0;

	cmdline->cmd_modified = 0;

	cmdline->hist_curr_entry = cmdline->hist_last_entry;

	memset(vui_cmd_base, ' ', cols - cmd_base);


	vui_bar = vui_cmd;
	vui_crsrx = cmd_base + cmd_crsrx;

	return currstate;
}

static vui_state* vui_tfunc_cmd_key(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline* cmdline = data;

	if (act <= 0) return vui_return(act);

	hist_last_entry_edit(cmdline);

	// TODO: make special characters display properly

	// if cursor is not at EOL, shift line right
	if (vui_crsrx < cmd_base + cmd_len)
	{
		memmove(&vui_cmd_base[cmd_crsrx + 1], &vui_cmd_base[cmd_crsrx], cmd_len - cmd_crsrx);
	}
	vui_cmd_base[cmd_crsrx++] = c;
	cmd_len++;

	vui_bar = vui_cmd;
	vui_crsrx = cmd_base + cmd_crsrx;

	return vui_return(act);
}

static vui_state* vui_tfunc_cmd_backspace(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline* cmdline = data;

	if (act <= 0)
	{
		return vui_return_n(act, (cmd_len) ? 0 : 1);
	}


	if (!cmd_len)
	{
		vui_bar = vui_status;
		vui_crsrx = -1;
		return vui_return_n(act, 1);
	}

	if (cmd_crsrx > 0)
	{
		hist_last_entry_edit(cmdline);

		cmd_crsrx--;
		memmove(&vui_cmd_base[cmd_crsrx], &vui_cmd_base[cmd_crsrx+1], cmd_len - cmd_crsrx);
		vui_cmd_base[cmd_len] = ' ';
		cmd_len--;

		if (cmd_len)
		{
			hist_last_entry_clear(cmdline);
		}
	}


	vui_bar = vui_cmd;
	vui_crsrx = cmd_base + cmd_crsrx;

	return vui_return(act);
}

static vui_state* vui_tfunc_cmd_delete(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline* cmdline = data;

	if (act <= 0) return vui_return(act);


	if (cmd_crsrx < cmd_len)
	{
		hist_last_entry_edit(cmdline);

		vui_cmd_base[cmd_len] = ' ';
		memmove(&vui_cmd_base[cmd_crsrx], &vui_cmd[vui_crsrx+1], cmd_len - cmd_crsrx);
		cmd_len--;

		if (cmd_len)
		{
			hist_last_entry_clear(cmdline);
		}
	}

	vui_bar = vui_cmd;
	vui_crsrx = cmd_base + cmd_crsrx;

	return vui_return(act);
}

static vui_state* vui_tfunc_cmd_left(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline* cmdline = data;

	if (act <= 0) return vui_return(act);


	if (cmd_crsrx > 0)
	{
		cmd_crsrx--;
	}


	vui_bar = vui_cmd;
	vui_crsrx = cmd_base + cmd_crsrx;

	return vui_return(act);
}

static vui_state* vui_tfunc_cmd_right(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline* cmdline = data;

	if (act <= 0) return vui_return(act);


	if (cmd_crsrx < cmd_len)
	{
		cmd_crsrx++;
	}


	vui_bar = vui_cmd;
	vui_crsrx = cmd_base + cmd_crsrx;

	return vui_return(act);
}

static vui_state* vui_tfunc_cmd_up(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline* cmdline = data;

	if (act <= 0) return vui_return(act);


	if (cmdline->hist_curr_entry->prev != NULL && !cmdline->cmd_modified)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
		if (cmdline->hist_curr_entry->prev == cmdline->hist_curr_entry)
		{
			vui_debugf("own prev!\n");
		}
#endif

		cmdline->hist_curr_entry = cmdline->hist_curr_entry->prev;

		vui_cmdline_hist_entry_get(cmdline->hist_curr_entry, vui_cmd_base, &cmd_len);

		cmd_crsrx = cmd_len;
		memset(&vui_cmd_base[cmd_crsrx], ' ', cols - cmd_base);
	}
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
	else
	{
		vui_debugf("no prev\n");
	}
#endif

	vui_bar = vui_cmd;
	vui_crsrx = cmd_base + cmd_crsrx;

	return vui_return(act);
}

static vui_state* vui_tfunc_cmd_down(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline* cmdline = data;

	if (act <= 0) return vui_return(act);


	if (cmdline->hist_curr_entry->next != NULL && !cmdline->cmd_modified)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
		if (cmdline->hist_curr_entry->next == cmdline->hist_curr_entry)
		{
			vui_debugf("own next!\n");
		}
#endif

		cmdline->hist_curr_entry = cmdline->hist_curr_entry->next;

		vui_cmdline_hist_entry_get(cmdline->hist_curr_entry, vui_cmd_base, &cmd_len);

		cmd_crsrx = cmd_len;
		memset(&vui_cmd_base[cmd_crsrx], ' ', cols - cmd_base);
	}
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
	else
	{
		vui_debugf("no next\n");
	}
#endif

	vui_bar = vui_cmd;
	vui_crsrx = cmd_base + cmd_crsrx;

	return vui_return(act);
}

static vui_state* vui_tfunc_cmd_home(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline* cmdline = data;

	if (act <= 0) return vui_return(act);


	cmd_crsrx = 0;


	vui_bar = vui_cmd;
	vui_crsrx = cmd_base + cmd_crsrx;

	return vui_return(act);
}

static vui_state* vui_tfunc_cmd_end(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline* cmdline = data;

	if (act <= 0) return vui_return(act);


	cmd_crsrx = cmd_len;


	vui_bar = vui_cmd;
	vui_crsrx = cmd_base + cmd_crsrx;

	return vui_return(act);
}

static vui_state* vui_tfunc_cmd_escape(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline* cmdline = data;

	if (act <= 0) return vui_return_n(act, 1);


	if (cmdline->hist_curr_entry != cmdline->hist_last_entry && !cmdline->cmd_modified)
	{
		vui_cmdline_hist_entry_kill(cmdline->hist_curr_entry);
		cmdline->hist_curr_entry = cmdline->hist_last_entry;
	}

	vui_cmdline_hist_entry_set(cmdline->hist_curr_entry, vui_cmd_base, cmd_len);


	vui_cmdline_hist_entry_shrink(cmdline->hist_curr_entry);

	if (cmdline->hist_curr_entry->line.n > 0)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
		vui_debugf("new entry\n");
#endif

		vui_cmdline_hist_entry_new(cmdline);
	}
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
	else
	{
		vui_debugf("no new entry\n");
	}
#endif

	vui_bar = vui_status;
	vui_crsrx = -1;

	return vui_return_n(act, 1);
}

static vui_state* vui_tfunc_cmd_enter(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline* cmdline = data;

	if (act <= 0) return vui_return_n(act, 1);


	if (cmdline->hist_curr_entry != cmdline->hist_last_entry && !cmdline->cmd_modified)
	{
		vui_cmdline_hist_entry_kill(cmdline->hist_curr_entry);
		cmdline->hist_curr_entry = cmdline->hist_last_entry;
	}

	vui_cmdline_hist_entry_set(cmdline->hist_curr_entry, vui_cmd_base, cmd_len);

	cmdline->on_submit(vui_tr_run_str(cmdline->tr, &cmdline->hist_curr_entry->line));


	vui_cmdline_hist_entry_shrink(cmdline->hist_curr_entry);

	if (cmdline->hist_curr_entry->line.n > 0)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
		vui_debugf("new entry\n");
#endif

		vui_cmdline_hist_entry_new(cmdline);
	}
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
	else
	{
		vui_debugf("no new entry\n");
	}
#endif

	vui_bar = vui_status;
	vui_crsrx = -1;

	return vui_return_n(act, 1);
}

static vui_state* vui_tfunc_cmd_prepaste(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (act <= 0) return vui_return(act);

	vui_tfunc_cmd_key(currstate, '"', act, data);

	return vui_return(act);
}

static vui_state* vui_tfunc_cmd_paste(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline* cmdline = data;

	if (act <= 0) return vui_return(act);

	vui_tfunc_cmd_backspace(currstate, c, act, data);

	vui_string* reg = vui_register_get(c);

	if (reg != NULL)
	{
		vui_state* cmdline_state = vui_return(VUI_ACT_TEST);

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
		vui_debugf("paste %c\n", c);
#endif
		for (size_t i = 0; i < reg->n; i++)
		{
			vui_tfunc_cmd_key(cmdline_state, reg->s[i], act, data);
		}
	}
	else
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
		vui_debugf("paste empty %c\n", c);
#endif
	}

	return vui_return(act);
}

static vui_state* vui_tfunc_cmd_paste_cancel(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (act <= 0) return vui_return(act);

	vui_tfunc_cmd_backspace(currstate, c, act, data);

	return vui_return(act);
}

vui_cmdline* vui_cmdline_new(char* cmd, char* name, char* label, vui_tr* tr, vui_cmdline_submit_callback on_submit)
{
	vui_cmdline* cmdline = vui_new(vui_cmdline);
	cmdline->on_submit = on_submit;
	cmdline->hist_last_entry = NULL;

	vui_mode* cmdline_mode = vui_mode_new(cmd, name, label, 0, NULL, NULL, NULL);

	cmdline->mode = cmdline_mode;

	vui_state* cmdline_state = cmdline_mode->state_internal;
	vui_state* mode_state = cmdline_mode->state;

	cmdline_state->push = vui_state_stack;

	if (tr != NULL)
	{
		cmdline->tr = tr;
	}
	else
	{
		cmdline->tr = vui_tr_new_identity();
	}

	vui_transition* transition_normal_to_cmd = vui_transition_new3(cmdline_state, vui_tfunc_normal_to_cmd, cmdline);

	vui_set_string_t(vui_normal_mode->state, cmd, transition_normal_to_cmd);

	vui_transition* transition_cmd_key = vui_transition_new3(cmdline_state, vui_tfunc_cmd_key, cmdline);
	vui_transition* transition_cmd_escape = vui_transition_new3(NULL, vui_tfunc_cmd_escape, cmdline);
	vui_transition* transition_cmd_enter = vui_transition_new3(NULL, vui_tfunc_cmd_enter, cmdline);
	vui_transition* transition_cmd_backspace = vui_transition_new2(vui_tfunc_cmd_backspace, cmdline);

	vui_transition* transition_cmd_up = vui_transition_new3(cmdline_state, vui_tfunc_cmd_up, cmdline);
	vui_transition* transition_cmd_down = vui_transition_new3(cmdline_state, vui_tfunc_cmd_down, cmdline);
	vui_transition* transition_cmd_left = vui_transition_new3(cmdline_state, vui_tfunc_cmd_left, cmdline);
	vui_transition* transition_cmd_right = vui_transition_new3(cmdline_state, vui_tfunc_cmd_right, cmdline);
	vui_transition* transition_cmd_delete = vui_transition_new3(cmdline_state, vui_tfunc_cmd_delete, cmdline);
	vui_transition* transition_cmd_home = vui_transition_new3(cmdline_state, vui_tfunc_cmd_home, cmdline);
	vui_transition* transition_cmd_end = vui_transition_new3(cmdline_state, vui_tfunc_cmd_end, cmdline);

	vui_transition* transition_macro_paste = vui_transition_new3(cmdline_state, vui_tfunc_cmd_paste, cmdline);
	vui_state* paste_state = vui_state_new_t(transition_macro_paste);

	vui_transition* transition_paste_cancel = vui_transition_new3(cmdline_state, vui_tfunc_cmd_paste_cancel, cmdline);
	vui_set_char_t_u(paste_state, VUI_KEY_ESCAPE, transition_paste_cancel);
	vui_set_char_t_u(paste_state, VUI_KEY_ENTER, transition_paste_cancel);
	vui_set_char_t_u(paste_state, VUI_KEY_BACKSPACE, transition_paste_cancel);

	for (unsigned int i=32; i<127; i++)
	{
		vui_set_char_t(cmdline_state, i, transition_cmd_key);
	}

	vui_set_char_t_u(cmdline_state, VUI_KEY_ESCAPE, transition_cmd_escape);
	vui_set_char_t_u(cmdline_state, VUI_KEY_ENTER, transition_cmd_enter);
	vui_set_char_t_u(cmdline_state, VUI_KEY_BACKSPACE, transition_cmd_backspace);

	vui_set_char_t_u(cmdline_state, VUI_KEY_UP, transition_cmd_up);
	vui_set_char_t_u(cmdline_state, VUI_KEY_DOWN, transition_cmd_down);
	vui_set_char_t_u(cmdline_state, VUI_KEY_LEFT, transition_cmd_left);
	vui_set_char_t_u(cmdline_state, VUI_KEY_RIGHT, transition_cmd_right);
	vui_set_char_t_u(cmdline_state, VUI_KEY_DELETE, transition_cmd_delete);
	vui_set_char_t_u(cmdline_state, VUI_KEY_HOME, transition_cmd_home);
	vui_set_char_t_u(cmdline_state, VUI_KEY_END, transition_cmd_end);
	vui_set_char_t_u(cmdline_state, 'E' + VUI_KEY_MODIFIER_CONTROL, transition_cmd_end);

	vui_set_char_t_u(cmdline_state, 'R' + VUI_KEY_MODIFIER_CONTROL, vui_transition_new3(paste_state, vui_tfunc_cmd_prepaste, cmdline));

#if 0
	vui_state* keyescapestate = vui_state_new_t(transition_cmd_key);

	vui_set_char_s_u(cmdline_state, 22, keyescapestate);
#endif

	vui_cmdline_hist_entry_new(cmdline);

	return cmdline;
}

void vui_mode_kill(vui_mode* mode)
{
	vui_gc_decr(mode->state);
	vui_gc_decr(mode->state_internal);

	vui_gc_decr(mode->state_curr_ptr);

	vui_string_dtor(&mode->name);
	vui_string_dtor(&mode->label);

	vui_free(mode);
}

void vui_cmdline_kill(vui_cmdline* cmdline)
{
	vui_cmdline_hist_entry* hist_entry = cmdline->hist_last_entry;

	while (hist_entry != NULL)
	{
		vui_cmdline_hist_entry* last = hist_entry;

		hist_entry = last->prev;

		vui_cmdline_hist_entry_kill(last);
	}

	vui_mode_kill(cmdline->mode);
	vui_tr_kill(cmdline->tr);

	vui_free(cmdline);
}

void vui_status_set(const char* s)
{
	vui_bar = vui_status;
	char* p = vui_status;
	while (*s)
	{
		*p++ = *s++;
	}
	while (p < &vui_status[cols])
	{
		*p++ = ' ';
	}
}

void vui_status_clear(void)
{
	memset(vui_status, ' ', cols);
}

// input
void vui_input(unsigned int c)
{
	if (vui_register_recording != NULL)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
		vui_debugf("register_putc('%c' %#02x)\n", c, c);
#endif
		vui_string_put(vui_register_recording, c);
	}

	vui_state* new_state = vui_next_u(vui_curr_state, c, VUI_ACT_RUN);

	if (new_state != NULL)
	{
		vui_curr_state = new_state;
	}

	if (vui_curr_state->push != NULL)
	{
		vui_stack_push_nodup(vui_curr_state->push, vui_curr_state);
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STATEMACHINE)
		vui_debugf("push %d\n", vui_curr_state->push->n);
#endif
	}
}

void vui_reset(void)
{
	vui_showcmd_reset();
	vui_count = 0;
	vui_register_curr = vui_register_get('"');
}
