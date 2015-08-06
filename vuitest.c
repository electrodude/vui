#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <curses.h>

#include "vui.h"


WINDOW* statusline;

int VUI_KEY_UP = KEY_UP;
int VUI_KEY_DOWN = KEY_DOWN;
int VUI_KEY_LEFT = KEY_LEFT;
int VUI_KEY_RIGHT = KEY_RIGHT;
int VUI_KEY_ENTER = '\n';
int VUI_KEY_BACKSPACE = 127;
int VUI_KEY_DELETE = KEY_DC;
int VUI_KEY_ESCAPE = 27;
int VUI_KEY_HOME = KEY_HOME;
int VUI_KEY_END = KEY_END;

int dbgfd;

vui_cmdline_def* cmd_mode;
vui_cmdline_def* search_mode;

void wrlog(char* s)
{
	write(dbgfd, s, strlen(s));
}

#ifdef VUI_DEBUG
void vui_debug(char* s)
{
	wrlog(s);
}
#endif

static vui_state* transition_quit(vui_state* prevstate, int c, int act, void* data)
{
	if (act)
	{
		wrlog("quit\r\n");
		endwin();
		printf("Quitting!\n");
		exit(0);
	}

	return prevstate;
}

static vui_state* transition_winch(vui_state* prevstate, int c, int act, void* data)
{
	int width;
	int height;
	getmaxyx(stdscr, height, width);

	wresize(statusline, 1, width);

	mvwin(statusline, height-1, 0);

	vui_resize(width);
}

void on_cmd_submit(char* cmd)
{
	wrlog("command: \"");
	wrlog(cmd);
	wrlog("\"\r\n");

	if (!strcmp(cmd, "q"))
	{
		endwin();
		printf("Quitting!\n");
		exit(0);
	}
}

void on_search_submit(char* cmd)
{
	wrlog("search: \"");
	wrlog(cmd);
	wrlog("\"\r\n");
}


int main(int argc, char** argv)
{
	printf("Opening log...\n");
	dbgfd = open("log", O_WRONLY | O_APPEND);

	wrlog("start\r\n");

	initscr();
	cbreak();
	noecho();

	int width, height;
	getmaxyx(stdscr, height, width);

	statusline = newwin(1, width, height-1, 0);

	keypad(statusline, TRUE);
	meta(statusline, TRUE);


	vui_init(width);

	cmd_mode = vui_cmdline_mode_new(":", "command", ":", on_cmd_submit);

	search_mode = vui_cmdline_mode_new("/", "search", "/", on_search_submit);


	vui_transition quit = vui_transition_new2(transition_quit, NULL);

	vui_set_char_t(vui_normal_mode, 'Q', quit);

	vui_set_string_t(vui_normal_mode, "ZZ", quit);

	vui_set_char_t(vui_normal_mode, KEY_RESIZE, vui_transition_new3(vui_normal_mode, transition_winch, NULL));


	while (1)
	{
		int c = wgetch(statusline);

		int c2 = c >= 32 && c <= 127 ? c : '?';

		char s[256];
		sprintf(s, "char %d: %c\r\n", c, c2);
		wrlog(s);

		if (c >= 0)
		{
			vui_input(c);
			mvwaddstr(statusline, 0, 0, vui_bar);
			wmove(statusline, 0, vui_crsrx);
			curs_set(vui_crsrx >= 0);
			wrefresh(statusline);

			if (vui_curr_state == vui_normal_mode)
			{
				wrlog("normal mode\r\n");
			}
			else if (vui_curr_state == cmd_mode->cmdline_state)
			{
				wrlog("cmd mode\r\n");
			}
			else if (vui_curr_state == search_mode->cmdline_state)
			{
				wrlog("search mode\r\n");
			}
			else
			{
				wrlog("unknown mode\r\n");
			}
		}
	}

	endwin();
}
