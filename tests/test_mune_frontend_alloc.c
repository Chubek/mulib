#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static size_t calls, fail_at, live;
static void *test_malloc(size_t n)
{
    void *p;
    if (++calls==fail_at) return NULL;
    p=malloc(n); if(p) ++live; return p;
}
static void *test_calloc(size_t count,size_t size)
{
    void *p;
    assert(!size || count<=SIZE_MAX/size);
    p=test_malloc(count*size); if(p) memset(p,0,count*size); return p;
}
static void *test_realloc(void *old,size_t size)
{
    void *p;
    int existed=old!=NULL;
    assert(size);
    if (++calls==fail_at) return NULL;
    p=realloc(old,size); if(p && !existed) ++live; return p;
}
static void test_free(void *p)
{
    if(p) { assert(live); --live; } free(p);
}
#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define free test_free
#define MULIB_IMPL_MUNE
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_GLOB
#include "mulib-mune.h"
#undef malloc
#undef calloc
#undef realloc
#undef free

int main(void)
{
    const char source[]="local x<const> = 1; local function f(a,...) return x,a,... end; ::again:: if x then goto again end; return f(2)";
    mulib_mune_error_t e;
    mulib_mune_chunk_t *chunk;
    size_t allocations, k;
    chunk=mulib_mune_parse(source,sizeof(source)-1,NULL,&e); assert(chunk);
    allocations=calls;
    mulib_mune_chunk_destroy(chunk); assert(!live);
    for(k=1;k<=allocations;++k) {
        calls=0; fail_at=k;
        chunk=mulib_mune_parse(source,sizeof(source)-1,NULL,&e);
        assert(!chunk && e.code==MULIB_MUNE_MEMORY);
        assert(!live);
    }
    return 0;
}
