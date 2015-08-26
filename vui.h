#pragma once

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
// End user-declared variables

#include "statemachine.h"

typedef struct hist_entry
{
	struct hist_entry* prev;
	struct hist_entry* next;

	char* line;
	size_t len;
	size_t maxlen;
} hist_entry;

typedef struct vui_register
{
	size_t n;
	size_t maxn;
	char* s;
} vui_register;

typedef void vui_cmdline_submit_callback(char* cmd);

typedef struct vui_cmdline_def
{
	vui_state* cmdline_state;
	vui_cmdline_submit_callback* on_submit;

	int cmd_modified;

	char* label;

	hist_entry* hist_curr_entry;
	hist_entry* hist_last_entry;
} vui_cmdline_def;

extern char* vui_bar;	// Pointer to status bar - changes, you must re-check this every time!
extern int vui_crsrx;	// Current cursor position, or -1 if hidden

extern int vui_count;

extern vui_state_stack* vui_stack;

extern vui_state* vui_curr_state;

extern vui_state* vui_normal_mode;	// normal mode state
extern vui_state* vui_count_mode;	// count mode

extern vui_state* vui_register_container; // macro container state

extern vui_register* vui_register_recording;


// state machine helpers

static inline vui_transition vui_transition_return(void)
{
	return vui_transition_stack_pop(vui_stack);
}

static inline vui_state* vui_return(int pop)
{
	if (pop)
	{
		return vui_state_stack_pop(vui_stack);
	}
	else
	{
		return vui_state_stack_peek(vui_stack);
	}
}


// showcmd
void vui_showcmd_put(int c);

void vui_showcmd_reset(void);

void vui_showcmd_setup(int start, int length);


// init/resize
void vui_init(int width);		// initialize vui, set width

void vui_resize(int width);		// change width


// count
void vui_count_init(void);

// macros
void vui_macro_init(unsigned int record, unsigned int execute);

// registers
void vui_register_init(void);

vui_register* vui_register_new(void);
vui_register* vui_register_get(int c);

void vui_register_record(int c);
void vui_register_endrecord(void);
void vui_register_execute(int c);


// new modes
#define VUI_NEW_MODE_IN_MANUAL 1

vui_state* vui_mode_new(                                // create new mode
                        char* cmd,                      // default command to get to this mode
                        char* name,                     // mode name
                        char* label,                    // mode label (e.g. -- INSERT --)
                        int mode,                       // flags
                        vui_transition func_enter,      // transition on entry into mode
                        vui_transition func_in,         // transition while in mode
                        vui_transition func_exit        // transition on exit from mode via escape
);

vui_cmdline_def* vui_cmdline_mode_new(                  // create new command mode (like : or / or ?)
                            char* cmd,                  // default command to get to this mode
                            char* name,                 // name (internal)
                            char* label,                // mode label (e.g. : or / or ?) (can be multicharacter)
                            void on_submit(char* cmd)   // callback to call on submission
);


// status line
void vui_status_set(const char* s);
void vui_status_clear(void);


// input
void vui_input(unsigned int c);

void vui_reset(void);

#ifdef __cplusplus
}
#endif
