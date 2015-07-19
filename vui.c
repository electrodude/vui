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
vui_state* vui_cmd_mode;

int cmd_len;

static vui_state* tfunc_normal(vui_state* prevstate, int c, int act, void* data)
{
	if (!act) return NULL;

}

static vui_state* tfunc_normal_to_cmd(vui_state* prevstate, int c, int act, void* data)
{
	if (!act) return NULL;

	vui_bar = vui_cmd;
	vui_crsrx = 0;
	cmd_len = 0;
	vui_cmd[vui_crsrx++] = ':';

	memset(&vui_cmd[1], ' ', cols-1);
}

static vui_state* tfunc_cmd(vui_state* prevstate, int c, int act, void* data)
{
	if (!act) return (c == VUI_KEY_BACKSPACE && vui_crsrx <= 1) ? vui_cmd_mode : vui_normal_mode;

	if (c == VUI_KEY_BACKSPACE)
	{
		if (cmd_len <= 0)
		{
			vui_bar = vui_status;
			vui_crsrx = -1;
			return vui_normal_mode;
		}

		if (vui_crsrx > 1)
		{
			vui_cmd[cmd_len+1] = ' ';
			vui_crsrx--;
			memmove(&vui_cmd[vui_crsrx], &vui_cmd[vui_crsrx+1], cmd_len-vui_crsrx+1);
			cmd_len--;
		}
	}
	else if (c == VUI_KEY_DELETE)
	{
		if (vui_crsrx <= cmd_len)
		{
			vui_cmd[cmd_len+1] = ' ';
			memmove(&vui_cmd[vui_crsrx], &vui_cmd[vui_crsrx+1], cmd_len-vui_crsrx+1);
			cmd_len--;
		}
	}
	else if (c == VUI_KEY_LEFT)
	{
		if (vui_crsrx > 1)
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

	}
	else if (c == VUI_KEY_DOWN)
	{

	}
	else if (c >= 32 && c < 127)
	{
		vui_cmd[vui_crsrx++] = c;
		cmd_len++;
	}


	sprintf(&vui_cmd[cols-10], "%d, %d   ", vui_crsrx, cmd_len);

	return vui_cmd_mode;
}

static vui_state* tfunc_cmd_to_normal(vui_state* prevstate, int c, int act, void* data)
{
	if (!act) return NULL;

	vui_bar = vui_status;
	vui_crsrx = -1;
}

static vui_state* tfunc_cmd_submit(vui_state* prevstate, int c, int act, void* data)
{
	if (!act) return NULL;

	char* cmd = malloc(cmd_len);

	strncpy(cmd, &vui_cmd[1], cmd_len);

	vui_on_cmd_submit(cmd);

	vui_bar = vui_status;
	vui_crsrx = -1;
}


void vui_init(int width)
{
	vui_normal_mode = vui_curr_state = vui_state_new(NULL);

	vui_transition transition_normal = {.next = vui_normal_mode, .func = tfunc_normal};
	vui_transition transition_normal_to_cmd = {.next = vui_cmd_mode, .func = tfunc_normal_to_cmd};
	vui_transition transition_cmd = {.next = NULL, .func = tfunc_cmd};
	vui_transition transition_cmd_to_normal = {.next = vui_normal_mode, .func = tfunc_cmd_to_normal};
	vui_transition transition_cmd_submit = {.next = vui_normal_mode, .func = tfunc_cmd_submit};

	for (int i=0; i<MAXINPUT; i++)
	{
		vui_normal_mode->next[i] = transition_normal;
	}

	vui_cmd_mode = vui_mode_new(":", "command", "", VUI_NEW_MODE_IN_MANUAL, transition_normal_to_cmd, transition_cmd, transition_cmd_to_normal);

	vui_set_char_t(vui_cmd_mode, VUI_KEY_ENTER, transition_cmd_submit);

	cols = width;

	vui_cmd = malloc((cols+1)*sizeof(char));
	vui_status = malloc((cols+1)*sizeof(char));

	memset(vui_cmd, ' ', cols);
	memset(vui_status, ' ', cols);

	vui_cmd[cols] = 0;
	vui_status[cols] = 0;

	vui_bar = vui_status;
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
	vui_set_char_t(this, 27, func_exit);

	return this;
}
