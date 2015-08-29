#include <stdlib.h>
#include <string.h>

#include "utf8.h"

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
	if (!act) return NULL;

	if (act == 1)
	{
		vui_reset();
	}
	else if (act == 2)
	{
		vui_showcmd_put(c);
	}

	return NULL;
}

static vui_state* tfunc_showcmd_put(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (!act) return NULL;

	vui_showcmd_put(c);

	return NULL;
}

static vui_state* tfunc_status_set(vui_state* currstate, unsigned int c, int act, void* data)
{
	char* s = data;

	if (!act) return NULL;

	vui_status_set(s);

	return NULL;
}

static vui_state* tfunc_status_clear(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (!act) return NULL;

	vui_status_clear();

	return NULL;
}


// cmdline callbacks
static vui_state* tfunc_normal_to_cmd(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline_def* cmdline = data;

	if (!act) return cmdline->cmdline_state;

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

	if (!act) return NULL;

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

	if (!act) return (vui_crsrx <= cmd_base) ? NULL : vui_normal_mode;

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

	if (!act) return NULL;

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

	if (!act) return NULL;

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

	if (!act) return NULL;

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

	if (!act) return NULL;

	vui_bar = vui_cmd;

	if (cmdline->hist_curr_entry->prev != NULL && !cmdline->cmd_modified)
	{
#ifdef VUI_DEBUG
		if (cmdline->hist_curr_entry->prev == cmdline->hist_curr_entry)
		{
			vui_debug("own prev!\n");
		}
#endif

		cmdline->hist_curr_entry = cmdline->hist_curr_entry->prev;

		memcpy(&vui_cmd[cmd_base], cmdline->hist_curr_entry->line, cmdline->hist_curr_entry->len);
		vui_crsrx = cmd_base + cmdline->hist_curr_entry->len - 1;
		cmd_len = vui_crsrx - 1;
		memset(&vui_cmd[vui_crsrx], ' ', cols - vui_crsrx);
	}
#ifdef VUI_DEBUG
	else
	{
		vui_debug("no prev\n");
	}
#endif

	return NULL;
}

static vui_state* tfunc_cmd_down(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline_def* cmdline = data;

	if (!act) return NULL;

	vui_bar = vui_cmd;

	if (cmdline->hist_curr_entry->next != NULL && !cmdline->cmd_modified)
	{
#ifdef VUI_DEBUG
		if (cmdline->hist_curr_entry->next == cmdline->hist_curr_entry)
		{
			vui_debug("own next!\n");
		}
#endif

		cmdline->hist_curr_entry = cmdline->hist_curr_entry->next;

		memcpy(&vui_cmd[cmd_base], cmdline->hist_curr_entry->line, cmdline->hist_curr_entry->len);
		vui_crsrx = cmd_base + cmdline->hist_curr_entry->len - 1;
		cmd_len = vui_crsrx - 1;
		cmd_len = vui_crsrx - 1;
		memset(&vui_cmd[vui_crsrx], ' ', cols - vui_crsrx);
	}
#ifdef VUI_DEBUG
	else
	{
		vui_debug("no next\n");
	}
#endif

	return NULL;
}

static vui_state* tfunc_cmd_home(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline_def* cmdline = data;

	if (!act) return NULL;

	vui_bar = vui_cmd;

	vui_crsrx = cmd_base;

	return NULL;
}

static vui_state* tfunc_cmd_end(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline_def* cmdline = data;

	if (!act) return NULL;

	vui_bar = vui_cmd;

	vui_crsrx = cmd_len + 1;

	return NULL;
}

static vui_state* tfunc_cmd_escape(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline_def* cmdline = data;

	if (!act) return NULL;

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
#ifdef VUI_DEBUG
		vui_debug("new entry\n");
#endif

		hist_entry_new(cmdline);
	}
#ifdef VUI_DEBUG
	else
	{
		vui_debug("no new entry\n");
	}
#endif

	return NULL;
}

static vui_state* tfunc_cmd_enter(vui_state* currstate, unsigned int c, int act, void* data)
{
	vui_cmdline_def* cmdline = data;

	if (!act) return NULL;

	if (cmdline->hist_curr_entry != cmdline->hist_last_entry && !cmdline->cmd_modified)
	{
		hist_entry_kill(cmdline->hist_curr_entry);
		cmdline->hist_curr_entry = cmdline->hist_last_entry;
	}

	hist_entry_set(cmdline->hist_curr_entry, &vui_cmd[cmd_base], cmd_len);

	cmdline->on_submit(cmdline->hist_curr_entry->line);

	vui_bar = vui_status;
	vui_crsrx = -1;

	hist_entry_shrink(cmdline->hist_curr_entry);

	if (cmdline->hist_curr_entry->line[0] != 0)
	{
#ifdef VUI_DEBUG
		vui_debug("new entry\n");
#endif

		hist_entry_new(cmdline);
	}
#ifdef VUI_DEBUG
	else
	{
		vui_debug("no new entry\n");
	}
#endif

	return NULL;
}

