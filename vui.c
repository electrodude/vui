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

vui_state* vui_register_container; // register container state

vui_string* vui_register_recording;

// non-extern globals
int cols;

char* vui_cmd;
char* vui_status;

int vui_showcmd_start = -1;
int vui_showcmd_end;
int vui_showcmd_cursor;

int cmd_base;
int cmd_len;

// cmdline history functions
static hist_entry* hist_entry_new(vui_cmdline_def* cmdline)
{
	hist_entry* entry = malloc(sizeof(hist_entry));

	entry->prev = cmdline->hist_last_entry;

	if (cmdline->hist_last_entry != NULL)
	{
		cmdline->hist_last_entry->next = entry;
	}

	cmdline->hist_curr_entry = cmdline->hist_last_entry = entry;

	entry->next = NULL;

	entry->len = 1;
	entry->maxlen = cols*2;
	entry->line = malloc(entry->maxlen);

	entry->line[0] = 0;

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
	free(entry->line);

	// free entry
	free(entry);
}

static void hist_entry_set(hist_entry* entry, char* str, int len)
{
	entry->len = len + 1;
	if (entry->maxlen < entry->len)
	{
		entry->maxlen = entry->len*2;
		entry->line = realloc(entry->line, entry->maxlen);
	}

	memcpy(entry->line, str, entry->len);

	entry->line[len] = 0;
}

static void hist_entry_shrink(hist_entry* entry)
{
	entry->maxlen = entry->len;

	entry->line = realloc(entry->line, entry->maxlen);
}

static void hist_last_entry_edit(vui_cmdline_def* cmdline)
{
	cmdline->cmd_modified = 1;

	if (cmdline->hist_curr_entry == cmdline->hist_last_entry)
	{
		return;
	}

	hist_entry_set(cmdline->hist_last_entry, cmdline->hist_curr_entry->line, cmdline->hist_curr_entry->len);

	cmdline->hist_curr_entry = cmdline->hist_last_entry;
}

static void hist_last_entry_clear(vui_cmdline_def* cmdline)
{
	cmdline->cmd_modified = 0;

	cmdline->hist_last_entry->len = 1;
	cmdline->hist_last_entry->line[0] = 0;
}


// state machine helpers

void vui_map(vui_state* mode, char* action, char* reaction)
{
	vui_set_string_t(mode, action, vui_transition_run_s_s(mode, reaction));
}

void vui_map2(vui_state* mode, char* action, vui_state* reaction_st, char* reaction_str)
{
	vui_set_string_t(mode, action, vui_transition_run_s_s(reaction_st, reaction_str));
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

	}


	vui_showcmd_cursor += start - vui_showcmd_start;
	vui_showcmd_start = start;
	vui_showcmd_end = end;

	vui_showcmd_reset();
}


// misc callbacks
static vui_state* tfunc_normal(vui_state* currstate, unsigned int c, int act, void* data)
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

static vui_state* tfunc_showcmd_put(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (act <= 0) return NULL;

	vui_showcmd_put(c);

	return NULL;
}

static vui_state* tfunc_status_set(vui_state* currstate, unsigned int c, int act, void* data)
{
	char* s = data;

	if (act <= 0) return NULL;

	vui_status_set(s);

	return NULL;
}

static vui_state* tfunc_status_clear(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (act <= 0) return NULL;

	vui_status_clear();

	return NULL;
}


// cmdline callbacks
static vui_state* tfunc_normal_to_cmd(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline_def* cmdline = data;

	if (act <= 0) return cmdline->cmdline_state;

	vui_bar = vui_cmd;
	vui_crsrx = 0;

	char* label = cmdline->label;
	while (*label)
	{
		vui_cmd[vui_crsrx++] = *label++;
	}

	cmd_base = vui_crsrx;

	cmd_len = cmd_base - 1;

	cmdline->cmd_modified = 0;

	cmdline->hist_curr_entry = cmdline->hist_last_entry;

	memset(&vui_cmd[cmd_base], ' ', cols - cmd_base);

	return cmdline->cmdline_state;
}

