/* MuLIB compiler tools: Checked aligned stack-frame slot placement with final frame alignment.
 * ISO C11 single-header module. Define MULIB_IMPL_FRAME_LAYOUT in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_FRAME_LAYOUT_H
#define MULIB_FRAME_LAYOUT_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {size_t size,alignment,offset;
} mulib_frame_slot_t;
typedef struct {mulib_frame_slot_t *slots;
size_t count,capacity,used;
} mulib_frame_layout_t;
int mulib_frame_layout_add(mulib_frame_layout_t *f,size_t size,size_t alignment,size_t *id);
int mulib_frame_layout_finish(const mulib_frame_layout_t *f,size_t frame_alignment,size_t *total_size);
void mulib_frame_layout_free(mulib_frame_layout_t *f);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FRAME_LAYOUT_H */
#if defined(MULIB_IMPL_FRAME_LAYOUT) && !defined(MULIB_FRAME_LAYOUT_IMPLEMENTED)
#define MULIB_FRAME_LAYOUT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_frame_align_(size_t n,size_t a,size_t *result){if(!a||(a&(a-1))||n>SIZE_MAX-(a-1))return 0;
*result=(n+a-1)&~(a-1);
return 1;
}
int mulib_frame_layout_add(mulib_frame_layout_t *f,size_t size,size_t align,size_t *id){if(!f||!id||!size)return 0;
size_t at;
 if(!mulib_frame_align_(f->used,align,&at)||at>SIZE_MAX-size)return 0;
 if(f->count==f->capacity){size_t cap=f->capacity?f->capacity*2:8;
if(cap<f->capacity||cap>SIZE_MAX/sizeof(*f->slots))return 0;
void *p=realloc(f->slots,cap*sizeof(*f->slots));
if(!p)return 0;
f->slots=(mulib_frame_slot_t*)p;
f->capacity=cap;
}
 *id=f->count;
f->slots[f->count++]=(mulib_frame_slot_t){size,align,at};
f->used=at+size;
return 1;
}
int mulib_frame_layout_finish(const mulib_frame_layout_t *f,size_t align,size_t *total){return f&&total&&mulib_frame_align_(f->used,align,total);
}
void mulib_frame_layout_free(mulib_frame_layout_t *f){if(f){free(f->slots);
memset(f,0,sizeof(*f));
}}
#ifdef __cplusplus
}
#endif
#endif
