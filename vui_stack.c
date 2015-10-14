#include <string.h>

#include "vui_debug.h"

#include "vui_stack.h"

vui_stack* vui_stack_new_prealloc(size_t maxn)
{
	vui_stack* stk = malloc(sizeof(vui_stack));

	stk->n = 0;
	stk->maxn = maxn;
	stk->s = malloc(stk->maxn*sizeof(void*));
	stk->def = NULL;
	stk->dtor = NULL;

	return stk;
}

void vui_stack_kill(vui_stack* stk)
{
	if (stk == NULL) return;

	free(stk->s);

	free(stk);
}

void vui_stack_trunc(vui_stack* stk, size_t n)
{
	if (stk == NULL) return;

	if (stk->dtor != NULL)
	{
		while (stk->n > n)
		{
			stk->dtor(stk->s[--stk->n]);
		}
	}
	else
	{
		stk->n = 0;
	}
}

void** vui_stack_release(vui_stack* stk, size_t* n)
{
	if (stk == NULL)
	{
		*n = 0;
		return NULL;
	}

	vui_stack_shrink(stk);

	void** s = stk->s;
	*n = stk->n;

	free(stk);

	return s;
}


void* vui_stack_shrink(vui_stack* stk)
{
	if (stk == NULL) return NULL;

	stk->s = realloc(stk->s, stk->n*sizeof(void*));

	return stk->s;
}


void vui_stack_push(vui_stack* stk, void* s)
{
	if (stk == NULL) return;

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STACK)
	char s2[64];
	snprintf(s2, 64, "Push 0x%lX\r\n", s2);
	vui_debug(s2);
#endif

	if (stk->maxn < stk->n + 1)
	{
		stk->maxn = stk->n*2;
		stk->s = realloc(stk->s, stk->maxn*sizeof(void*));
	}

	stk->s[stk->n++] = s;
}

void vui_stack_append(vui_stack* stk, vui_stack* stk2)
{
	if (stk == NULL) return;

	if (stk2 == NULL) return;

	size_t n = stk->n + stk2->n;
	// make room for stk2
	if (stk->maxn < n);
	{
		stk->maxn = n*2;
		stk->s = realloc(stk->s, stk->maxn);
	}

	memcpy(&stk->s[stk->n], stk2->s, stk2->n*sizeof(void*));

	stk->n = n;

	vui_stack_kill(stk2);
}

void vui_stack_push_nodup(vui_stack* stk, void* s)
{
	if (stk == NULL) return;

	if (s == vui_stack_peek(stk)) return;

	vui_stack_push(stk, s);
}

void* vui_stack_pop(vui_stack* stk)
{
	if (stk == NULL) return NULL;

	if (stk->n <= 0)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STACK)
		char s[64];
		snprintf(s, 64, "Pop def 0x%lX\r\n", stk->def);
		vui_debug(s);
#endif

		return stk->def;
	}

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STACK)
	char s[64];
	snprintf(s, 64, "Pop 0x%lX\r\n", stk->s[stk->n-1]);
	vui_debug(s);
#endif

	return stk->s[--stk->n];
}

void* vui_stack_peek(vui_stack* stk)
{
	if (stk == NULL) return NULL;

	if (stk->n <= 0)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STACK)
		char s[64];
		snprintf(s, 64, "Peek def 0x%lX\r\n", stk->def);
		vui_debug(s);
#endif

		return stk->def;
	}

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STACK)
	char s[64];
	snprintf(s, 64, "Peek 0x%lX\r\n", stk->s[stk->n-1]);
	vui_debug(s);
#endif

	return stk->s[stk->n-1];
}

void* vui_stack_index(vui_stack* stk, unsigned int i)
{
	if (stk == NULL) return NULL;

	if (i >= stk->n)
	{
		return NULL;
	}
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STACK)
	char s[64];
	snprintf(s, 64, "Index [i] = 0x%lX\r\n", i, stk->s[i]);
	vui_debug(s);
#endif

	return stk->s[i];
}