static vui_state* tfunc_cmd_key(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline_def* cmdline = data;

	if (act <= 0) return NULL;

	vui_bar = vui_cmd;

	hist_last_entry_edit(cmdline);

	if (vui_crsrx != cmd_len)
	{
		memmove(&vui_cmd[vui_crsrx + 1], &vui_cmd[vui_crsrx], cmd_len - vui_crsrx + 1);
	}
	vui_cmd[vui_crsrx++] = c;
	cmd_len++;

	return NULL;
}

static vui_state* tfunc_cmd_backspace(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline_def* cmdline = data;

	if (act <= 0)
	{
		if (act == VUI_ACT_GC)
		{
			vui_gc_mark(vui_normal_mode);
		}

		return (vui_crsrx <= cmd_base) ? NULL : vui_normal_mode;
	}

	if (cmd_len <= 0)
	{
		vui_bar = vui_status;
		vui_crsrx = -1;
		return vui_normal_mode;
	}

	vui_bar = vui_cmd;

	if (vui_crsrx > cmd_base)
	{
		hist_last_entry_edit(cmdline);

		vui_cmd[cmd_len+1] = ' ';
		vui_crsrx--;
		memmove(&vui_cmd[vui_crsrx], &vui_cmd[vui_crsrx+1], cmd_len-vui_crsrx+1);
		cmd_len--;

		if (!cmd_len)
		{
			hist_last_entry_clear(cmdline);
		}
	}

	return NULL;
}

static vui_state* tfunc_cmd_delete(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline_def* cmdline = data;

	if (act <= 0) return NULL;

	vui_bar = vui_cmd;

	if (vui_crsrx <= cmd_len)
	{
		hist_last_entry_edit(cmdline);

		vui_cmd[cmd_len+1] = ' ';
		memmove(&vui_cmd[vui_crsrx], &vui_cmd[vui_crsrx+1], cmd_len-vui_crsrx+1);
		cmd_len--;

		if (!cmd_len)
		{
			hist_last_entry_clear(cmdline);
		}
	}

	return NULL;
}

static vui_state* tfunc_cmd_left(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline_def* cmdline = data;

	if (act <= 0) return NULL;

	vui_bar = vui_cmd;

	if (vui_crsrx > cmd_base)
	{
		vui_crsrx--;
	}

	return NULL;
}

static vui_state* tfunc_cmd_right(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline_def* cmdline = data;

	if (act <= 0) return NULL;

	vui_bar = vui_cmd;

	if (vui_crsrx <= cmd_len)
	{
		vui_crsrx++;
	}

	return NULL;
}

static vui_state* tfunc_cmd_up(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline_def* cmdline = data;

	if (act <= 0) return NULL;

	vui_bar = vui_cmd;

	if (cmdline->hist_curr_entry->prev != NULL && !cmdline->cmd_modified)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
		if (cmdline->hist_curr_entry->prev == cmdline->hist_curr_entry)
		{
			vui_debugf("own prev!\n");
		}
#endif

		cmdline->hist_curr_entry = cmdline->hist_curr_entry->prev;

		memcpy(&vui_cmd[cmd_base], cmdline->hist_curr_entry->line, cmdline->hist_curr_entry->len);
		vui_crsrx = cmd_base + cmdline->hist_curr_entry->len - 1;
		cmd_len = vui_crsrx - 1;
		memset(&vui_cmd[vui_crsrx], ' ', cols - vui_crsrx);
	}
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
	else
	{
		vui_debugf("no prev\n");
	}
#endif

	return NULL;
}

