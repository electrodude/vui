#ifndef VUI_STRING_H
#define VUI_STRING_H

#ifdef __cplusplus
extern "C"
{
#endif

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
	char* s;          // pointer to buffer
} vui_string;

// Create a new string, given how many bytes to preallocate
// if str == NULL, malloc the string
// otherwise, put it at *str
#define vui_string_new_prealloc(maxn) vui_string_new_prealloc_at(NULL, maxn)
vui_string* vui_string_new_prealloc_at(vui_string* str, size_t maxn);

// Create a new string
// if str == NULL, malloc the string
// otherwise, put it at *str
#define vui_string_new() vui_string_new_at(NULL)
static inline vui_string* vui_string_new_at(vui_string* str)
{
	return vui_string_new_prealloc_at(str, 32);
}


// Create a new string
// Copy n chars starting at c into the string
#define vui_string_new_array(n, s) vui_string_new_array_at(NULL, n, s)
vui_string* vui_string_new_array_at(vui_string* str, size_t n, const char* s);


// Create a new string
// Copy a null-terminated string at s into the string
#define vui_string_new_str(s) vui_string_new_str_at(NULL, s)
vui_string* vui_string_new_str_at(vui_string* str, const char* s);

// Clone a string
#define vui_string_dup(orig) vui_string_dup_at(NULL, orig)
vui_string* vui_string_dup_at(vui_string* str, const vui_string* orig);

// Destroy a string and its buffer
void vui_string_kill(vui_string* str);

// Destruct a non-malloc'd string
void vui_string_dtor(vui_string* str);

// Destroy string, returning its internal buffer
// The caller assumes responsibilty for free()ing the returned buffer.
// Appends null terminator
char* vui_string_release(vui_string* str);


// Reset a string to zero length
// This does nothing but `str->n = 0`; it neither null-terminates it
//  nor shrinks the allocated size of the internal buffer.
static inline void vui_string_reset(vui_string* str)
{
	str->n = 0;
}

// realloc() internal buffer to be as small as possible
// Appends null terminator
// Returns pointer to internal buffer
// The pointer is only valid until the next call to `vui_string_put*`.
char* vui_string_shrink(vui_string* str);


// Append a character
void vui_string_putc(vui_string* str, char c);

// Append a null-terminated string
void vui_string_puts(vui_string* str, const char* s);

// Append a string that is n characters long
void vui_string_putn(vui_string* str, size_t n, const char* s);

// Append printf-formatted text
#define vui_string_putf(str, fmt, ...) vui_string_append_printf(str, fmt, __VA_ARGS__)
void vui_string_append_printf(vui_string* str, const char* fmt, ...);

// Append a vui_string
void vui_string_append(vui_string* str, const vui_string* str2);

// Append a vui_string, escaping things as necessary
void vui_string_append_quote(vui_string* str, const vui_string* str2);

// Append the data starting at start and ending at the byte before end
static inline void vui_string_append_stretch(vui_string* str, const char* start, const char* end)
{
	vui_string_putn(str, end - start, start);
}

// Append a codepoint to be encoded as UTF-8
void vui_string_put(vui_string* str, unsigned int c);


// Appends null terminator
// Returns pointer to internal buffer
// The pointer is only valid until the next call to `vui_string_put*`.
static inline char* vui_string_get(vui_string* str)
{
	if (str == NULL) return NULL;

	str->s[str->n] = 0; // null-terminate the string
	                    //  (there is room already allocated)
	return str->s;
}


void vui_string_get_replace(vui_string* str, char** s);

#ifdef __cplusplus
}
#endif

#endif
