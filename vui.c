#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vui.h"


char* vui_bar;
int vui_crsrx;
int cols;

char* vui_cmd;
char* vui_status;

vui_state* vui_normal_mode;

typedef struct hist_entry
{
	struct hist_entry* prev;
	struct hist_entry* next;

	char* line;
	size_t len;
	size_t maxlen;
} hist_entry;

hist_entry* hist_curr_entry;
hist_entry* hist_last_entry;

int cmd_base;
int cmd_len;
int cmd_modified;

static hist_entry* hist_entry_new(void)
{
	hist_entry* entry = malloc(sizeof(hist_entry));

	entry->prev = hist_last_entry;

	if (hist_last_entry != NULL)
	{
		hist_last_entry->next = entry;
	}

	hist_curr_entry = hist_last_entry = entry;

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

static void hist_last_entry_edit(void)
{
	cmd_modified = 1;

	if (hist_curr_entry == hist_last_entry)
	{
		return;
	}

	hist_entry_set(hist_last_entry, hist_curr_entry->line, hist_curr_entry->len);

	hist_curr_entry = hist_last_entry;
}

static void hist_last_entry_clear(void)
{
	cmd_modified = 0;

	hist_last_entry->len = 1;
	hist_last_entry->line[0] = 0;
}


static vui_state* tfunc_normal(vui_state* prevstate, int c, int act, void* data)
{
	if (!act) return NULL;

	return NULL;
}

static vui_state* tfunc_normal_to_cmd(vui_state* prevstate, int c, int act, void* data)
{
	if (!act) return NULL;

	vui_bar = vui_cmd;
	vui_crsrx = 0;

	char* data2 = data;
	while (*data2)
	{
		vui_cmd[vui_crsrx++] = *data2++;
	}

	cmd_base = vui_crsrx;

	cmd_len = cmd_base - 1;

	cmd_modified = 0;

	hist_curr_entry = hist_last_entry;

	memset(&vui_cmd[cmd_base], ' ', cols - cmd_base);

	return NULL;
}

static vui_state* tfunc_cmd(vui_state* prevstate, int c, int act, void* data)
{
	if (!act) return (c == VUI_KEY_BACKSPACE && vui_crsrx <= cmd_base) ? NULL : vui_normal_mode;

	if (c == VUI_KEY_BACKSPACE)
	{
		if (cmd_len <= 0)
		{
			vui_bar = vui_status;
			vui_crsrx = -1;
			return vui_normal_mode;
		}

		if (vui_crsrx > cmd_base)
		{
			hist_last_entry_edit();

			vui_cmd[cmd_len+1] = ' ';
			vui_crsrx--;
			memmove(&vui_cmd[vui_crsrx], &vui_cmd[vui_crsrx+1], cmd_len-vui_crsrx+1);
			cmd_len--;

			if (!cmd_len)
			{
				hist_last_entry_clear();
			}
		}
	}
	else if (c == VUI_KEY_DELETE)
	{
		if (vui_crsrx <= cmd_len)
		{
			hist_last_entry_edit();

			vui_cmd[cmd_len+1] = ' ';
			memmove(&vui_cmd[vui_crsrx], &vui_cmd[vui_crsrx+1], cmd_len-vui_crsrx+1);
			cmd_len--;

			if (!cmd_len)
			{
				hist_last_entry_clear();
			}
		}
	}
	else if (c == VUI_KEY_LEFT)
	{
		if (vui_crsrx > cmd_base)
		{
			vui_crsrx--;
		}
	}
	else if (c == VUI_KEY_RIGHT)
	{
		if (vui_crsrx <= cmd_len)
		{
			vui_crsrx++;
		}
	}
	else if (c == VUI_KEY_UP)
	{
		if (hist_curr_entry->prev != NULL && !cmd_modified)
		{
			hist_curr_entry = hist_curr_entry->prev;

			memcpy(&vui_cmd[cmd_base], hist_curr_entry->line, hist_curr_entry->len);
			vui_crsrx = cmd_base + hist_curr_entry->len - 1;
			cmd_len = vui_crsrx - 1;
			memset(&vui_cmd[vui_crsrx], ' ', cols - vui_crsrx);
		}
	}
	else if (c == VUI_KEY_DOWN)
	{
		if (hist_curr_entry->next != NULL && !cmd_modified)
		{
			hist_curr_entry = hist_curr_entry->next;

			memcpy(&vui_cmd[cmd_base], hist_curr_entry->line, hist_curr_entry->len);
			vui_crsrx = cmd_base + hist_curr_entry->len - 1;
			cmd_len = vui_crsrx - 1;
			cmd_len = vui_crsrx - 1;
			memset(&vui_cmd[vui_crsrx], ' ', cols - vui_crsrx);
		}
	}
	else if (c == VUI_KEY_HOME)
	{
		vui_crsrx = cmd_base;
	}
	else if (c == VUI_KEY_END)
	{
		vui_crsrx = cmd_len + 1;
	}
	else if (c >= 32 && c < 127)
	{
		hist_last_entry_edit();

		if (vui_crsrx != cmd_len)
		{
			memmove(&vui_cmd[vui_crsrx + 1], &vui_cmd[vui_crsrx], cmd_len - vui_crsrx + 1);
		}
		vui_cmd[vui_crsrx++] = c;
		cmd_len++;
	}


	return NULL;
}

static vui_state* tfunc_cmd_to_normal(vui_state* prevstate, int c, int act, void* data)
{
	if (!act) return NULL;

	if (hist_curr_entry != hist_last_entry && !cmd_modified)
	{
		hist_entry_kill(hist_curr_entry);
		hist_curr_entry = hist_last_entry;
	}

	hist_entry_set(hist_curr_entry, &vui_cmd[cmd_base], cmd_len);

	vui_bar = vui_status;
	vui_crsrx = -1;

	hist_entry_shrink(hist_curr_entry);

	if (hist_curr_entry->line[0] != 0)
	{
		hist_entry_new();
	}

	return NULL;
}

static vui_state* tfunc_cmd_submit(vui_state* prevstate, int c, int act, void* data)
{
	if (!act) return NULL;

	if (hist_curr_entry != hist_last_entry && !cmd_modified)
	{
		hist_entry_kill(hist_curr_entry);
		hist_curr_entry = hist_last_entry;
	}

	hist_entry_set(hist_curr_entry, &vui_cmd[cmd_base], cmd_len);

	vui_cmd_submit_callback* on_submit = *((vui_cmd_submit_callback*)data);

	on_submit(hist_curr_entry->line);

	vui_bar = vui_status;
	vui_crsrx = -1;

	hist_entry_shrink(hist_curr_entry);

	if (hist_curr_entry->line[0] != 0)
	{
		hist_entry_new();
	}

	return NULL;
}


void vui_init(int width)
{
	vui_normal_mode = vui_curr_state = vui_state_new(NULL);

	vui_transition transition_normal = {.next = vui_normal_mode, .func = tfunc_normal};

	for (int i=0; i<MAXINPUT; i++)
	{
		vui_normal_mode->next[i] = transition_normal;
	}

	cols = width;

	vui_cmd = malloc((cols+1)*sizeof(char));
	vui_status = malloc((cols+1)*sizeof(char));

	memset(vui_cmd, ' ', cols);
	memset(vui_status, ' ', cols);

	vui_cmd[cols] = 0;
	vui_status[cols] = 0;

	vui_bar = vui_status;

	hist_entry_new();
}

void vui_resize(int width)
{
	// if the number of columns changed
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

vui_state* vui_mode_new(char* cmd, char* name, char* label, int mode, vui_transition func_enter, vui_transition func_in, vui_transition func_exit)
{
	vui_state* this = vui_state_new(NULL);

	if (func_enter.next == NULL) func_enter.next = this;
	if (mode != VUI_NEW_MODE_IN_MANUAL)
	{
		if (func_in.next == NULL) func_in.next = this;
	}
	if (func_exit.next == NULL) func_exit.next = vui_normal_mode;

	for (int i=0; i<MAXINPUT; i++)
	{
		this->next[i] = func_in;
	}

	vui_set_string_t(vui_normal_mode, cmd, func_enter);
	vui_set_char_t(this, VUI_KEY_ESCAPE, func_exit);

	return this;
}

vui_state* vui_cmd_mode_new(char* cmd, char* name, char* label, void on_submit(char* cmd))
{
	vui_cmd_submit_callback* on_submit_malloc = malloc(sizeof(vui_cmd_submit_callback));
	on_submit_malloc = on_submit;

	vui_transition transition_normal_to_cmd = {.next = NULL, .func = tfunc_normal_to_cmd, .data = label};
	vui_transition transition_cmd = {.next = NULL, .func = tfunc_cmd};
	vui_transition transition_cmd_to_normal = {.next = vui_normal_mode, .func = tfunc_cmd_to_normal};
	vui_transition transition_cmd_submit = {.next = vui_normal_mode, .func = tfunc_cmd_submit, .data = on_submit_malloc};

	vui_state* vui_cmd_mode = vui_mode_new(cmd, name, "", VUI_NEW_MODE_IN_MANUAL, transition_normal_to_cmd, transition_cmd, transition_cmd_to_normal);

	vui_set_char_t(vui_cmd_mode, VUI_KEY_ENTER, transition_cmd_submit);

	return vui_cmd_mode;
}
