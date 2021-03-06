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

#include "vui_stack_refcount.h"

#include "vui_gc.h"

#include "vui.h"
#include "vui_combinator.h"
#include "vui_translator.h"

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

	size_t len = COLS > 256 ? COLS : 256;

	char line[len];

	vsnprintf(line, len, format, argp);
	va_end(argp);

	fprintf(dbgf, "%s", line);
	fflush(dbgf);

	waddstr(logwindow, line);
	wrefresh(logwindow);

	curs_set(vui_crsrx >= 0);
	if (vui_crsrx >= 0)
	{
		wrefresh(statusline);
	}
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
	vui_debugf("Quitting!\n");

	if (cmd_mode != NULL) vui_cmdline_kill(cmd_mode);
	if (search_mode != NULL) vui_cmdline_kill(search_mode);

	vui_deinit();

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_TEST)
	vui_debugf("live GC objects: %zd\n", vui_gc_nlive);
#endif

	vui_debugf("Quitting!\n");

	fclose(dbgf);

	endwin();

	printf("Quitting!\n");

	exit(0);
}

static void sighandler(int signo)
{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_TEST)
	vui_debugf("signal %d\n", signo);
#endif

        switch (signo)
        {
		case SIGINT:
		case SIGQUIT:
		case SIGTERM:
		{
			endwin();
			puts("Terminating!");
			exit(1);
		}

                case SIGWINCH:
                {
			ungetch(KEY_RESIZE);

                        break;
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

	// never happens
	return vui_return(act);
}

static vui_state* tfunc_gc(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (act <= 0) return vui_return(act);

	vui_reset();

	vui_debugf("pre  gc objects: %zd\n", vui_gc_nlive);

	vui_gc_run();

	vui_debugf("post gc objects: %zd\n", vui_gc_nlive);

	return vui_return(act);
}


static vui_state* tfunc_graphviz(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (act <= 0) return vui_return(act);

	vui_reset();

	vui_stack* gv_roots = vui_stack_refcount_new();
	vui_stack_push(gv_roots, vui_normal_mode->state);
	//vui_stack_push(gv_roots, cmd_tr_start);

	FILE* f = fopen("vui.dot", "w");
	vui_gv_write(f, gv_roots);
	fclose(f);

	vui_stack_kill(gv_roots);

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_TEST)
	vui_debugf("wrote vui.dot\n");
#endif

	return vui_return(act);
}


static vui_state* tfunc_paste(vui_state* currstate, unsigned int c, int act, void* data)
{
	if (act <= 0) return vui_return(act);

	if (vui_count != 0)
	{
		vui_debugf("count: %d\n", vui_count);
	}

	vui_string str;
	vui_string_new_at(&str);
	vui_string_append_quote(&str, vui_register_curr);

	vui_debugf("paste \"%s\"\n", vui_string_get(&str));

	vui_string_dtor(&str);

	vui_reset();

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
#define arg_str(i) (vui_tr_obj_cast(vui_stack_index(cmd, i), VUI_TR_OBJ_STRING)->obj.str)
#define arg(i) vui_string_get(arg_str(i))

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
		vui_string* action = arg_str(1);
		if (action == NULL)
		{
			vui_debugf("action: NULL\n");
			return;
		}
		else
		{
			vui_debugf("action: \"%s\"\n", vui_string_get(action));
		}
		vui_string* reaction = arg_str(2);
		if (reaction == NULL)
		{
			vui_debugf("reaction: NULL\n");
			return;
		}
		else
		{
			vui_debugf("reaction: \"%s\"\n", vui_string_get(reaction));
		}

		vui_map(vui_normal_mode, action, reaction);
	}
#undef arg
}

void on_search_submit(vui_stack* cmd)
{
	vui_debugf("search: \"%s\"\n", vui_string_get(vui_tr_obj_cast(vui_stack_peek(cmd), VUI_TR_OBJ_STRING)->obj.str));
}


