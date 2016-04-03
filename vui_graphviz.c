#include <string.h>

#include "vui_debug.h"

#include "vui_gc.h"

#include "vui_graphviz.h"

int gv_id = 0;

int vui_transition_cmp(vui_transition* t1, vui_transition* t2)
{
	return t1 == t2 || (t1->next == t2->next && t1->func == t2->func && t1->data == t2->data);
}

int vui_transition_samedest(vui_state* currstate, unsigned int c, vui_transition* t1, vui_transition* t2)
{
	return t1 == t2 || t1->next == t2->next || (t1->func == t2->func && t1->data == t2->data && vui_next_t(currstate, c, t1, VUI_ACT_TEST) == vui_next_t(currstate, c, t2, VUI_ACT_TEST));
}


void vui_gv_write(FILE* f, vui_stack* roots)
{
	vui_string out;
	vui_string_new_at(&out);

	vui_gv_write_str(&out, roots);

	fwrite(vui_string_get(&out), 1, out.n, f);

	vui_string_dtor(&out);
}

void vui_gv_write_str(vui_string* out, vui_stack* roots)
{
	vui_iter_gen++;
	gv_id = 0;

	vui_string_puts(out, "digraph vui\n{\n");

	vui_string_puts(out, "\tsplines=true;\n");
	vui_string_puts(out, "\toverlap=scalexy;\n");

	for (size_t i=0; i < roots->n; i++)
	{
		vui_state* root = roots->s[i];

		vui_gv_print_s(out, root);
	}

	vui_string_puts(out, "}\n");
}

static void vui_gv_print_set(vui_string* out, unsigned int i, int* covered)
{
	int state = 0;

	for (unsigned int j = i; j < VUI_MAXSTATE + 1; j++)
	{
		if (covered[j] == i)
		{
			if (state == 0)
			{
				vui_string_putq(out, j);
				state = 1;
			}
			else if (state == 1)
			{
				vui_string_putc(out, '-');
				state = 2;
			}
		}
		else
		{
			if (state == 2)
			{
				vui_string_putq(out, j-1);
			}

			state = 0;
		}
	}
}

void vui_gv_print_t(vui_string* out, vui_state* currstate, vui_transition* t)
{
	if (t->iter_gen == vui_iter_gen) return;

	t->iter_gen = vui_iter_gen;

	t->iter_id = gv_id++;

	if (t->next == NULL)
	{
		int covered[VUI_MAXSTATE + 1];
		for (unsigned int i = 0; i < VUI_MAXSTATE; i++) covered[i] = -1;


		for (unsigned int i = 0; i < VUI_MAXSTATE; i++)
		{
			vui_state* s = vui_next_t(currstate, i, t, VUI_ACT_TEST);

			if (covered[i] == -1)
			{
				if (s != NULL)
				{
					vui_gv_print_s(out, s);
				}

				for (unsigned int j = 0; j < VUI_MAXSTATE; j++)
				{
					vui_state* s2 = vui_next_t(currstate, j, t, VUI_ACT_TEST);

					if (s == s2)
					{
						covered[j] = i;
					}
				}
			}
		}

		vui_string_append_printf(out, "\t\"t_%d\" [width=0.2,height=0.2,fixedwidth=true,label=\"", t->iter_id);


		vui_string tmp;
		vui_string_new_at(&tmp);

		for (unsigned int i = 0; i < VUI_MAXSTATE; i++)
		{
			if (covered[i] == i)
			{
				vui_state* s = vui_next_t(currstate, i, t, VUI_ACT_TEST);

				if (s != NULL)
				{
					vui_string_append_printf(out, "\"];\n\t\"t_%d\" -> \"s_%d\" [label=\"", t->iter_id, s->iter_id);
				}
				else
				{
					vui_string_append_printf(out, "\"];\n\t\"t_%d\" -> \"NULL\" [label=\"", t->iter_id);
				}

				vui_string_reset(&tmp);
				vui_gv_print_set(&tmp, i, covered);
				vui_string_append_quote(out, &tmp);
			}
		}

		vui_string_dtor(&tmp);
	}
	else
	{
		vui_state* s = t->next;

		vui_gv_print_s(out, s);

		vui_string_append_printf(out, "\t\"t_%d\" [width=0.2,height=0.2,fixedwidth=true,label=\"", t->iter_id);

		vui_string_append_printf(out, "\"];\n\t\"t_%d\" -> \"s_%d\" [label=\"", t->iter_id, s->iter_id);
	}

	vui_string_puts(out, "\"];\n");
}

void vui_gv_print_s(vui_string* out, vui_state* s)
{
	if (s->iter_gen == vui_iter_gen) return;

	s->iter_gen = vui_iter_gen;

	s->iter_id = gv_id++;

	if (s->gv_norank)
	{
		return;
	}

	for (unsigned int i = 0; i < VUI_MAXSTATE; i++)
	{
		vui_transition* t = vui_state_index(s, i);

		if (t != NULL)
		{
			vui_gv_print_t(out, s, t);
		}
	}

	vui_string_append_printf(out, "\t\"s_%d\" [label=\"", s->iter_id);

	if (s->name.n > 0)
	{
		vui_string_append_quote(out, &s->name);
	}
	else
	{
		vui_string_append_printf(out, "%d", s->iter_id);
	}

	int covered[VUI_MAXSTATE + 1];
	for (unsigned int i = 0; i < VUI_MAXSTATE; i++) covered[i] = -1;


	for (unsigned int i = 0; i < VUI_MAXSTATE; i++)
	{
		vui_transition* t = vui_state_index(s, i);

		if (covered[i] == -1)
		{
			if (t != NULL)
			{
				vui_gv_print_t(out, s, t);
			}

			for (unsigned int j = 0; j < VUI_MAXSTATE; j++)
			{
				vui_transition* t2 = vui_state_index(s, j);

				if (t == t2)
				{
					covered[j] = i;
				}
			}
		}
	}


	vui_string tmp;
	vui_string_new_at(&tmp);

	for (unsigned int i = 0; i < VUI_MAXSTATE; i++)
	{
		if (covered[i] == i)
		{
			vui_transition* t = vui_state_index(s, i);

			if (t != NULL)
			{
				vui_string_append_printf(out, "\"];\n\t\"s_%d\" -> \"t_%d\" [label=\"", s->iter_id, t->iter_id);

				vui_string_reset(&tmp);
				vui_gv_print_set(&tmp, i, covered);
				vui_string_append_quote(out, &tmp);
			}

		}
	}

	vui_string_dtor(&tmp);


	vui_string_puts(out, "\"];\n");
}
