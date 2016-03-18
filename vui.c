#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <string.h>

#include "vui_debug.h"

#include "vui_utf8.h"
#include "vui_gc.h"

#include "vui.h"

// extern globals
char* vui_bar;
int vui_crsrx;

int vui_count;

vui_stack* vui_state_stack;

vui_state* vui_curr_state;

vui_state* vui_normal_mode;
vui_state* vui_count_mode;
vui_state* vui_register_select_mode;

vui_state* vui_register_container; // register container state

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
static hist_entry* hist_entry_new(vui_cmdline* cmdline)
{
	hist_entry* entry = malloc(sizeof(hist_entry));

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

static void hist_entry_kill(hist_entry* entry)
{
	// make prev and next point to each other instead of entry
	hist_entry* oldprev = entry->prev;
	hist_entry* oldnext = entry->next;

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
	free(entry);
}

static void hist_entry_set(hist_entry* entry, char* str, size_t len)
{
	vui_string_reset(&entry->line);
	vui_string_putn(&entry->line, len, str);
	vui_string_get(&entry->line);
}

static void hist_entry_get(hist_entry* entry, char* str, size_t *len)
{
	size_t n = entry->line.n;
	memcpy(str, entry->line.s, n);

	*len = n;
}

static void hist_entry_shrink(hist_entry* entry)
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

	hist_entry_set(cmdline->hist_last_entry, cmdline->hist_curr_entry->line.s, cmdline->hist_curr_entry->line.n);

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
	vui_normal_mode = vui_curr_state = vui_state_new();
	vui_string_reset(&vui_normal_mode->name);
	vui_string_puts(&vui_normal_mode->name, "vui_normal_mode");

	vui_gc_incr(vui_normal_mode);

	vui_transition transition_normal = vui_transition_new_normal();

	for (unsigned int i=0; i<VUI_MAXSTATE; i++)
	{
		vui_set_char_t(vui_normal_mode, i, transition_normal);
	}

	vui_state_stack = vui_state_stack_new();
	vui_state_stack->def = vui_normal_mode;

	vui_normal_mode->push = vui_state_stack;

	cols = width;

	vui_cmd = malloc((cols+1)*sizeof(char));
	vui_status = malloc((cols+1)*sizeof(char));

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

	vui_gc_decr(vui_normal_mode);

	vui_stack_reset(vui_state_stack);

	vui_gc_run();

	vui_stack_kill(vui_state_stack);

	free(vui_cmd);
	free(vui_status);
}