static vui_state* tfunc_cmd_down(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline_def* cmdline = data;

	if (act <= 0) return NULL;

	vui_bar = vui_cmd;

	if (cmdline->hist_curr_entry->next != NULL && !cmdline->cmd_modified)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
		if (cmdline->hist_curr_entry->next == cmdline->hist_curr_entry)
		{
			vui_debugf("own next!\n");
		}
#endif

		cmdline->hist_curr_entry = cmdline->hist_curr_entry->next;

		memcpy(&vui_cmd[cmd_base], cmdline->hist_curr_entry->line, cmdline->hist_curr_entry->len);
		vui_crsrx = cmd_base + cmdline->hist_curr_entry->len - 1;
		cmd_len = vui_crsrx - 1;
		cmd_len = vui_crsrx - 1;
		memset(&vui_cmd[vui_crsrx], ' ', cols - vui_crsrx);
	}
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
	else
	{
		vui_debugf("no next\n");
	}
#endif

	return NULL;
}

static vui_state* tfunc_cmd_home(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline_def* cmdline = data;

	if (act <= 0) return NULL;

	vui_bar = vui_cmd;

	vui_crsrx = cmd_base;

	return NULL;
}

static vui_state* tfunc_cmd_end(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline_def* cmdline = data;

	if (act <= 0) return NULL;

	vui_bar = vui_cmd;

	vui_crsrx = cmd_len + 1;

	return NULL;
}

static vui_state* tfunc_cmd_escape(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline_def* cmdline = data;

	if (act <= 0) return NULL;

	vui_bar = vui_cmd;

	if (cmdline->hist_curr_entry != cmdline->hist_last_entry && !cmdline->cmd_modified)
	{
		hist_entry_kill(cmdline->hist_curr_entry);
		cmdline->hist_curr_entry = cmdline->hist_last_entry;
	}

	hist_entry_set(cmdline->hist_curr_entry, &vui_cmd[cmd_base], cmd_len);

	vui_bar = vui_status;
	vui_crsrx = -1;

	hist_entry_shrink(cmdline->hist_curr_entry);

	if (cmdline->hist_curr_entry->line[0] != 0)
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

	return NULL;
}

static vui_state* tfunc_cmd_enter(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline_def* cmdline = data;

	if (act <= 0) return NULL;

	if (cmdline->hist_curr_entry != cmdline->hist_last_entry && !cmdline->cmd_modified)
	{
		hist_entry_kill(cmdline->hist_curr_entry);
		cmdline->hist_curr_entry = cmdline->hist_last_entry;
	}

	hist_entry_set(cmdline->hist_curr_entry, &vui_cmd[cmd_base], cmd_len);

	cmdline->on_submit(vui_translator_run(cmdline->tr, cmdline->hist_curr_entry->line));

	vui_bar = vui_status;
	vui_crsrx = -1;

	hist_entry_shrink(cmdline->hist_curr_entry);

	if (cmdline->hist_curr_entry->line[0] != 0)
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

	return NULL;
}

// init/resize
void vui_init(int width)
{
	vui_normal_mode = vui_curr_state = vui_state_new();
	vui_string_reset(&vui_normal_mode->name);
	vui_string_puts(&vui_normal_mode->name, "vui_normal_mode");

	vui_normal_mode->gc.root++;

	vui_transition transition_normal = vui_transition_new3(vui_normal_mode, tfunc_normal, NULL);

	for (unsigned int i=0; i<VUI_MAXSTATE; i++)
	{
		vui_set_char_t(vui_normal_mode, i, transition_normal);
	}

	vui_state_stack = vui_stack_new();
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

static vui_state* tfunc_count_enter(vui_state* currstate, unsigned int c, int act, void* data)
{
	int* count = data;

	if (act <= 0) return NULL;

	vui_showcmd_put(c);

	*count = c - '0';

	return NULL;
}

static vui_state* tfunc_count(vui_state* currstate, unsigned int c, int act, void* data)
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

	vui_transition transition_count = vui_transition_new3(vui_count_mode, tfunc_count, &vui_count);
	vui_set_range_t(vui_count_mode, '0', '9', transition_count);

	vui_transition transition_count_enter = vui_transition_new3(vui_count_mode, tfunc_count_enter, &vui_count);
	vui_set_range_t(vui_normal_mode, '1', '9', transition_count_enter);
}

// macros

