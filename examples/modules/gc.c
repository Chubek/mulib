#define MULIB_IMPL_GC
#include "mulib-gc.h"
#include <assert.h>
int main(void) {
    mulib_gc_heap_t heap={0};size_t reclaimed;
    mulib_gc_object_t *root=mulib_gc_alloc(&heap,1),*child=mulib_gc_alloc(&heap,0);
    assert(root&&child&&mulib_gc_set(&heap,root,0,child));
    assert(mulib_gc_collect(&heap,&root,1,&reclaimed)&&reclaimed==0);
    assert(mulib_gc_collect(&heap,NULL,0,&reclaimed)&&reclaimed==2);
    mulib_gc_heap_free(&heap);return 0;
}
