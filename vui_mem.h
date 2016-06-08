#ifndef VUI_MEM_H
#define VUI_MEM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>


#if __GNUC__
#define typeof __typeof__
#define VUI_MEM_HAS_TYPEOF
#endif

static inline void* vui_malloc(size_t size)
{
	void* ptr = malloc(size);

	if (ptr == NULL)
	{
		abort();
	}

	return ptr;
}

#define vui_new(type) ((type*)vui_malloc(sizeof(type)))

#define vui_new_array(type, len) ((type*)vui_malloc(sizeof(type)*len))

static inline void* vui_realloc_(void* ptr, size_t size)
{
	void* ptr2 = realloc(ptr, size);

	if (ptr2 == NULL)
	{
		abort();
	}

	return ptr2;
}

#ifdef VUI_MEM_HAS_TYPEOF
#define vui_realloc(ptr, size) ((typeof(ptr))vui_realloc_(ptr, size))
#else
#define vui_realloc(ptr, size) vui_realloc_(ptr, size)
#endif

#define vui_resize_array(ptr, type, size) ((type*)vui_realloc_((type*)(ptr), size*sizeof(type)))

#define vui_free(ptr) do { void** _ptr = (void**)&(ptr); free(*_ptr); *_ptr = NULL; } while (0);

#ifdef __cplusplus
}
#endif

#endif