void vui_resize(int width)
{
	// if the number of columns actually changed
	if (width != cols)
	{
		vui_cmd = realloc(vui_cmd, (width+1)*sizeof(char));
		vui_status = realloc(vui_status, (width+1)*sizeof(char));

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
	vui_transition transition_count_leave = vui_transition_run_c_s(vui_normal_mode);

	vui_count_mode = vui_state_new_t(transition_count_leave);
	vui_string_reset(&vui_count_mode->name);
	vui_string_puts(&vui_count_mode->name, "vui_count_mode");

	vui_transition transition_count = vui_transition_new3(vui_count_mode, vui_tfunc_count, &vui_count);
	vui_set_range_t(vui_count_mode, '0', '9', transition_count);

	vui_transition transition_count_enter = vui_transition_new3(vui_count_mode, vui_tfunc_count_enter, &vui_count);
	vui_set_range_t(vui_normal_mode, '1', '9', transition_count_enter);
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
	else if (act == VUI_ACT_GC)
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

void vui_macro_init(unsigned int record, unsigned int execute)
{
	vui_transition transition_macro_record = vui_transition_new3(vui_normal_mode, vui_tfunc_macro_record, NULL);
	state_macro_record = vui_state_new_t(transition_macro_record);

	char sr[16];
	vui_utf8_encode(record, sr);
	vui_string_reset(&state_macro_record->name);
	vui_string_puts(&state_macro_record->name, sr);

	vui_transition transition_record_enter = vui_transition_new2(vui_tfunc_record_enter, NULL);

	vui_set_char_t_u(vui_normal_mode, record, transition_record_enter);

	vui_transition transition_macro_execute = vui_transition_new3(vui_normal_mode, vui_tfunc_macro_execute, NULL);

	vui_state* state_macro_execute = vui_state_new_t(transition_macro_execute);

	char se[16];
	vui_utf8_encode(execute, se);
	vui_string_reset(&state_macro_execute->name);
	vui_string_puts(&state_macro_execute->name, se);

	vui_set_char_t_u(vui_normal_mode, execute, vui_transition_new_showcmd_put(state_macro_execute));
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
	vui_register_container = vui_state_new_s(NULL);
	vui_register_recording = NULL;

	vui_register_select_mode = vui_state_new_t(vui_transition_new2(vui_tfunc_register_select, &vui_register_curr));
	vui_string_reset(&vui_register_select_mode->name);
	vui_string_puts(&vui_register_select_mode->name, "vui_register_select_mode");

	vui_set_char_t(vui_normal_mode, '"', vui_transition_new_showcmd_put(vui_register_select_mode));
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
	vui_string_put(&str, c);

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

void vui_bind_u(vui_state* mode, unsigned int c, vui_transition t)
{
	vui_set_char_t_u(mode, c, t);
}

void vui_bind(vui_state* mode, char* s, vui_transition t)
{
	vui_set_string_t_mid(mode, s, vui_transition_new_showcmd_put(NULL), t);
}

void vui_bind_str(vui_state* mode, vui_string* s, vui_transition t)
{
	vui_set_string_t_mid(mode, vui_string_get(s), vui_transition_new_showcmd_put(NULL), t);
}


void vui_map(vui_state* mode, vui_string* action, vui_string* reaction)
{
	vui_bind_str(mode, action, vui_transition_run_str_s(mode, reaction));
}

void vui_map2(vui_state* mode, vui_string* action, vui_state* reaction_st, vui_string* reaction_str)
{
	vui_bind_str(mode, action, vui_transition_run_str_s(reaction_st, reaction_str));
}



// new modes

vui_state* vui_mode_new(char* cmd, char* name, char* label, int mode, vui_transition func_enter, vui_transition func_in, vui_transition func_exit)
{
	vui_state* state;

	if (mode & VUI_MODE_NEW_INHERIT)
	{
		state = vui_state_dup(vui_normal_mode);
	}
	else
	{
		state = vui_state_new();
	}

	vui_string_reset(&state->name);
	vui_string_puts(&state->name, name);

	if (func_enter.next == NULL) func_enter.next = state;

	if (func_enter.func == NULL)
	{
		func_enter.func = vui_tfunc_status_set;
		func_enter.data = label;
	}

	if (!(mode & VUI_MODE_NEW_MANUAL_IN))
	{
		if (func_in.next == NULL) func_in.next = state;
	}

	if (func_exit.next == NULL) func_exit.next = vui_normal_mode;

	if (func_exit.func == NULL)
	{
		func_exit.func = vui_tfunc_status_clear;
	}

	for (unsigned int i=0; i<VUI_MAXSTATE; i++)
	{
		vui_set_char_t(state, i, func_in);
	}

	vui_set_string_t(vui_normal_mode, cmd, func_enter);
	vui_set_char_t_u(state, VUI_KEY_ESCAPE, func_exit);

	state->push = vui_state_stack;

	return state;
}


// cmdline callbacks
static vui_state* vui_tfunc_normal_to_cmd(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline* cmdline = data;

	if (act <= 0) return cmdline->cmdline_state;


	vui_cmd_base = vui_cmd;
	cmd_base = 0;

	char* label = cmdline->label.s;
	while (*label)
	{
		*vui_cmd_base++ = *label++;
		cmd_base++;
	}

	cmd_len = 0;
	cmd_crsrx = 0;

	cmdline->cmd_modified = 0;

	cmdline->hist_curr_entry = cmdline->hist_last_entry;

	memset(vui_cmd_base, ' ', cols - cmd_base);


	vui_bar = vui_cmd;
	vui_crsrx = cmd_base + cmd_crsrx;

	return cmdline->cmdline_state;
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

		hist_entry_get(cmdline->hist_curr_entry, vui_cmd_base, &cmd_len);

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

		hist_entry_get(cmdline->hist_curr_entry, vui_cmd_base, &cmd_len);

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
		hist_entry_kill(cmdline->hist_curr_entry);
		cmdline->hist_curr_entry = cmdline->hist_last_entry;
	}

	hist_entry_set(cmdline->hist_curr_entry, vui_cmd_base, cmd_len);


	hist_entry_shrink(cmdline->hist_curr_entry);

	if (cmdline->hist_curr_entry->line.n > 0)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
		vui_debugf("new entry\n");
#endif

		hist_entry_new(cmdline);
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
		hist_entry_kill(cmdline->hist_curr_entry);
		cmdline->hist_curr_entry = cmdline->hist_last_entry;
	}

	hist_entry_set(cmdline->hist_curr_entry, vui_cmd_base, cmd_len);

	cmdline->on_submit(vui_translator_run_str(cmdline->tr, &cmdline->hist_curr_entry->line));


	hist_entry_shrink(cmdline->hist_curr_entry);

	if (cmdline->hist_curr_entry->line.n > 0)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
		vui_debugf("new entry\n");
#endif

		hist_entry_new(cmdline);
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

vui_cmdline* vui_cmdline_new(char* cmd, char* name, char* label, vui_translator* tr, vui_cmdline_submit_callback on_submit)
{
	vui_cmdline* cmdline = malloc(sizeof(vui_cmdline));
	cmdline->on_submit = on_submit;
	cmdline->hist_last_entry = NULL;

	vui_string_new_at(&cmdline->label);
	vui_string_puts(&cmdline->label, label);
	vui_string_get(&cmdline->label);

	if (tr != NULL)
	{
		cmdline->tr = tr;
	}
	else
	{
		cmdline->tr = vui_translator_new_identity();
	}

	vui_state* cmdline_state = vui_state_new();
	vui_string_reset(&cmdline_state->name);
	vui_string_puts(&cmdline_state->name, name);

	vui_gc_incr(cmdline_state);

	cmdline_state->push = vui_state_stack;

	vui_transition transition_normal_to_cmd = vui_transition_new3(cmdline_state, vui_tfunc_normal_to_cmd, cmdline);

	vui_set_string_t(vui_normal_mode, cmd, transition_normal_to_cmd);

	vui_transition transition_cmd_key = vui_transition_new3(cmdline_state, vui_tfunc_cmd_key, cmdline);
	vui_transition transition_cmd_escape = vui_transition_new3(NULL, vui_tfunc_cmd_escape, cmdline);
	vui_transition transition_cmd_enter = vui_transition_new3(NULL, vui_tfunc_cmd_enter, cmdline);
	vui_transition transition_cmd_backspace = vui_transition_new2(vui_tfunc_cmd_backspace, cmdline);

	vui_transition transition_cmd_up = vui_transition_new3(cmdline_state, vui_tfunc_cmd_up, cmdline);
	vui_transition transition_cmd_down = vui_transition_new3(cmdline_state, vui_tfunc_cmd_down, cmdline);
	vui_transition transition_cmd_left = vui_transition_new3(cmdline_state, vui_tfunc_cmd_left, cmdline);
	vui_transition transition_cmd_right = vui_transition_new3(cmdline_state, vui_tfunc_cmd_right, cmdline);
	vui_transition transition_cmd_delete = vui_transition_new3(cmdline_state, vui_tfunc_cmd_delete, cmdline);
	vui_transition transition_cmd_home = vui_transition_new3(cmdline_state, vui_tfunc_cmd_home, cmdline);
	vui_transition transition_cmd_end = vui_transition_new3(cmdline_state, vui_tfunc_cmd_end, cmdline);

	vui_transition transition_macro_paste = vui_transition_new3(cmdline_state, vui_tfunc_cmd_paste, cmdline);
	vui_state* paste_state = vui_state_new_t(transition_macro_paste);

	vui_transition transition_paste_cancel = vui_transition_new3(cmdline_state, vui_tfunc_cmd_paste_cancel, cmdline);
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

	cmdline->cmdline_state = cmdline_state;

	hist_entry_new(cmdline);

	return cmdline;
}

void vui_cmdline_kill(vui_cmdline* cmdline)
{
	vui_gc_decr(cmdline->cmdline_state);
	vui_string_dtor(&cmdline->label);
	vui_translator_kill(cmdline->tr);
	free(cmdline);
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
		vui_debugf("register_putc('%c' 0x%02x)\n", c, c);
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
		vui_state_stack_push_nodup(vui_curr_state->push, vui_curr_state);
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
