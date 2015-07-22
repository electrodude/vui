#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "statemachine.h"

// The user must declare these
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

extern void vui_on_cmd_submit(char* cmd);
// end user must declare these

extern char* vui_bar;	// Pointer to status bar - changes, you must re-check this every time!
extern int vui_crsrx;	// Current cursor position, or -1 if hidden

extern vui_state* vui_normal_mode;	// normal mode state
extern vui_state* vui_cmd_mode;		// command mode state


void vui_init(int width);		// initialize vui, set width

void vui_resize(int width);		// change width


#define VUI_NEW_MODE_IN_MANUAL 1

vui_state* vui_mode_new(				// create new mode
                        char* cmd, 			// default command to get to this mode
                        char* name,			// mode name
                        char* label,			// mode label (e.g. -- INSERT --)
                        int mode, 			// flags
                        vui_transition func_enter,	// transition on entry into mode
                        vui_transition func_in,		// transition while in mode
                        vui_transition func_exit	// transition on exit from mode via escape
);

#ifdef __cplusplus
}
#endif
