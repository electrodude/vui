#pragma once

#include <stdlib.h>

// String builder class
//
// N.B.: The null terminator is not present on str->s except after calls to
//  `vui_string_release`, `vui_string_shrink`, and `vui_string_get`.

typedef struct vui_string
{
	size_t n;         // current length of string
	                  //  (not counting null terminator)
	size_t maxn;      // allocated length of buffer
	unsigned char* s; // pointer to buffer
} vui_string;

// Create a new string
// if str == NULL, malloc the string
// otherwise, put it at *str
vui_string* vui_string_new(vui_string* str);

// Destroy a string and its buffer
void vui_string_kill(vui_string* str);

// Destroy string, returning its internal buffer
// The caller assumes responsibilty for free()ing the returned buffer.
// Appends null terminator
unsigned char* vui_string_release(vui_string* str);


// Reset a string to zero length
// This does nothing but `str->n = 0`; it neither null-terminates it
//  nor shrinks the allocated size of the internal buffer.
static inline void vui_string_reset(vui_string* str)
{
	str->n = 0;
}

// realloc() internal buffer to be as small as possible
// Appends null terminator
void vui_string_shrink(vui_string* str);


// Append a character
void vui_string_putc(vui_string* str, unsigned char c);

// Append a null-terminated string
void vui_string_puts(vui_string* str, unsigned char* s);

// Append a codepoint to be encoded as UTF-8
void vui_string_put(vui_string* str, unsigned int c);


// Get pointer to internal buffer
// The pointer is only valid until the next call to `vui_string_put*`.
// Appends null terminator
static inline unsigned char* vui_string_get(vui_string* str)
{
	if (str == NULL) return NULL;

	str->s[str->n] = 0; // null-terminate the string
	                    //  (there is room already allocated)
	return str->s;
}
