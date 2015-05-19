#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "statemachine.h"

vui_state* vui_curr_state;


vui_state* vui_state_new(vui_state* parent)
{
	vui_state* this = malloc(sizeof(vui_state));

	this->refs = 0;

	if (parent != NULL)
	{
		this->templatestate = parent->templatestate;

		for (int i=0; i<MAXINPUT; i++)
		{
			this->next[i] = parent->next[i];
			vui_next(this, i, 0)->refs++;
		}
	}
	else
	{
		vui_transition next = vui_transition_new1(this);

		this->templatestate = next;
		this->refs++;

		for (int i=0; i<MAXINPUT; i++)
		{
			this->next[i] = next;
			this->refs++;
		}
	}

	return this;
}

vui_state* vui_state_cow(vui_state* parent, int c)
{
	vui_state* this = parent->next[c].next;

	if (this == NULL) return NULL;

	if (this->refs == 1) return this;

	vui_state* newthis = vui_state_new(this);

	vui_set_char_t(parent, c, vui_transition_new1(newthis));

	return newthis;
}

int vui_state_kill(vui_state* this)
{
	if (this == NULL) return 0;

	if (--this->refs != 0) return 0;

	for (int i=0; i<MAXINPUT; i++)
	{
		vui_state* next = vui_next(this, i, 0);
		if (next != this)
		{
			vui_state_kill(next);
		}
	}

	free(this);
}

void vui_state_replace(vui_state* this, vui_transition search, vui_transition replacement)
{
	for (int i=0; i<MAXINPUT; i++)
	{
		vui_transition t = this->next[i];

		if (t.callback == search.callback && t.data == search.data)
		{
			this->next[i] = replacement;
		}
	}
}

vui_transition vui_transition_new(vui_callback callback, void* data)
{
	return (vui_transition){.next = NULL, .callback = callback, .data = data};
}

vui_transition vui_transition_new1(vui_state* next)
{
	return (vui_transition){.next = next, .callback = NULL, .data = NULL};
}


void vui_set_char_t(vui_state* this, int c, vui_transition next)
{
	vui_state* nextst = vui_next(this, c, 0);

	if (nextst != NULL)
	{
		nextst->refs++;
	}


	vui_state* oldnext = vui_next(this, c, 0);

	this->next[c] = next;

	vui_state_kill(oldnext);
}

void vui_set_char_s(vui_state* this, int c, vui_state* next)
{
	vui_set_char_t(this, c, vui_transition_new1(next));
}

void vui_set_range_t(vui_state* this, int c1, int c2, vui_transition next)
{
	for (int c = c1; c != c2; c = (c+1) % MAXINPUT)
	{
		vui_set_char_t(this, c, next);
	}
}

void vui_set_range_s(vui_state* this, int c1, int c2, vui_state* next)
{
	vui_set_range_t(this, c1, c2, vui_transition_new1(next));
}

void vui_set_string_t(vui_state* this, char* s, vui_transition next)
{
	vui_transition t = this->next[*s];

	if (t.next == NULL)
	{
		printf("vui_set_string_t: bad transition! String: %s\n", s);
	}
	else
	{
		if (s[1])
		{
			vui_state* nextst = vui_state_cow(this, *s);
			vui_set_string_t(nextst, s+1, next);
		}
		else
		{
			vui_set_char_t(this, *s, next);
		}
	}
}

void vui_set_string_s(vui_state* this, char* s, vui_state* next)
{
	vui_set_string_t(this, s, vui_transition_new1(next));
}

vui_state* vui_next(vui_state* s, int c, int act)
{
	vui_transition t = s->next[c];

	if (t.next != NULL) return t.next;

	if (t.callback == NULL) return NULL;

	return t.callback(s, c, act, t.data);
}

void vui_input(int c)
{
	vui_curr_state = vui_next(vui_curr_state, c, 1);
}

void vui_runstring(vui_state** sp, char* s, int act)
{
	for (;*s;s++)
	{
		*sp = vui_next(*sp, *s, act);
	}
}

