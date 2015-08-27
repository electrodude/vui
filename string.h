#pragma once

typedef struct vui_string
{
	size_t n;
	size_t maxn;
	unsigned char* s;
} vui_string;


vui_string* vui_string_new(vui_string* str);
void vui_string_kill(vui_string* str);
void vui_string_reset(vui_string* str);
unsigned char* vui_string_release(vui_string* str);

void vui_string_putc(vui_string* str, unsigned char c);
void vui_string_puts(vui_string* str, unsigned char* s);
void vui_string_put(vui_string* str, unsigned int c);

static inline unsigned char* vui_string_get(vui_string* str)
{
	str->s[str->n] = 0; // null-terminate the string (there is room)
	return str->s;
}
