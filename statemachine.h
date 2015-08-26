#pragma once

//#define VUI_DEBUG

#ifdef __cplusplus
extern "C"
{
#endif

#define VUI_STATE_BITS 8
#define VUI_MAXSTATE (1 << VUI_STATE_BITS)

#ifdef VUI_DEBUG
extern void vui_debug(char* msg);  // user must declare this
#endif

struct vui_state;

/*
 * \param prevstate previous state
 * \param act whether side effects should happen (as opposed to just returning the next state)
 * \param data extra data (next state, etc.)
 * \return next state (ignored if transition.next != NULL)
 *
 */
typedef struct vui_state* (*vui_transition_callback)(struct vui_state* currstate, unsigned int c, int act, void* data);

typedef struct vui_transition
{
	struct vui_state* next;

	vui_transition_callback func;

	void* data;

} vui_transition;

typedef struct vui_state
{
	struct vui_transition next[VUI_MAXSTATE];


	int refs;

	void* data;

} vui_state;



vui_state* vui_state_new(vui_state* parent);
vui_state* vui_state_new_t(vui_transition parent);
vui_state* vui_state_cow(vui_state* parent, unsigned int c);

void vui_state_replace(vui_state* state, vui_transition search, vui_transition replacement);


static inline vui_transition vui_transition_new1(vui_state* next)
{
	return (vui_transition){.next = next, .func = NULL, .data = NULL};
}

static inline vui_transition vui_transition_new2(vui_transition_callback func, void* data)
{
	return (vui_transition){.next = NULL, .func = func, .data = data};
}

static inline vui_transition vui_transition_new3(vui_state* next, vui_transition_callback func, void* data)
{
	return (vui_transition){.next = next, .func = func, .data = data};
}

vui_transition vui_transition_sameas_s(vui_state* other);
vui_transition vui_transition_sameas_t(vui_transition* t);


void vui_codepoint_to_utf8(unsigned int c, unsigned char* s);

void vui_set_char_t_raw(vui_state* state, unsigned int c, vui_transition next);
void vui_set_char_t(vui_state* state, unsigned int c, vui_transition next);

static inline void vui_set_char_s(vui_state* state, unsigned int c, vui_state* next)
{
	vui_set_char_t(state, c, vui_transition_new1(next));
}

void vui_set_range_t(vui_state* state, unsigned int c1, unsigned int c2, vui_transition next);

static inline void vui_set_range_s(vui_state* state, unsigned int c1, unsigned int c2, vui_state* next)
{
	vui_set_range_t(state, c1, c2, vui_transition_new1(next));
}

void vui_set_string_t(vui_state* state, unsigned char* s, vui_transition next);

static inline void vui_set_string_s(vui_state* state, unsigned char* s, vui_state* next)
{
	vui_set_string_t(state, s, vui_transition_new1(next));
}



vui_state* vui_next_t(vui_state* currstate, unsigned int c, vui_transition t, int act);
vui_state* vui_next(vui_state* currstate, unsigned int c, int act);
static inline vui_state* vui_next_raw(vui_state* currstate, unsigned int c, int act)
{
	return vui_next_t(currstate, c, currstate->next[c], act);
}

vui_state* vui_run_c_p(vui_state** sp, unsigned int c, int act);
vui_state* vui_run_s_p(vui_state** sp, unsigned char* s, int act);
vui_state* vui_run_s(vui_state* st, unsigned char* s, int act);

#ifdef __cplusplus
}
#endif
