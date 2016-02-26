#ifndef VUI_FRAGMENT_H
#define VUI_FRAGMENT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "vui_stack.h"

#include "vui_statemachine.h"

typedef struct vui_frag
{
	vui_state* entry;
	vui_stack* exits;
} vui_frag;

// Create a fragment given an entry point and a stack of exit points
vui_frag* vui_frag_new(vui_state* entry, vui_stack* exits);

// Destroy a fragment
void vui_frag_kill(vui_frag* frag);

// Clone a fragment
vui_frag* vui_frag_dup(vui_frag* orig);

// Fill in the exit points of a fragment and release the completed state machine
// Destroys the fragment, since it isn't useful anymore
vui_state* vui_frag_release(vui_frag* frag, vui_state* exit);

#ifdef __cplusplus
}
#endif

#endif