// init/resize
void vui_init(int width)
{
	vui_normal_mode = vui_curr_state = vui_state_new(NULL);

	vui_transition transition_normal = {.next = vui_normal_mode, .func = tfunc_normal};

	for (int i=0; i<VUI_MAXSTATE; i++)
	{
		vui_set_char_t(vui_normal_mode, i, transition_normal);
	}

	vui_state_stack = vui_stack_new(vui_normal_mode);

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

	if (!act) return NULL;

	vui_showcmd_put(c);

	*count = c - '0';

	return NULL;
}

static vui_state* tfunc_count(vui_state* currstate, unsigned int c, int act, void* data)
{
	int* count = data;

	if (!act) return NULL;

	vui_showcmd_put(c);

	*count = *count*10 + c - '0';

	return NULL;
}

void vui_count_init(void)
{
	vui_transition transition_count_leave = vui_transition_run_c_s(vui_normal_mode);

	vui_count_mode = vui_state_new_t(transition_count_leave);

	vui_transition transition_count = {.next = vui_count_mode, .func = tfunc_count, .data = &vui_count};
	vui_set_range_t(vui_count_mode, '0', '9', transition_count);

	vui_transition transition_count_enter = {.next = vui_count_mode, .func = tfunc_count_enter, .data = &vui_count};
	vui_set_range_t(vui_normal_mode, '1', '9', transition_count_enter);
}

// macros

vui_state* state_macro_record;

static vui_state* tfunc_macro_record(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (!act) return vui_return(0);

#ifdef VUI_DEBUG
	char s[256];
	snprintf(s, 256, "record %c\r\n", c);
	vui_debug(s);
#endif

	vui_reset();

	vui_register_record(c);

	return vui_return(1);
}

static vui_state* tfunc_macro_execute(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (!act) return vui_return(0);

#ifdef VUI_DEBUG
	char s[256];
	snprintf(s, 256, "execute %c\r\n", c);
	vui_debug(s);
#endif

	int count = vui_count;
	vui_reset();

	if (count < 1) count = 1;

	while (count--)
	{
		vui_register_execute(c);
	}

	return vui_return(1);
}

