#include "stack.h"

vui_stack* vui_stack_new(void)
{
	vui_stack* stk = malloc(sizeof(vui_stack));

	stk->n = 0;
	stk->maxn = 16;
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

void vui_stack_trunc(vui_stack* stk, int n)
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

void** vui_stack_release(vui_stack* stk, int* n)
{
	if (stk == NULL)
	{
		*n = 0;
		return NULL;
	}

	void** s = realloc(stk->s, stk->n*sizeof(void*));
	*n = stk->n;
	free(stk);

	return s;
}

void vui_stack_push(vui_stack* stk, void* s)
{
	if (stk == NULL) return;

#ifdef VUI_DEBUG
	char s2[64];
	snprintf(s2, 64, "Push 0x%lX\r\n", s2);
	vui_debug(s2);
#endif

	if (stk->n >= stk->maxn)
	{
		stk->maxn = stk->n*2;
		stk->s = realloc(stk->s, stk->maxn*sizeof(void*));
	}

	stk->s[stk->n++] = s;
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
#ifdef VUI_DEBUG
		char s[64];
		snprintf(s, 64, "Pop def 0x%lX\r\n", stk->def);
		vui_debug(s);
#endif

		return stk->def;
	}

#ifdef VUI_DEBUG
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
#ifdef VUI_DEBUG
		char s[64];
		snprintf(s, 64, "Peek def 0x%lX\r\n", stk->def);
		vui_debug(s);
#endif

		return stk->def;
	}

#ifdef VUI_DEBUG
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
#ifdef VUI_DEBUG
	char s[64];
	snprintf(s, 64, "Index [i] = 0x%lX\r\n", i, stk->s[i]);
	vui_debug(s);
#endif

	return stk->s[i];
}