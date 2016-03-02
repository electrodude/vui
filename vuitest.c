#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>

#include <curses.h>

#include "vui_debug.h"

#include "vui.h"
#include "vui_combinator.h"
#include "vui_translator.h"
#include "vui_gc.h"

#include "vui_graphviz.h"

#define LOGWIDTH 48


WINDOW* logwindow;
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
int VUI_KEY_MODIFIER_CONTROL = -'@';

FILE* dbgf;

vui_cmdline* cmd_mode;
vui_cmdline* search_mode;

// Defined even if !defined(VUI_DEBUG), since we use it for debug output
void vui_debugf(const char* format, ...)
{
	va_list argp;
	va_start(argp, format);

	size_t len = COLS;

	char line[len];

	vsnprintf(line, len, format, argp);
	va_end(argp);

	fprintf(dbgf, "%s", line);
	fflush(dbgf);

	waddstr(logwindow, line);
	wrefresh(logwindow);

}

static void on_winch(void)
{
	endwin();

	struct winsize w;
	ioctl(0, TIOCGWINSZ, &w);
	vui_debugf("TIOCGWINSZ: %dx%d\n", w.ws_col, w.ws_row);

	if (!is_term_resized(w.ws_row, w.ws_col))
	{
		return;
	}

	resize_term(w.ws_row, w.ws_col);

	refresh();
	clear();
	doupdate();

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_TEST)
	vui_debugf("winch %dx%d\n", COLS, LINES);
#endif


	wresize(logwindow, LINES-1, LOGWIDTH);
	mvwin(logwindow, 0, COLS - LOGWIDTH);
	wrefresh(logwindow);

	wresize(statusline, 1, COLS);

	mvwin(statusline, LINES-1, 0);

	vui_resize(COLS);

	vui_showcmd_setup(COLS - 20, 10);

	mvwaddstr(statusline, 0, 0, vui_bar);
	wmove(statusline, 0, vui_crsrx);
	curs_set(vui_crsrx >= 0);
	wrefresh(statusline);

	refresh();
}

static void quit(void)
{
	endwin();
	printf("Quitting!\n");

	if (cmd_mode != NULL) vui_cmdline_kill(cmd_mode);
	if (search_mode != NULL) vui_cmdline_kill(search_mode);

	vui_deinit();

	vui_gc_run();

	printf("live GC objects: %ld\n", vui_gc_nlive);

	exit(0);
}

static void sighandler(int signo)
{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_TEST)
	vui_debugf("signal %d\n", signo);
#endif

        switch (signo)
        {
                case SIGWINCH:
                {
			ungetch(KEY_RESIZE);

                        break;
                }

		case SIGINT:
		{
			quit();
		}

                default:
                {

                }
        }
}

static vui_state* tfunc_quit(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (act <= 0) return vui_return(act);

	if (vui_count != 0)
	{
		vui_debugf("count: %d\n", vui_count);

		vui_reset();

		return vui_return(act);
	}

	vui_debugf("quit\n");

	quit();
}

static vui_state* tfunc_gc(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (act <= 0) return vui_return(act);

	vui_reset();

	vui_debugf("pre  gc objects: %ld\n", vui_gc_nlive);

	vui_gc_run();

	vui_debugf("post gc objects: %ld\n", vui_gc_nlive);

	return vui_return(act);
}


static vui_state* tfunc_graphviz(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (act <= 0) return vui_return(act);

	vui_reset();

	vui_stack* gv_roots = vui_stack_new();
	vui_state_stack_push(gv_roots, vui_normal_mode);
	//vui_state_stack_push(gv_roots, cmd_tr_start);

	FILE* f = fopen("vui.dot", "w");
	vui_gv_write(f, gv_roots);
	fclose(f);

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_TEST)
	vui_debugf("wrote vui.dot\n");
#endif

	return vui_return(act);
}


static vui_state* tfunc_winch(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (act <= 0) return vui_return(act);

	on_winch();

	return vui_return(act);
}

void on_cmd_submit(vui_stack* cmd)
{
#define arg(i) vui_string_get(vui_stack_index(cmd, i))

	char* op = arg(0);

	if (op != NULL)
	{
		vui_debugf("op: \"%s\"\n", op);
	}
	else
	{
		vui_debugf("op: NULL");
		return;
	}

	if (!strcmp(op, "q"))
	{
		quit();
	}
	else if (!strcmp(op, "map"))
	{
		char* action = arg(1);
		if (action == NULL)
		{
			vui_debugf("action: NULL\n");
			return;
		}
		else
		{
			vui_debugf("action: \"%s\"\n", action);
		}
		char* reaction = arg(2);
		if (reaction == NULL)
		{
			vui_debugf("reaction: NULL\n");
			return;
		}
		else
		{
			vui_debugf("reaction: \"%s\"\n", reaction);
		}

		vui_map(vui_normal_mode, action, reaction);
	}
#undef arg
}

