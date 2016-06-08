#include <stdarg.h>
#include <string.h>

#include "vui_debug.h"

#include "vui_mem.h"

#include "vui_stack.h"

vui_stack* vui_stack_new_prealloc_at(vui_stack* stk, size_t maxn)
{
	if (stk == NULL)
	{
		stk = vui_new(vui_stack);
	}

	stk->n = 0;
	stk->maxn = maxn;
	stk->s = vui_new_array(void*, stk->maxn);
	stk->def = NULL;
	stk->dtor = NULL;

	return stk;
}

vui_stack* vui_stack_new_v_at(vui_stack* stk, size_t n, ...)
{
	va_list ap;
	va_start(ap, n);

	stk = vui_stack_new_prealloc_at(stk, n);

	for (size_t i = 0; i < n; i++)
	{
		vui_stack_push(stk, va_arg(ap, void*));
	}

	va_end(ap);

	return stk;
}

vui_stack* vui_stack_new_array_at(vui_stack* stk, size_t n, void** elements)
{
	stk = vui_stack_new_prealloc_at(stk, n);

	vui_stack_pushn(stk, n, elements);

	return stk;
}

vui_stack* vui_stack_dup_at(vui_stack* stk, vui_stack* orig)
{
	orig->dtor = NULL;	// prevent double free

	stk = vui_stack_new_prealloc_at(stk, orig->maxn);

	memcpy(stk->s, orig->s, orig->n * sizeof(void*));

	stk->n = orig->n;

	return stk;
}

void vui_stack_kill(vui_stack* stk)
{
	if (stk == NULL) return;

	vui_stack_dtor(stk);

	vui_free(stk);
}

void vui_stack_dtor(vui_stack* stk)
{
	if (stk == NULL) return;

	vui_stack_reset(stk);

	if (stk->dtor != NULL && vui_stack_def_get(stk) != NULL)
	{
		stk->dtor(vui_stack_def_get(stk));
	}

	stk->n = stk->maxn = 0;

	if (stk->s == NULL) return;

	vui_free(stk->s);

	stk->s = NULL;
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

	vui_free(stk);

	return s;
}


static inline void vui_stack_grow(vui_stack* stk, size_t maxn_new)
{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STRING)
	printf("realloc: %zd, %zd -> %zd\n", stk->n, stk->maxn, maxn_new);
#endif
	stk->maxn = maxn_new;
	stk->s = vui_resize_array(stk->s, void*, stk->maxn);
}

void* vui_stack_shrink(vui_stack* stk)
{
	if (stk == NULL) return NULL;

	vui_stack_grow(stk, stk->n);

	return stk->s;
}


void vui_stack_push(vui_stack* stk, void* s)
{
	if (stk == NULL) return;

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STACK)
	vui_debugf("Push %p\n", s);
#endif

	if (stk->maxn < stk->n + 1)
	{
		vui_stack_grow(stk, stk->n*2);
	}

	stk->s[stk->n++] = s;
}

void vui_stack_pushn(vui_stack* stk, size_t n, void** elements)
{
	if (stk == NULL) return;

	if (elements == NULL) return;

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
		vui_stack_grow(stk, n*2);
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
		vui_debugf("Pop def %p\n", stk->def);
#endif

		return stk->def;
	}

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STACK)
	vui_debugf("Pop %p\n", stk->s[stk->n-1]);
#endif

	return stk->s[--stk->n];
}

void* vui_stack_peek(vui_stack* stk)
{
	if (stk == NULL) return NULL;

	if (stk->n <= 0)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STACK)
		vui_debugf("Peek def %p\n", stk->def);
#endif

		return stk->def;
	}

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STACK)
	vui_debugf("Peek %p\n", stk->s[stk->n-1]);
#endif

	return stk->s[stk->n-1];
}

void* vui_stack_index_end(vui_stack* stk, size_t i)
{
	if (stk == NULL) return NULL;

	size_t i2 = stk->n - 1 - i;

	if (i >= stk->n)
	{
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STACK)
		vui_debugf("Index rev [-%zd = %zd] def\n", i, i2);
#endif

		return stk->def;
	}

#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STACK)
	vui_debugf("Index rev [-%zd = %zd]\n", i, i2, stk->s[i2]);
#endif

	return stk->s[i2];
	//return NULL;
}

void* vui_stack_index(vui_stack* stk, size_t i)
{
	if (stk == NULL) return NULL;

	if (i < 0 || i >= stk->n)
	{
		return NULL;
	}
#if defined(VUI_DEBUG) && defined(VUI_DEBUG_STACK)
	vui_debugf("Index [%zd] = %p\n", i, stk->s[i]);
#endif

	return stk->s[i];
}

void vui_stack_foreach(vui_stack* stk, void (*func)(void* elem))
{
	if (stk == NULL) return;

	for (size_t i = 0; i < stk->n; i++)
	{
		func(stk->s[i]);
	}
}

vui_stack* vui_stack_map(vui_stack* stk, void* (*func)(void* elem))
{
	if (stk == NULL) return NULL;

	vui_stack* stk2 = vui_stack_new_prealloc(stk->n);

	for (size_t i = 0; i < stk->n; i++)
	{
		vui_stack_push(stk2, func(stk->s[i]));
	}

	return stk2;
}
