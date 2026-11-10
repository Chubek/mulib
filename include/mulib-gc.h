/* MuLIB compiler tools: Owned stop-the-world tracing mark-and-sweep heap with explicit object reference slots.
 * ISO C11 single-header module. Define MULIB_IMPL_GC in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_GC_H
#define MULIB_GC_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_gc_object mulib_gc_object_t;
struct mulib_gc_object {mulib_gc_object_t *next,**children;
size_t slots;
unsigned marked;
};
typedef struct {mulib_gc_object_t *head;
size_t count;
} mulib_gc_heap_t;
mulib_gc_object_t *mulib_gc_alloc(mulib_gc_heap_t *heap,size_t slots);
int mulib_gc_set(mulib_gc_heap_t *heap,mulib_gc_object_t *parent,size_t slot,mulib_gc_object_t *child);
/* Roots borrowed; validates they belong to this heap; 1 success. */
int mulib_gc_collect(mulib_gc_heap_t *heap,mulib_gc_object_t *const *roots,size_t count,size_t *reclaimed);
void mulib_gc_heap_free(mulib_gc_heap_t *heap);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_GC_H */
#if defined(MULIB_IMPL_GC) && !defined(MULIB_GC_IMPLEMENTED)
#define MULIB_GC_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_gc_member_(const mulib_gc_heap_t *h,const mulib_gc_object_t *o){for(const mulib_gc_object_t *x=h->head;x;x=x->next)if(x==o)return 1;
return 0;
}
mulib_gc_object_t *mulib_gc_alloc(mulib_gc_heap_t *h,size_t slots){if(!h||h->count==SIZE_MAX||slots>SIZE_MAX/sizeof(mulib_gc_object_t*))return NULL;
mulib_gc_object_t *o=(mulib_gc_object_t*)calloc(1,sizeof(*o));
if(!o)return NULL;
if(slots){o->children=(mulib_gc_object_t**)calloc(slots,sizeof(*o->children));
if(!o->children){free(o);
return NULL;
}}o->slots=slots;
o->next=h->head;
h->head=o;
h->count++;
return o;
}
int mulib_gc_set(mulib_gc_heap_t *h,mulib_gc_object_t *parent,size_t slot,mulib_gc_object_t *child){if(!h||!parent||!mulib_gc_member_(h,parent)||slot>=parent->slots||(child&&!mulib_gc_member_(h,child)))return 0;
parent->children[slot]=child;
return 1;
}
int mulib_gc_collect(mulib_gc_heap_t *h,mulib_gc_object_t *const *roots,size_t count,size_t *reclaimed){
 if(!h||(!roots&&count)||!reclaimed||h->count>SIZE_MAX/sizeof(mulib_gc_object_t*))return 0;
 for(size_t i=0;i<count;i++)if(roots[i]&&!mulib_gc_member_(h,roots[i]))return 0;
 for(mulib_gc_object_t *p=h->head;p;p=p->next)for(size_t j=0;j<p->slots;j++)if(p->children[j]&&!mulib_gc_member_(h,p->children[j]))return 0;
 mulib_gc_object_t **stack=(mulib_gc_object_t**)malloc((h->count?h->count:1)*sizeof(*stack));
if(!stack)return 0;
 for(mulib_gc_object_t *p=h->head;p;p=p->next)p->marked=0;
size_t top=0;
 for(size_t i=0;i<count;i++)if(roots[i]&&!roots[i]->marked){roots[i]->marked=1;
stack[top++]=roots[i];
}
 while(top){mulib_gc_object_t *p=stack[--top];
for(size_t i=0;i<p->slots;i++){mulib_gc_object_t *next=p->children[i];
if(next&&!next->marked){next->marked=1;
stack[top++]=next;
}}}
 free(stack);
size_t n=0;
mulib_gc_object_t **p=&h->head;
while(*p){mulib_gc_object_t *o=*p;
if(o->marked){p=&o->next;
continue;
}*p=o->next;
free(o->children);
free(o);
h->count--;
n++;
}
 *reclaimed=n;
return 1;
}
void mulib_gc_heap_free(mulib_gc_heap_t *h){if(!h)return;
mulib_gc_object_t *p=h->head;
while(p){mulib_gc_object_t *next=p->next;
free(p->children);
free(p);
p=next;
}memset(h,0,sizeof(*h));
}
#ifdef __cplusplus
}
#endif
#endif
