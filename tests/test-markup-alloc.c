/* Exercise allocation failures across the CST -> tree -> spans -> hybrid graph
 * pipeline, without changing any production allocator or diagnostic API. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t calls, fail_at, live;
static int exhausted(void) { return ++calls==fail_at; }
static void *test_malloc(size_t n) {
  void *p=exhausted()?NULL:malloc(n);
  if(p)live++;
  return p;
}
static void *test_calloc(size_t n,size_t width) {
  void *p=exhausted()?NULL:calloc(n,width);
  if(p)live++;
  return p;
}
static void test_free(void *p) {
  if(p){assert(live);live--;free(p);}
}
static void *test_realloc(void *p,size_t n) {
  int fresh=!p;
  void *q;
  if(exhausted())return NULL;
  if(!n){test_free(p);return NULL;}
  q=realloc(p,n);
  if(q&&fresh)live++;
  return q;
}

#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define free test_free
#define MULIB_IMPL_CDD_MARKUP_TREE
#define MULIB_IMPL_CDD_MARKUP_STREAM
#define MULIB_IMPL_CDD_MARKUP_SPANS
#define MULIB_IMPL_CDD_MARKUP_GRAPH
#define MULIB_IMPL_CDD_MARKUP_BRIDGE
#include "mulib-markup-bridge.h"
#undef malloc
#undef calloc
#undef realloc
#undef free

static const char input[]="<p id='x'>alpha <b>beta</b> gamma</p>";

static size_t exercise(unsigned op,size_t failure) {
  mulib_mkir_cst_t cst={0};
  mulib_mkir_hybrid_t hybrid={0};
  unsigned char saved[sizeof hybrid];
  size_t offset=0,n;
  int ok;
  assert(!live);
  if(op>=1)assert(mulib_mkir_cst_parse(input,sizeof input-1,16,&cst,&offset));
  if(op>=2) {
    assert(mulib_mkir_hybrid_from_tree(&cst.tree,0,&hybrid));
    assert(mulib_mkir_hybrid_reference(&hybrid,1,0,"xref"));
    assert(mulib_mkir_spans_add(&hybrid.spans,3,9,"selection",0,NULL));
  }
  memcpy(saved,&hybrid,sizeof saved);
  calls=0;fail_at=failure;
  if(op==0)ok=mulib_mkir_cst_parse(input,sizeof input-1,16,&cst,&offset);
  else if(op==1)ok=mulib_mkir_hybrid_from_tree(&cst.tree,0,&hybrid);
  else ok=mulib_mkir_hybrid_index(&hybrid);
  n=calls;fail_at=0;
  if(!failure)assert(ok);
  if(!ok) {
    if(op==0)assert(!cst.source && !cst.tokens && !cst.tree.nodes && offset<=sizeof input-1);
    else assert(!memcmp(saved,&hybrid,sizeof saved));
  } else if(op==0) {
    assert(cst.length==sizeof input-1 && !memcmp(cst.source,input,cst.length));
    assert(mulib_mkir_tree_validate(&cst.tree));
  } else {
    assert(mulib_mkir_graph_validate(&hybrid.graph) && mulib_mkir_spans_validate(&hybrid.spans));
    assert(hybrid.spans.length==16 && !memcmp(hybrid.spans.text,"alpha beta gamma",16));
    assert(hybrid.spans.count==(op==1?2:3));
  }
  mulib_mkir_hybrid_free(&hybrid);
  mulib_mkir_cst_free(&cst);
  assert(!live);
  return n;
}

int main(void) {
  size_t total=0;
  for(unsigned op=0;op<3;op++) {
    size_t count=exercise(op,0);
    assert(count);
    for(size_t i=1;i<=count;i++)(void)exercise(op,i);
    total+=count;
  }
  printf("Markup IR allocation injection: %zu failure points\n",total);
  return 0;
}