static vui_state* tfunc_record_enter(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (act > 0)
	{
		vui_showcmd_put(c);
	}

	if (vui_register_recording == NULL) // begin recording
	{
		return state_macro_record;
	}
	else // end recording
	{
		if (act > 0)
		{
#ifdef VUI_DEBUG
			vui_debug("end record\r\n");
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

	vui_transition transition_record_enter = vui_transition_new2(tfunc_record_enter, NULL);

	vui_set_char_t(vui_normal_mode, record, transition_record_enter);

	vui_transition transition_macro_execute = vui_transition_new3(vui_normal_mode, tfunc_macro_execute, NULL);

	vui_state* state_macro_execute = vui_state_new_t(transition_macro_execute);

	vui_set_char_t(vui_normal_mode, execute, vui_transition_new3(state_macro_execute, tfunc_showcmd_put, NULL));
}


// registers

void vui_register_init(void)
{
	vui_register_container = vui_state_new(NULL);
	vui_register_recording = NULL;
}

vui_string* vui_register_get(int c)
{
	vui_string** regptr = (vui_string**)&vui_next_u(vui_register_container, c, -1)->data;

	if (*regptr == NULL)
	{
		*regptr = vui_string_new(NULL);
	}

	return *regptr;
}

void vui_register_record(int c)
{
	vui_string* reg = vui_register_get(c);
	reg->n = 0;
	reg->s[0] = 0;

	vui_register_recording = reg;
}

void vui_register_endrecord(void)
{
	vui_register_recording->s[--vui_register_recording->n] = 0;
#ifdef VUI_DEBUG
	char s[64];
	snprintf(s, 64, "finished recording macro: %s\r\n", vui_register_recording->s);
	vui_debug(s);
#endif
	vui_register_recording = NULL;
}

void vui_register_execute(int c)
{
	vui_string* reg = vui_register_get(c);
	vui_run_s(vui_return(0), reg->s, 1);
}


// new modes

vui_state* vui_mode_new(char* cmd, char* name, char* label, int mode, vui_transition func_enter, vui_transition func_in, vui_transition func_exit)
{
	vui_state* state = vui_state_new(NULL);

	if (func_enter.next == NULL) func_enter.next = state;

	if (func_enter.func == NULL)
	{
		func_enter.func = tfunc_status_set;
		func_enter.data = label;
	}

	if (mode != VUI_NEW_MODE_IN_MANUAL)
	{
		if (func_in.next == NULL) func_in.next = state;
	}

	if (func_exit.next == NULL) func_exit.next = vui_normal_mode;

	if (func_exit.func == NULL)
	{
		func_exit.func = tfunc_status_clear;
	}

	for (int i=0; i<VUI_MAXSTATE; i++)
	{
		vui_set_char_t(state, i, func_in);
	}

	vui_set_string_t(vui_normal_mode, cmd, func_enter);
	vui_set_char_t(state, VUI_KEY_ESCAPE, func_exit);

	state->push = vui_state_stack;

	return state;
}

vui_cmdline_def* vui_cmdline_mode_new(char* cmd, char* name, char* label, vui_cmdline_submit_callback on_submit)
{
	vui_cmdline_def* cmdline = malloc(sizeof(vui_cmdline_def));
	cmdline->on_submit = on_submit;
	cmdline->label = label;

	vui_transition transition_normal_to_cmd = {.next = NULL, .func = tfunc_normal_to_cmd, .data = cmdline};
	vui_transition transition_cmd = {.next = NULL, .func = NULL, .data = cmdline};
	vui_transition transition_cmd_escape = {.next = vui_normal_mode, .func = tfunc_cmd_escape, .data = cmdline};

	vui_state* cmdline_state = vui_mode_new(cmd, name, "", 0, transition_normal_to_cmd, transition_cmd, transition_cmd_escape);

	vui_transition transition_cmd_key = {.next = cmdline_state, .func = tfunc_cmd_key, .data = cmdline};
	vui_transition transition_cmd_up = {.next = cmdline_state, .func = tfunc_cmd_up, .data = cmdline};
	vui_transition transition_cmd_down = {.next = cmdline_state, .func = tfunc_cmd_down, .data = cmdline};
	vui_transition transition_cmd_left = {.next = cmdline_state, .func = tfunc_cmd_left, .data = cmdline};
	vui_transition transition_cmd_right = {.next = cmdline_state, .func = tfunc_cmd_right, .data = cmdline};
	vui_transition transition_cmd_enter = {.next = vui_normal_mode, .func = tfunc_cmd_enter, .data = cmdline};
	vui_transition transition_cmd_backspace = {.next = NULL, .func = tfunc_cmd_backspace, .data = cmdline};
	vui_transition transition_cmd_delete = {.next = cmdline_state, .func = tfunc_cmd_delete, .data = cmdline};
	vui_transition transition_cmd_home = {.next = cmdline_state, .func = tfunc_cmd_home, .data = cmdline};
	vui_transition transition_cmd_end = {.next = cmdline_state, .func = tfunc_cmd_end, .data = cmdline};

	vui_set_range_t(cmdline_state, 32, 126, transition_cmd_key);
	vui_set_char_t(cmdline_state, VUI_KEY_UP, transition_cmd_up);
	vui_set_char_t(cmdline_state, VUI_KEY_DOWN, transition_cmd_down);
	vui_set_char_t(cmdline_state, VUI_KEY_LEFT, transition_cmd_left);
	vui_set_char_t(cmdline_state, VUI_KEY_RIGHT, transition_cmd_right);
	vui_set_char_t(cmdline_state, VUI_KEY_ENTER, transition_cmd_enter);
	vui_set_char_t(cmdline_state, VUI_KEY_BACKSPACE, transition_cmd_backspace);
	vui_set_char_t(cmdline_state, VUI_KEY_DELETE, transition_cmd_delete);
	//vui_set_char_t(cmdline_state, VUI_KEY_ESCAPE, transition_cmd_escape);
	vui_set_char_t(cmdline_state, VUI_KEY_HOME, transition_cmd_home);
	vui_set_char_t(cmdline_state, VUI_KEY_END, transition_cmd_end);


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
#ifdef VUI_DEBUG
		char s[64];
		snprintf(s, 64, "register_putc(%d)\r\n", c);
		vui_debug(s);
#endif
		vui_string_put(vui_register_recording, c);
	}

	vui_run_c_p_u(&vui_curr_state, c, 1);
}

void vui_reset(void)
{
	vui_showcmd_reset();
	vui_count = 0;
}
