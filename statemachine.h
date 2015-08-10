#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef MAXINPUT
# define MAXINPUT 512
#endif

struct vui_state;

/*
 * \param prevstate previous state
 * \param act whether side effects should happen (as opposed to just returning the next state)
 * \param data extra data (next state, etc.)
 * \return next state (ignored if transition.next != NULL)
 *
 */
typedef struct vui_state* (*vui_callback)(struct vui_state* currstate, int c, int act, void* data);

typedef struct vui_transition
{
	struct vui_state* next;

	vui_callback func;

	void* data;

} vui_transition;

typedef struct vui_state
{
	struct vui_transition next[MAXINPUT];

	struct vui_transition templatestate;

	int refs;

	void* data;

} vui_state;


extern vui_state* vui_curr_state;


vui_state* vui_state_new(vui_state* parent);
vui_state* vui_state_cow(vui_state* parent, int c);

void vui_state_replace(vui_state* state, vui_transition search, vui_transition replacement);

vui_transition vui_transition_new1(vui_state* next);
vui_transition vui_transition_new2(vui_callback func, void* data);
vui_transition vui_transition_new3(vui_state* next, vui_callback func, void* data);


void vui_set_char_t(vui_state* state, int c, vui_transition next);
void vui_set_char_s(vui_state* state, int c, vui_state* next);

void vui_set_range_t(vui_state* state, int c1, int c2, vui_transition next);
void vui_set_range_s(vui_state* state, int c1, int c2, vui_state* next);

void vui_set_string_t(vui_state* state, char* s, vui_transition next);
void vui_set_string_s(vui_state* state, char* s, vui_state* next);


vui_state* vui_next(vui_state* s, int c, int act);
void vui_input(int c);
void vui_runstring(vui_state** sp, char* s, int act);

#ifdef __cplusplus
}
#endif
