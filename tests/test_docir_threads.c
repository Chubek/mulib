#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCIR
#include "mulib-docir.h"
#include <assert.h>
#if !defined(__STDC_NO_THREADS__) && !defined(__STDC_NO_ATOMICS__) && !defined(_MSC_VER)
#include <threads.h>
static int worker_(void *argument) {
    (void)argument;
    for(size_t iteration=0;iteration<50;iteration++) {
        mulib_docir_node_t *root=mulib_docir_new(mulib_docir_ROOT,NULL);assert(root);
        for(size_t i=0;i<32;i++) {
            mulib_docir_node_t *text=mulib_docir_new(mulib_docir_TEXT,"Concurrent document text");assert(text);
            assert(mulib_docir_append(root,text));
        }
        mulib_docir_snapshot_t snapshot={0};assert(mulib_docir_snapshot_make(root,NULL,&snapshot,NULL)&&snapshot.count==33);
        mulib_docir_node_t *copy=mulib_docir_build(&snapshot,mulib_docir_NORMALIZE_TEXT,NULL);assert(copy&&copy->count==1);
        mulib_docir_destroy(root);assert(mulib_docir_snapshot_validate(&snapshot,0,NULL));
        mulib_docir_snapshot_destroy(&snapshot);mulib_docir_destroy(copy);
    }
    return 0;
}
int main(void) {
    thrd_t threads[4];size_t started=0;
    for(;started<4;started++)if(thrd_create(threads+started,worker_,NULL)!=thrd_success)break;
    for(size_t i=0;i<started;i++){int result=1;assert(thrd_join(threads[i],&result)==thrd_success&&!result);}
    assert(started==4);return 0;
}
#else
int main(void){puts("SKIP: concurrent DocIR test requires C11 threads/atomics.");return 0;}
#endif