vui_state* state_macro_record;

static vui_state* tfunc_macro_record(vui_state* currstate, unsigned int c, int act, void* data)
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

static vui_state* tfunc_macro_execute(vui_state* currstate, unsigned int c, int act, void* data)
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

static vui_state* tfunc_record_enter(vui_state* currstate, unsigned int c, int act, void* data)
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
	vui_transition transition_macro_record = vui_transition_new3(vui_normal_mode, tfunc_macro_record, NULL);
	state_macro_record = vui_state_new_t(transition_macro_record);

	char sr[16];
	vui_utf8_encode(record, sr);
	vui_string_reset(&state_macro_record->name);
	vui_string_puts(&state_macro_record->name, sr);

	vui_transition transition_record_enter = vui_transition_new2(tfunc_record_enter, NULL);

	vui_set_char_t_u(vui_normal_mode, record, transition_record_enter);

	vui_transition transition_macro_execute = vui_transition_new3(vui_normal_mode, tfunc_macro_execute, NULL);

	vui_state* state_macro_execute = vui_state_new_t(transition_macro_execute);

	char se[16];
	vui_utf8_encode(execute, se);
	vui_string_reset(&state_macro_execute->name);
	vui_string_puts(&state_macro_execute->name, se);

	vui_set_char_t_u(vui_normal_mode, execute, vui_transition_new3(state_macro_execute, tfunc_showcmd_put, NULL));
}


// registers

void vui_register_init(void)
{
	vui_register_container = vui_state_new_s(NULL);
	vui_register_recording = NULL;
}

vui_string* vui_register_get(unsigned int c)
{
	vui_state* st = vui_next_u(vui_register_container, c, VUI_ACT_MAP);
	vui_string* reg = (vui_string*)st->data;

	if (reg == NULL)
	{
		reg = vui_string_new(NULL);

		vui_state* st_new = vui_state_new_s(NULL);
		st_new->data = reg;

		vui_set_char_s_u(vui_register_container, c, st_new);
	}

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
	vui_debugf("register %c: %s\n", c, reg->s);
#endif

	return reg;
}

void vui_register_record(unsigned int c)
{
	vui_string* reg = vui_register_get(c);
	vui_string_reset(reg);

	vui_register_recording = reg;

	vui_status_set("recording");
}

void vui_register_endrecord(void)
{
	if (vui_register_recording != NULL)
	{
		vui_register_recording->n--;
		vui_string_get(vui_register_recording);	// append null terminator

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
		vui_debugf("finished recording macro: %s\n", vui_register_recording->s);
#endif

		vui_register_recording = NULL;
	}

	vui_status_clear();
}

vui_state* vui_register_execute(vui_state* currstate, unsigned int c, int act)
{
	vui_state* st = vui_next_u(vui_register_container, c, VUI_ACT_MAP);
	void* data = st->data;

	if (data == NULL)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
		vui_debugf("stopped recursive macro %c\n", c);
#endif
	}

	vui_string* reg = vui_register_get(c);

	st->data = NULL;

	vui_state* nextstate = vui_run_s(currstate, reg->s, act);

	st->data = data;

	return nextstate;
}



void vui_bind_u(vui_state* mode, unsigned int c, vui_transition_callback func, void* data)
{
	vui_transition t = vui_transition_new2(func, data);

	vui_set_char_t_u(mode, c, t);
}

void vui_bind(vui_state* mode, unsigned char* s, vui_transition_callback func, void* data)
{
	vui_transition t = vui_transition_new2(func, data);

	vui_set_string_t_mid(mode, s, vui_transition_return(), t);
}




