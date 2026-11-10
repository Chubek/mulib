/* Fail each allocation in turn and check ownership/transaction boundaries. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static size_t fail_at=SIZE_MAX,calls,live;
static int fail(void) {return calls++==fail_at;}
static void *test_malloc(size_t n) {if(fail())return NULL;void *p=malloc(n);if(p)live++;return p;}
static void *test_calloc(size_t n,size_t z) {if(fail())return NULL;void *p=calloc(n,z);if(p)live++;return p;}
static void *test_realloc(void *old,size_t n) {
    if(fail())return NULL;
    int was_null=old==NULL;void *p=realloc(old,n);if(p&&was_null)live++;return p;
}
static void test_free(void *p) {if(p){assert(live);live--;free(p);}}
#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define free test_free
#define MULIB_IMPL_BYTECODE_SERIALIZE
#define MULIB_IMPL_BYTECODE_VERIFY
#define MULIB_IMPL_VM_MODULE
#define MULIB_IMPL_COMPILER_CACHE
#define MULIB_IMPL_SOURCE_MANAGER
#define MULIB_IMPL_GC
#define MULIB_IMPL_GRAPH
#define MULIB_IMPL_SORT
#define MULIB_IMPL_SYMBOLTABLE
#define MULIB_IMPL_PARSER_PRATT
#define MULIB_IMPL_LEXER
#define MULIB_IMPL_LEXICAL_TOKEN
#include "mulib-bytecode-serialize.h"
#include "mulib-vm-module.h"
#include "mulib-compiler-cache.h"
#include "mulib-source-manager.h"
#include "mulib-gc.h"
#include "mulib-graph.h"
#include "mulib-sort.h"
#include "mulib-symboltable.h"
#include "mulib-parser-pratt.h"
#undef malloc
#undef calloc
#undef realloc
#undef free

static void inject(size_t index) {fail_at=index;calls=0;}
static void reset(void) {fail_at=SIZE_MAX;calls=0;}
static int compare(const void *a,const void *b) {int x=*(const int *)a,y=*(const int *)b;return (x>y)-(x<y);}
static void exercise(size_t failure) {
    mulib_compiler_cache_t cache={0};const void *data;size_t length;
    reset();assert(mulib_compiler_cache_put(&cache,1,"old",3));
    inject(failure);int ok=mulib_compiler_cache_put(&cache,1,"new",3);
    assert(mulib_compiler_cache_get(&cache,1,&data,&length)&&length==3);
    assert(!memcmp(data,ok?"new":"old",3));mulib_compiler_cache_free(&cache);
    reset();mulib_source_manager_t source={0};assert(mulib_source_manager_set(&source,"old",3));
    inject(failure);ok=mulib_source_manager_set(&source,"new\n",4);
    assert(source.length==(ok?4u:3u)&&!memcmp(source.text,ok?"new":"old",3));
    mulib_source_manager_free(&source);
    mulib_bc_instruction_t code[]={{MULIB_BC_PUSH,42},{MULIB_BC_HALT,0}},*decoded=NULL;
    unsigned char *bytes=NULL;length=77;
    inject(failure);ok=mulib_bytecode_encode(code,2,&bytes,&length);
    if(!ok)assert(!bytes&&length==77);
    else {
        size_t count=77;inject(failure);ok=mulib_bytecode_decode(bytes,length,&decoded,&count);
        if(!ok)assert(!decoded&&count==77);else assert(count==2&&decoded[0].operand==42);
        test_free(decoded);test_free(bytes);
    }
    mulib_vm_modules_t modules={0};inject(failure);
    ok=mulib_vm_module_add(&modules,"main",code,2,0);
    assert(modules.count==(ok?1u:0u));if(!ok)assert(!modules.modules);
    mulib_vm_modules_free(&modules);
    reset();mulib_gc_heap_t heap={0};mulib_gc_object_t *root=mulib_gc_alloc(&heap,1);assert(root);
    inject(failure);mulib_gc_object_t *child=mulib_gc_alloc(&heap,1);
    assert(heap.count==(child?2u:1u));
    size_t reclaimed=77;inject(failure);ok=mulib_gc_collect(&heap,&root,1,&reclaimed);
    if(!ok)assert(reclaimed==77&&heap.count==(child?2u:1u));else assert(heap.count==1);
    mulib_gc_heap_free(&heap);
    reset();mulib_graph_t graph={0},reverse={0};assert(mulib_graph_init(&graph,2));
    assert(mulib_graph_edge(&graph,0,1,1));size_t order[2]={77,77};
    inject(failure);ok=mulib_graph_toposort(&graph,order,2);
    if(!ok)assert(order[0]==77&&order[1]==77);else assert(order[0]==0&&order[1]==1);
    inject(failure);ok=mulib_graph_reverse(&graph,&reverse);
    if(!ok)assert(!reverse.edges);else assert(reverse.edges[2]);
    mulib_graph_free(&graph);mulib_graph_free(&reverse);
    int values[]={3,1,2};inject(failure);ok=mulib_sort_stable(values,3,sizeof(int),compare);
    if(!ok)assert(values[0]==3&&values[1]==1&&values[2]==2);else assert(values[0]==1&&values[2]==3);
    mulib_symboltable_t symbols={0};inject(failure);ok=mulib_symboltable_define(&symbols,"x",42);
    assert(symbols.count==(ok?1u:0u));mulib_symboltable_free(&symbols);
    mulib_parser_pratt_t tree={0};inject(failure);ok=mulib_parser_pratt_parse("1+2*3",5,&tree,NULL);
    if(!ok)assert(!tree.nodes&&!tree.count);else assert(tree.count==5);
    mulib_parser_pratt_free(&tree);assert(live==0);reset();
}
int main(void) {
    for(size_t failure=0;failure<8;failure++)exercise(failure);
    exercise(SIZE_MAX);return 0;
}
