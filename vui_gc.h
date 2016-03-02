#ifndef VUI_GC_H
#define VUI_GC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "vui_stack.h"

typedef enum vui_gc_dtor_mode
{
	VUI_GC_DTOR_MARK,
	VUI_GC_DTOR_SWEEP,
	VUI_GC_DTOR_DESCRIBE,
} vui_gc_dtor_mode;

typedef void (*vui_gc_dtor)(void* obj, vui_gc_dtor_mode);

typedef struct vui_gc_header
{
	struct vui_gc_header* gc_next;
	int gc_gen;

	size_t root;

	vui_gc_dtor dtor;
} vui_gc_header;

extern size_t vui_gc_nlive;

#define vui_gc_register(obj, dtor) vui_gc_register_header(&(*obj).gc, (vui_gc_dtor)dtor)
void vui_gc_register_header(vui_gc_header* obj, vui_gc_dtor dtor);

void vui_gc_run();

#define vui_gc_mark(obj) vui_gc_mark_header(&(*obj).gc);
void vui_gc_mark_header(vui_gc_header* obj);

#define vui_gc_incr(obj) vui_gc_incr_header(&(*obj).gc);
void vui_gc_incr_header(vui_gc_header* obj);
#define vui_gc_decr(obj) vui_gc_decr_header(&(*obj).gc);
void vui_gc_decr_header(vui_gc_header* obj);

#ifdef __cplusplus
}
#endif

#endif
