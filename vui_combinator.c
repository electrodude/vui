#include "vui_statemachine.h"

#include "vui_combinator.h"

/*
 * vui_state_union(vui_state* lhs, vui_state* rhs)
 *
 * Consider the state machines `a -(b)-> b -(a)-> a` and `a' -(b)-> b' -(c)-> c' -(a)-> a'`.
 * What happens if you union the two?
 *
 * Currently, you will get:
 *
 * a -(b)-> b -(a)-> a
 *          b -(c)-> c -(a)-> a
 *
 * This is what Ragel does.
 *
 * One could argue that unioning the two state machines should do the following:
 *
 * a -(a)-> a
 * a -(b)-> b -(a)-> a
 *          b -(b)-> b
 *          b -(c)-> c
 * a -(c)-> c -(a)-> a
 *          c -(b)-> b
 *          c -(c)-> c
 *
 */
static void vui_state_union(vui_state* lhs, vui_state* rhs)
{
	if (rhs->iter_gen == vui_iter_gen)
	{
		return;
	}

	rhs->iter_gen = vui_iter_gen;

	for (unsigned int i = 0; i < VUI_MAXSTATE; i++)
	{
		if (vui_state_index(rhs, i).next != NULL)
		{
			if (vui_state_index(lhs, i).next == NULL)
			{
				vui_state_index(lhs, i).next = vui_state_index(rhs, i).next;
			}
			else
			{
				vui_state_union(vui_state_index(lhs, i).next, vui_state_index(rhs, i).next);
			}
		}
	}
}

vui_frag* vui_frag_union(vui_frag* lhs, vui_frag* rhs)
{
	vui_string newname;
	vui_string_new_at(&newname);
	vui_string_puts(&newname, "(");
	vui_string_append(&newname, &lhs->entry->name);
	vui_string_puts(&newname, " | ");
	vui_string_append(&newname, &rhs->entry->name);
	vui_string_puts(&newname, ")");

	vui_iter_gen++;

	vui_state_union(lhs->entry, rhs->entry);

	vui_stack_append(lhs->exits, rhs->exits);

	vui_frag_kill(rhs);

	vui_string_reset(&lhs->entry->name);
	vui_string_append(&lhs->entry->name, &newname);
	vui_string_dtor(&newname);

	return lhs;
}

vui_frag* vui_frag_unionv(size_t n, ...)
{
	if (n == 0)
	{
		return NULL;
	}

	va_list ap;
	va_start(ap, n);

	vui_frag* frag = va_arg(ap, vui_frag*);

	n--;

	for (;n;n--)
	{
		frag = vui_frag_union(frag, va_arg(ap, vui_frag*));
	}

	va_end(ap);

	return frag;
}


vui_state* vui_frag_merge(vui_state* lhs, vui_frag* rhs, vui_state* exit)
{
	vui_iter_gen++;

	vui_state* rhs_st = vui_frag_release(rhs, exit);

	vui_state_union(lhs, rhs_st);

	return lhs;
}


vui_frag* vui_frag_cat(vui_frag* lhs, vui_frag* rhs)
{
	vui_string newname;
	vui_string_new_at(&newname);
	vui_string_puts(&newname, "(");
	vui_string_append(&newname, &lhs->entry->name);
	vui_string_puts(&newname, " .. ");
	vui_string_append(&newname, &rhs->entry->name);
	vui_string_puts(&newname, ")");

	vui_gc_decr(rhs->entry);

	rhs->entry = vui_frag_release(lhs, rhs->entry);

	vui_gc_incr(rhs->entry);

	vui_string_reset(&rhs->entry->name);
	vui_string_append(&rhs->entry->name, &newname);
	vui_string_dtor(&newname);

	return rhs;
}

vui_frag* vui_frag_catv(size_t n, ...)
{
	if (n == 0)
	{
		return NULL;
	}

	va_list ap;
	va_start(ap, n);

	vui_frag* frag = va_arg(ap, vui_frag*);

	n--;

	for (;n;n--)
	{
		frag = vui_frag_cat(frag, va_arg(ap, vui_frag*));
	}

	va_end(ap);

	return frag;
}
