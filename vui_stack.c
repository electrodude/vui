#include <stdarg.h>
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

vui_stack* vui_stack_new_v(size_t n, ...)
{
	va_list ap;
	va_start(ap, n);

	vui_stack* stk = vui_stack_new_prealloc(n);

	for (size_t i = 0; i < n; i++)
	{
		vui_stack_push(stk, va_arg(ap, void*));
	}

	va_end(ap);

	return stk;
}

vui_stack* vui_stack_new_array(size_t n, void** elements)
{
	vui_stack* stk = vui_stack_new_prealloc(n);

	vui_stack_pushn(stk, n, elements);

	return stk;
}

void vui_stack_kill(vui_stack* stk)
{
	if (stk == NULL) return;

	vui_stack_reset(stk);

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
		stk->n = n;
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

	stk->maxn = stk->n;

	stk->s = realloc(stk->s, stk->maxn*sizeof(void*));

	return stk->s;
}


void vui_stack_push(vui_stack* stk, void* s)
{
	if (stk == NULL) return;

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STACK)
	vui_debugf("Push 0x%lX\n", s);
#endif

	if (stk->maxn < stk->n + 1)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STACK)
		printf("realloc: %ld, %ld\n", stk->maxn, stk->n);
#endif
		stk->maxn = stk->n*2;
		stk->s = realloc(stk->s, stk->maxn*sizeof(void*));
	}

	stk->s[stk->n++] = s;
}

void vui_stack_pushn(vui_stack* stk, size_t n, void** elements)
{
	if (stk == NULL) return;

	for (size_t i = 0; i < n; i++)
	{
		vui_stack_push(stk, elements[i]);
	}
}

void vui_stack_append(vui_stack* stk, vui_stack* stk2)
{
	if (stk == NULL) return;

	if (stk2 == NULL) return;

	size_t n = stk->n + stk2->n;
	// make room for stk2
	if (stk->maxn < n)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STACK)
		printf("realloc: %ld, %ld\n", stk->maxn, stk->n);
#endif
		stk->maxn = n*2;
		stk->s = realloc(stk->s, stk->maxn);
	}

	memcpy(&stk->s[stk->n], stk2->s, stk2->n*sizeof(void*));

	stk->n = n;
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
		vui_debugf("Pop def 0x%lX\n", stk->def);
#endif

		return stk->def;
	}

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STACK)
	vui_debugf("Pop 0x%lX\n", stk->s[stk->n-1]);
#endif

	return stk->s[--stk->n];
}

void* vui_stack_peek(vui_stack* stk)
{
	if (stk == NULL) return NULL;

	if (stk->n <= 0)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STACK)
		vui_debugf("Peek def 0x%lX\n", stk->def);
#endif

		return stk->def;
	}

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STACK)
	vui_debugf("Peek 0x%lX\n", stk->s[stk->n-1]);
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
	vui_debugf("Index [i] = 0x%lX\n", i, stk->s[i]);
#endif

	return stk->s[i];
}

void vui_stack_foreach(vui_stack* stk, void (*func)(void* elem))
{
	for (size_t i = 0; i < stk->n; i++)
	{
		func(stk->s[i]);
	}
}

vui_stack* vui_stack_map(vui_stack* stk, void* (*func)(void* elem))
{
	vui_stack* stk2 = vui_stack_new_prealloc(stk->n);

	for (size_t i = 0; i < stk->n; i++)
	{
		vui_stack_push(stk2, func(stk->s[i]));
	}

	return stk2;
}
