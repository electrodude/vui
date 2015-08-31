#include "gc.h"

#include "graphviz.h"

int gv_id = 0;

int vui_transition_cmp(vui_transition* t1, vui_transition* t2)
{
	return t1 == t2 || (t1->next == t2->next && t1->func == t2->func && t1->data == t2->data);
}

int vui_transition_samedest(vui_state* currstate, unsigned int c, vui_transition* t1, vui_transition* t2)
{
	return t1 == t2 || t1->next == t2->next || (t1->func == t2->func && t1->data == t2->data && vui_next_t(currstate, c, *t1, VUI_ACT_TEST) == vui_next_t(currstate, c, *t2, VUI_ACT_TEST));
}

void vui_gv_putc(FILE* f, int c)
{
	if (c >= 32 && c < 127)
	{
		if (c == '\'')
		{
			fprintf(f, "'\\''");
		}
		else if (c == '\\')
		{
			fprintf(f, "\\\\\\\\");
		}
		else
		{
			fprintf(f, "'%c'", c);
		}
	}
	else if (c == '\n')
	{
		fprintf(f, "'\\\\n'");
	}
	else if (c == '\r')
	{
		fprintf(f, "'\\\\r'");
	}
	else if (c == '\t')
	{
		fprintf(f, "'\\\\t'");
	}
	else
	{
		fprintf(f, "%d", c);
	}
}

void vui_gv_puts(FILE* f, unsigned char* s)
{
	while (*s)
	{
		char c = *s++;
		if (c >= 32 && c < 127)
		{
			if (c == '\'')
			{
				fprintf(f, "\\'");
			}
			else if (c == '\\')
			{
				fprintf(f, "\\\\\\\\");
			}
			else
			{
				fprintf(f, "%c", c);
			}
		}
		else if (c == '\n')
		{
			fprintf(f, "\\\\n");
		}
		else if (c == '\r')
		{
			fprintf(f, "\\\\r");
		}
		else if (c == '\t')
		{
			fprintf(f, "\\\\t");
		}
		else
		{
			fprintf(f, "%d", c);
		}
	}
}

void vui_gv_write(FILE* f, vui_stack* roots)
{
	vui_iter_gen++;
	gv_id = 0;

	fprintf(f, "digraph vui\n{\n");

	for (int i=0; i < roots->n; i++)
	{
		vui_state* root = roots->s[i];

		vui_gv_print_s(f, root);
	}

	fprintf(f, "}\n");
}

void vui_gv_print_s(FILE* f, vui_state* s)
{
	if (s->iter_gen == vui_iter_gen) return;

	s->iter_gen = vui_iter_gen;

	s->iter_id = gv_id++;

	for (int i = 0; i < VUI_MAXSTATE; i++)
	{
		vui_state* next = vui_next(s, i, VUI_ACT_TEST);

		if (next != NULL)
		{
			vui_gv_print_s(f, next);
		}
	}

	fprintf(f, "\t\"%d\" [label=\"", s->iter_id);

	if (s->name != NULL)
	{
		vui_gv_puts(f, s->name);
	}
	else
	{
		fprintf(f, "%d", s->iter_id);
	}


	for (int i = 0; i < VUI_MAXSTATE; i++)
	{
		vui_transition t = s->next[i];
		vui_state* next = vui_next(s, i, VUI_ACT_TEST);

		if (next->iter_other != s)
		{
			int lastj = -1;
			vui_state* lastnext = NULL;

			int firstmatch = 1;

			fprintf(f, "\"];\n\t\"%d\" -> \"%d\" [label=\"", s->iter_id, next->iter_id);

			for (int j = 0; j < VUI_MAXSTATE; j++)
			{
				if (next == vui_next(s, j, VUI_ACT_TEST)) // match
				{
					if (lastj == -1) // first match
					{
						lastj = j; // record start of range
						lastnext = next;

						if (!firstmatch)
						{
							fprintf(f, ", ");
						}
						firstmatch = 0;

						vui_gv_putc(f, j); // print start of range
					}
				}
				else if (lastj != -1) // immediately after last match
				{
					if (lastj != j-1) // if range has more than one element
					{
						fprintf(f, " - "); // print range
						vui_gv_putc(f, j-1);
					}

					lastj = -1;
				}
			}

			if (lastj != -1 && lastj != VUI_MAXSTATE-1)
			{
				fprintf(f, " - "); // print range
				vui_gv_putc(f, VUI_MAXSTATE-1);
			}
		}

		next->iter_other = s;
	}


	fprintf(f, "\"];\n");
}
