#include <stdlib.h>
#include <string.h>

#include "vui.h"


char** vui_bar;
int lines;
int cols;


vui_state* vui_normal_mode;


void vui_init(int width, int height)
{
	vui_normal_mode = vui_curr_state = vui_state_new(NULL);

	lines = height;
	cols = width;

	vui_bar = malloc(height*sizeof(char*));

	for (int y=0; y<lines; y++)
	{
		vui_bar[y] = malloc((cols+1)*sizeof(char));
		memset(vui_bar[y], ' ', cols);
		vui_bar[y][cols] = 0;
	}
}

void vui_resize(int width, int height)
{
	int oldh = lines;
	int oldw = cols;

	lines = height;
	cols = width;

	int minh = oldh > lines ? lines : oldh;

	if (oldw != cols)
	{
		for (int y=0; y<minh; y++)
		{
			vui_bar[y] = realloc(vui_bar[y], (cols+1)*sizeof(char));
			vui_bar[y][cols] = 0;
		}
	}

	if (oldh != lines)
	{
		if (oldh > lines)
		{
			for (int y=lines; y<oldh; y++)
			{
				free(vui_bar[y]);
			}
		}

		vui_bar = realloc(vui_bar, lines*sizeof(char*));

		if (oldh < lines)
		{
			for (int y=oldh; y<lines; y++)
			{
				vui_bar[y] = malloc((cols+1)*sizeof(char));
				memset(vui_bar[y], ' ', cols);
				vui_bar[y][cols] = 0;
			}
		}
	}
}