static vui_state* tfunc_cmd_paste(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (act <= 0) return vui_return(act);

	vui_string* reg = vui_register_get(c);

	if (reg != NULL)
	{
		vui_state* cmdline_state = vui_return(VUI_ACT_TEST);

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_VUI)
		vui_debugf("paste %c\n", c);
#endif
		for (size_t i = 0; i < reg->n; i++)
		{
			tfunc_cmd_key(cmdline_state, reg->s[i], act, data);
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
		func_enter.func = tfunc_status_set;
		func_enter.data = label;
	}

	if (!(mode & VUI_MODE_NEW_MANUAL_IN))
	{
		if (func_in.next == NULL) func_in.next = state;
	}

	if (func_exit.next == NULL) func_exit.next = vui_normal_mode;

	if (func_exit.func == NULL)
	{
		func_exit.func = tfunc_status_clear;
	}

	for (unsigned int i=0; i<VUI_MAXSTATE; i++)
	{
		vui_set_char_t(state, i, func_in);
	}

	vui_set_string_t_nocall(vui_normal_mode, cmd, func_enter);
	vui_set_char_t_u(state, VUI_KEY_ESCAPE, func_exit);

	state->push = vui_state_stack;

	return state;
}

vui_cmdline_def* vui_cmdline_mode_new(char* cmd, char* name, char* label, vui_translator* tr, vui_cmdline_submit_callback on_submit)
{
	vui_cmdline_def* cmdline = malloc(sizeof(vui_cmdline_def));
	cmdline->on_submit = on_submit;
	cmdline->label = label;
	cmdline->hist_last_entry = NULL;

	if (tr != NULL)
	{
		cmdline->tr = tr;
	}
	else
	{
		cmdline->tr = vui_translator_identity;
	}

	vui_state* cmdline_state = vui_state_new();
	vui_string_reset(&cmdline_state->name);
	vui_string_puts(&cmdline_state->name, name);

	cmdline_state->push = vui_state_stack;

	vui_transition transition_normal_to_cmd = vui_transition_new3(cmdline_state, tfunc_normal_to_cmd, cmdline);

	vui_set_string_t_nocall(vui_normal_mode, cmd, transition_normal_to_cmd);

	vui_transition transition_cmd_key = vui_transition_new3(cmdline_state, tfunc_cmd_key, cmdline);
	vui_transition transition_cmd_escape = vui_transition_new3(vui_normal_mode, tfunc_cmd_escape, cmdline);
	vui_transition transition_cmd_enter = vui_transition_new3(vui_normal_mode, tfunc_cmd_enter, cmdline);
	vui_transition transition_cmd_backspace = vui_transition_new2(tfunc_cmd_backspace, cmdline);

	vui_transition transition_cmd_up = vui_transition_new3(cmdline_state, tfunc_cmd_up, cmdline);
	vui_transition transition_cmd_down = vui_transition_new3(cmdline_state, tfunc_cmd_down, cmdline);
	vui_transition transition_cmd_left = vui_transition_new3(cmdline_state, tfunc_cmd_left, cmdline);
	vui_transition transition_cmd_right = vui_transition_new3(cmdline_state, tfunc_cmd_right, cmdline);
	vui_transition transition_cmd_delete = vui_transition_new3(cmdline_state, tfunc_cmd_delete, cmdline);
	vui_transition transition_cmd_home = vui_transition_new3(cmdline_state, tfunc_cmd_home, cmdline);
	vui_transition transition_cmd_end = vui_transition_new3(cmdline_state, tfunc_cmd_end, cmdline);

	vui_transition transition_macro_paste = vui_transition_new3(cmdline_state, tfunc_cmd_paste, cmdline);
	vui_state* paste_state = vui_state_new_t(transition_macro_paste);

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

	vui_set_char_s_u(cmdline_state, 'R' + VUI_KEY_MODIFIER_CONTROL, paste_state);

#if 0
	vui_state* keyescapestate = vui_state_new_t(transition_cmd_key);

	vui_set_char_s_u(cmdline_state, 22, keyescapestate);
#endif

	cmdline->cmdline_state = cmdline_state;

	hist_entry_new(cmdline);

	return cmdline;
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

	vui_run_c_p_u(&vui_curr_state, c, VUI_ACT_RUN);
}

void vui_reset(void)
{
	vui_showcmd_reset();
	vui_count = 0;
}
