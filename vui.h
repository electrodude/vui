#ifndef VUI_H
#define VUI_H

#ifdef __cplusplus
extern "C"
{
#endif

// Begin user-declared variables
extern int VUI_KEY_UP;
extern int VUI_KEY_DOWN;
extern int VUI_KEY_LEFT;
extern int VUI_KEY_RIGHT;
extern int VUI_KEY_ENTER;
extern int VUI_KEY_BACKSPACE;
extern int VUI_KEY_DELETE;
extern int VUI_KEY_ESCAPE;
extern int VUI_KEY_HOME;
extern int VUI_KEY_END;
extern int VUI_KEY_MODIFIER_CONTROL;
// End user-declared variables

#include "vui_string.h"

#include "vui_translator.h"

#include "vui_statemachine.h"

typedef struct hist_entry
{
	struct hist_entry* prev;
	struct hist_entry* next;

	vui_string line;
} hist_entry;

typedef void vui_cmdline_submit_callback(vui_stack* cmd);

typedef struct vui_cmdline
{
	vui_state* cmdline_state;
	vui_cmdline_submit_callback* on_submit;

	vui_translator* tr;

	int cmd_modified;

	vui_string label;

	hist_entry* hist_curr_entry;
	hist_entry* hist_last_entry;
} vui_cmdline;

extern char* vui_bar;	// Pointer to status bar - changes, you must re-check this every time!
extern int vui_crsrx;	// Current cursor position, or -1 if hidden

extern int vui_count;

extern vui_stack* vui_state_stack;

extern vui_state* vui_curr_state;

extern vui_state* vui_normal_mode;	// normal mode state
extern vui_state* vui_count_mode;	// count mode

extern vui_state* vui_register_container; // macro container state

extern vui_string* vui_register_recording;

extern vui_string* vui_register_curr;


// state machine helpers

static inline vui_transition vui_transition_return(void)
{
	return vui_transition_stack_pop(vui_state_stack);
}

static inline vui_state* vui_return(int act)
{
	if (act > 0)
	{
		return vui_state_stack_pop(vui_state_stack);
	}
	else
	{
		return vui_stack_peek(vui_state_stack);
	}
}

static inline vui_state* vui_return_n(int act, size_t n)
{
	if (act > 0)
	{
		while (n-- > 0) vui_state_stack_pop(vui_state_stack);

		return vui_state_stack_pop(vui_state_stack);
	}
	else
	{
		return vui_stack_index_end(vui_state_stack, n);
	}
}


// showcmd
void vui_showcmd_put(int c);

void vui_showcmd_reset(void);

void vui_showcmd_setup(int start, int length);

vui_state* vui_tfunc_showcmd_put(vui_state* currstate, unsigned int c, int act, void* data);
static inline vui_transition vui_transition_new_showcmd_put(vui_state* next)
{
	return vui_transition_new3(next, vui_tfunc_showcmd_put, NULL);
}


// init/resize
void vui_init(int width);		// initialize vui, set width

void vui_deinit(void);

void vui_resize(int width);		// change width


// count
void vui_count_init(void);

// macros
void vui_macro_init(unsigned int record, unsigned int execute);

// registers
void vui_register_init(void);

vui_string* vui_register_get(unsigned int c);

void vui_register_record(unsigned int c);
void vui_register_endrecord(void);
vui_state* vui_register_execute(vui_state* currstate, unsigned int c, int act);


// keybinds

void vui_bind_u(vui_state* mode, unsigned int c, vui_transition t);
void vui_bind(vui_state* mode, unsigned char* s, vui_transition t);
void vui_bind_str(vui_state* mode, vui_string* s, vui_transition t);

void vui_map(vui_state* mode, vui_string* action, vui_string* reaction);
void vui_map2(vui_state* mode, vui_string* action, vui_state* reaction_st, vui_string* reaction_str);

// misc callbacks
vui_state* vui_tfunc_normal(vui_state* currstate, unsigned int c, int act, void* data);
static inline vui_transition vui_transition_new_normal(void)
{
	return vui_transition_new3(vui_normal_mode, vui_tfunc_normal, NULL);
}

vui_state* vui_tfunc_status_set(vui_state* currstate, unsigned int c, int act, void* data);
static inline vui_transition vui_transition_new_status_set(vui_state* next, char* str)
{
	return vui_transition_new3(next, vui_tfunc_status_set, str);
}

vui_state* vui_tfunc_status_clear(vui_state* currstate, unsigned int c, int act, void* data);
static inline vui_transition vui_transition_new_clear(vui_state* next)
{
	return vui_transition_new3(next, vui_tfunc_status_clear, NULL);
}

// new modes
#define VUI_MODE_NEW_MANUAL_IN 0x1
#define VUI_MODE_NEW_INHERIT   0x2

vui_state* vui_mode_new(                                           // create new mode
                        char* cmd,                                 // default command to get to this mode
                        char* name,                                // mode name
                        char* label,                               // mode label (e.g. -- INSERT --)
                        int mode,                                  // flags
                        vui_transition func_enter,                 // transition on entry into mode
                        vui_transition func_in,                    // transition while in mode
                        vui_transition func_exit                   // transition on exit from mode via escape
);

vui_cmdline* vui_cmdline_new(                                      // create new command mode (like : or / or ?)
                        char* cmd,                                 // default command to get to this mode
                        char* name,                                // name (internal)
                        char* label,                               // mode label (e.g. : or / or ?) (can be multicharacter)
                        vui_translator* tr,                        // parser
                        vui_cmdline_submit_callback on_submit      // callback to call on submission
);

void vui_cmdline_kill(vui_cmdline* cmdline);


// status line
void vui_status_set(const char* s);
void vui_status_clear(void);


// input
void vui_input(unsigned int c);

void vui_reset(void);


// deprecated old names
#define vui_cmdline_mode_new vui_cmdline_new
#define vui_cmdline_def vui_cmdline;

#ifdef __cplusplus
}
#endif

#endif