int main(int argc, char** argv)
{
	printf("Opening log...\n");
	dbgf = fopen("log", "a");

	setvbuf(dbgf, NULL, _IOLBF, BUFSIZ);

	initscr();
	cbreak();
	noecho();

	vui_debugf("start\n");

	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = sighandler;
	sigaction(SIGINT  , &sa, NULL);
	sigaction(SIGQUIT , &sa, NULL);
	sigaction(SIGTERM , &sa, NULL);
	sigaction(SIGWINCH, &sa, NULL);


	logwindow = newwin(LINES-1, LOGWIDTH, 0, COLS - LOGWIDTH);
	scrollok(logwindow, true);

	statusline = newwin(1, COLS, LINES-1, 0);

	keypad(statusline, TRUE);
	meta(statusline, TRUE);

	endwin();


	vui_init(COLS);

	vui_bind_u(vui_normal_mode, KEY_RESIZE, vui_transition_new2(tfunc_winch, NULL));

	vui_count_init();
	vui_register_init();

	vui_register_init();

	vui_macro_init("q", "@");

	vui_showcmd_setup(COLS - 20, 10);

	vui_tr_init();


	// make cmdline parser
	vui_tr* cmd_tr = vui_tr_new();

	vui_state* cmd_tr_start = vui_state_new_putc(cmd_tr);
	vui_string_sets(&cmd_tr_start->name, "cmd_tr");


	// :q
	vui_state* cmd_tr_q = vui_state_new_deadend();

	vui_set_string_t_mid(cmd_tr_start, "q", vui_transition_tr_append(cmd_tr, NULL), vui_transition_tr_append(cmd_tr, cmd_tr_q));


	// :map a b
	vui_state* cmd_tr_map = vui_frag_release(
	                                         vui_frag_cat(vui_frag_accept_escaped(cmd_tr), vui_frag_accept_escaped(cmd_tr)),
	                                         vui_state_new_deadend());

	vui_set_string_t_mid(cmd_tr_start, "map ", vui_transition_tr_append(cmd_tr, NULL), vui_transition_tr_new_string(cmd_tr, cmd_tr_map));


	vui_tr_replace(cmd_tr, cmd_tr_start);



	cmd_mode = vui_cmdline_new(":", "command", ":", cmd_tr, on_cmd_submit);

	search_mode = vui_cmdline_new("/", "search", "/", NULL, on_search_submit);


	vui_bind(vui_normal_mode, "Q", vui_transition_new2(tfunc_quit, NULL));

	vui_bind(vui_normal_mode, "ZZ", vui_transition_new2(tfunc_quit, NULL));


	vui_bind(vui_normal_mode, "gc", vui_transition_new2(tfunc_gc, NULL));

	vui_bind(vui_normal_mode, "gv", vui_transition_new2(tfunc_graphviz, NULL));

	vui_bind(vui_normal_mode, "p", vui_transition_new2(tfunc_paste, NULL));

	// done initialization; run garbage collector
	vui_gc_run();

#if 0
	vui_stack* gv_roots = vui_stack_new();
	vui_stack_push(gv_roots, vui_normal_mode);
	//vui_stack_push(gv_roots, cmd_tr_start);
#if 0
	vui_stack_push(gv_roots, vui_frag_release(vui_frag_catv(4,
	                          vui_frag_union(vui_frag_new_regexp("a[a-c\\]-_]df"),
	                          vui_frag_new_string("asdg")),
	                          vui_frag_union(vui_frag_new_string("asef"),
	                          vui_frag_new_string("aseg")),
	                          vui_frag_new_any(),
	                          vui_frag_union(vui_frag_new_string("atef"),
	                          vui_frag_new_string("ateg"))
	                         ), NULL));
#endif

	FILE* f = fopen("vui.dot", "w");
	vui_gv_write(f, gv_roots);
	fclose(f);

	vui_debugf("Quitting!\n");

	endwin();
	printf("Quitting!\n");

	exit(0);
#endif

#if 0
	const char* init_input = ":map a :map<sp>b<sp>:map<lt>sp>c<lt>sp>:q<lt>lt>cr><lt>cr><cr>\n";

	for (const char* p = init_input; *p; p++)
	{
		vui_input(*p);
	}
#endif

	vui_debugf("live GC objects: %zd\n", vui_gc_nlive);

	while (1)
	{
		int c = wgetch(statusline);

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_TEST)
		vui_string str;
		vui_string_new_at(&str);
		vui_string_append_printf(&str, "char %d: ", c);
		vui_string_putq(&str, c);
		vui_string_putc(&str, '\n');
		vui_debugf("%s", vui_string_get(&str));

		vui_string_dtor(&str);
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