void on_search_submit(vui_stack* cmd)
{
	vui_debugf("search: \"%s\"\n", vui_string_get(vui_stack_peek(cmd)));
}


int main(int argc, char** argv)
{
	printf("Opening log...\n");
	dbgf = fopen("log", "a");

	setvbuf(dbgf, NULL, _IOLBF, BUFSIZ);

	vui_debugf("start\n");

	initscr();
	cbreak();
	noecho();

	endwin();

	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = sighandler;
	sigaction(SIGWINCH, &sa, NULL);


	logwindow = newwin(LINES-1, LOGWIDTH, 0, COLS - LOGWIDTH);
	scrollok(logwindow, true);

	statusline = newwin(1, COLS, LINES-1, 0);

	keypad(statusline, TRUE);
	meta(statusline, TRUE);


	vui_init(COLS);

	vui_bind_u(vui_normal_mode, KEY_RESIZE, vui_transition_new2(tfunc_winch, NULL));

	vui_count_init();

	vui_register_init();

	vui_macro_init('q', '@');

	vui_showcmd_setup(COLS - 20, 10);

	vui_translator_init();


	// make cmdline parser
	vui_translator* cmd_tr = vui_translator_new();

	vui_state* cmd_tr_start = vui_state_new_putc(cmd_tr);
	vui_string_new_str_at(&cmd_tr_start->name, "cmd_tr");


	// :q
	vui_state* cmd_tr_q = vui_state_new_deadend();

	vui_set_string_t_mid(cmd_tr_start, "q", vui_transition_translator_putc(cmd_tr, NULL), vui_transition_translator_putc(cmd_tr, cmd_tr_q));


	// :map a b
	vui_state* cmd_tr_map = vui_frag_release(
	                                         vui_frag_cat(vui_frag_accept_escaped(cmd_tr), vui_frag_accept_escaped(cmd_tr)),
						 vui_state_new_deadend());

	vui_set_string_t_mid(cmd_tr_start, "map ", vui_transition_translator_putc(cmd_tr, NULL), vui_transition_translator_push(cmd_tr, cmd_tr_map));


	vui_translator_new2(cmd_tr, cmd_tr_start);



	cmd_mode = vui_cmdline_new(":", "command", ":", cmd_tr, on_cmd_submit);

	search_mode = vui_cmdline_new("/", "search", "/", NULL, on_search_submit);


	vui_bind(vui_normal_mode, "Q", vui_transition_new2(tfunc_quit, NULL));

	vui_bind(vui_normal_mode, "ZZ", vui_transition_new2(tfunc_quit, NULL));


	vui_bind(vui_normal_mode, "gc", vui_transition_new2(tfunc_gc, NULL));

	vui_bind(vui_normal_mode, "gv", vui_transition_new2(tfunc_graphviz, NULL));


	// done initialization; run garbage collector
	vui_gc_run();

	/*
	vui_stack* gv_roots = vui_stack_new();
	//vui_state_stack_push(gv_roots, vui_normal_mode);
	//vui_state_stack_push(gv_roots, cmd_tr_start);

	FILE* f = fopen("vui.dot", "w");
	vui_gv_write(f, gv_roots);
	fclose(f);

	exit(0);
	*/

	vui_debugf("live GC objects: %ld\n", vui_gc_nlive);

	while (1)
	{
		int c = wgetch(statusline);

		char c2[3] = {"??"};
		if (c >= 32 && c < 127)
		{
			c2[0] = c;
			c2[1] = 0;
		}
		else if (c >= 0 && c < 32)
		{
			c2[0] = '^';
			c2[1] = c + '@';
		}
		else
		{
			c2[0] = '?';
			c2[1] = '?';
		}

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_TEST)
		vui_debugf("char %d: %s\n", c, c2);
#endif

		if (c >= 0)
		{
			endwin();

			vui_input(c);
			mvwaddstr(statusline, 0, 0, vui_bar);
			wmove(statusline, 0, vui_crsrx);
			curs_set(vui_crsrx >= 0);
			wrefresh(statusline);

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_TEST)
			vui_debugf("%s\n", vui_state_name(vui_curr_state));
#endif
		}
	}

	quit();
}
