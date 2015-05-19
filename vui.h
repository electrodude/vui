#pragma once

#include "statemachine.h"


extern char** vui_bar;

extern vui_state* vui_normal_mode;


void vui_init(int width, int height);

void vui_resize(int width, int height);
