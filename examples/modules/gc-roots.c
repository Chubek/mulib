#define MULIB_IMPL_GC
#define MULIB_IMPL_GC_ROOTS
#include "mulib-gc-roots.h"
#include <assert.h>
int main(void) {
    mulib_gc_heap_t heap={0};mulib_gc_roots_t roots={0};size_t reclaimed;
    mulib_gc_object_t *object=mulib_gc_alloc(&heap,0);assert(object);
    assert(mulib_gc_roots_add(&roots,object));
    assert(mulib_gc_roots_collect(&heap,&roots,&reclaimed)&&reclaimed==0);
    assert(mulib_gc_roots_remove(&roots,object));
    assert(mulib_gc_roots_collect(&heap,&roots,&reclaimed)&&reclaimed==1);
    mulib_gc_roots_free(&roots);mulib_gc_heap_free(&heap);return 0;
}
