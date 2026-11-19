/* Exercise byte input plus structured stand-off ranges independently of the
 * parser, so malformed markup still reaches the graph and bridge kernels. */
#define MULIB_IMPL_CDD_MARKUP_TREE
#define MULIB_IMPL_CDD_MARKUP_STREAM
#define MULIB_IMPL_CDD_MARKUP_SPANS
#define MULIB_IMPL_CDD_MARKUP_GRAPH
#define MULIB_IMPL_CDD_MARKUP_BRIDGE
#include "mulib-markup-bridge.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void exercise_ranges(const uint8_t *data,size_t size) {
  mulib_mkir_spans_t s={0},flat={0};
  mulib_mkir_runs_t runs={0};
  mulib_mkir_graph_t g={0};
  mulib_mkir_tree_t t={0};
  size_t end=0,n=size<64?size:64,order[64],memberships=0;
  assert(mulib_mkir_spans_init(&s,data,n));
  for(size_t i=0;i+1<n && i<24;i+=2) {
    size_t a=data[i]%(n+1),b=data[i+1]%(n+1);
    assert(mulib_mkir_spans_add(&s,a<b?a:b,a>b?a:b,"span",0,NULL));
  }
  assert(mulib_mkir_spans_validate(&s) && mulib_mkir_spans_runs(&s,&runs));
  for(size_t i=0;i<runs.count;i++) {
    const mulib_mkir_run_t *r=&runs.items[i];
    size_t count=0;
    assert(r->begin==end && r->end>r->begin && r->end<=n);
    end=r->end;
    for(size_t j=0;j<s.count;j++) {
      const mulib_mkir_span_t *a=&s.items[j];
      if(a->begin<=r->begin && a->end>=r->end) {
        assert(count<r->count && r->annotations[count]==j);
        count++;
      } else assert(a->begin>=r->end || a->end<=r->begin || a->begin==a->end);
    }
    assert(count==r->count);
    memberships+=count;
  }
  assert(end==n);
  if(memberships) {
    assert(!mulib_cdd_mkir_spans_to_graph_limited(&s,runs.count,memberships-1,&g));
    assert(!g.vertices && !g.vertex_count);
  }
  assert(mulib_cdd_mkir_spans_to_graph_limited(&s,runs.count,memberships,&g) &&
         mulib_mkir_graph_validate(&g));
  assert(mulib_mkir_graph_toposort(&g,order,sizeof order/sizeof *order));
  if(mulib_mkir_spans_to_tree(&s,&t)) {
    assert(mulib_mkir_tree_validate(&t) && mulib_mkir_tree_to_spans(&t,0,&flat));
    assert(flat.length==s.length && !memcmp(flat.text,s.text,s.length) && flat.count==s.count);
  }
  mulib_mkir_spans_free(&flat);
  mulib_mkir_tree_free(&t);
  mulib_mkir_graph_free(&g);
  mulib_mkir_runs_free(&runs);
  mulib_mkir_spans_free(&s);
}

static void exercise_graph(const uint8_t *data,size_t size) {
  mulib_mkir_graph_t g={0};
  unsigned arcs[7][7]={{0}},path[7][7];
  size_t n,order[7],before[7];
  unsigned used=0;
  if(!size)return;
  n=1+data[0]%7;
  for(size_t i=0;i<n;i++)
    assert(mulib_mkir_graph_vertex(&g,MULIB_MKIR_REF_FREE,i,NULL));
  for(size_t i=1;i<size && i<17;i++) {
    size_t from=(data[i]&15u)%n,to=(data[i]>>4)%n;
    arcs[from][to]=1;
    assert(mulib_mkir_graph_edge(&g,from,to,"arc",NULL));
  }
  memcpy(path,arcs,sizeof path);
  for(size_t i=0;i<n;i++)path[i][i]=1;
  for(size_t k=0;k<n;k++)for(size_t i=0;i<n;i++)for(size_t j=0;j<n;j++)
    path[i][j]|=path[i][k]&path[k][j];
  assert(mulib_mkir_graph_validate(&g));
  for(size_t i=0;i<n;i++)for(size_t j=0;j<n;j++)
    assert(mulib_mkir_graph_path(&g,i,j)==(int)path[i][j]);
  for(size_t i=0;i<n;i++)order[i]=before[i]=SIZE_MAX;
  int sorted=mulib_mkir_graph_toposort(&g,order,n);
  for(size_t k=0;k<n;k++) {
    size_t candidate=n;
    for(size_t v=0;v<n;v++)if(!(used&(1u<<v))) {
      size_t incoming=0;
      for(size_t w=0;w<n;w++)
        if(!(used&(1u<<w)))incoming+=arcs[w][v];
      if(!incoming){candidate=v;break;}
    }
    if(candidate==n) {
      assert(!sorted && !memcmp(before,order,n*sizeof *order));
      break;
    }
    used|=1u<<candidate;
    if(sorted)assert(order[k]==candidate);
  }
  assert(sorted==(used==(1u<<n)-1));
  /* Validation additionally applies the acyclic invariant when requested. */
  g.acyclic=1;
  assert(mulib_mkir_graph_validate(&g)==sorted);
  mulib_mkir_graph_free(&g);
}

int LLVMFuzzerTestOneInput(const uint8_t *data,size_t size) {
  mulib_mkir_cst_t cst={0};
  size_t bad=0;
  if(size>4096)return 0;
  if(mulib_mkir_cst_parse((const char*)data,size,64,&cst,&bad)) {
    size_t end=0;
    const mulib_mkir_node_t *nodes=cst.tree.nodes;
    char *sxml=mulib_mkir_tree_sxml(&cst.tree,0);
    assert(sxml && nodes && mulib_mkir_tree_validate(&cst.tree));
    free(sxml);
    assert(cst.length==size && !memcmp(cst.source,data,size));
    for(size_t i=0;i<cst.count;i++) {
      const char *raw=NULL;size_t length=0;
      assert(cst.tokens[i].begin==end && cst.tokens[i].end>end && cst.tokens[i].end<=size);
      assert(mulib_cdd_mkir_cst_token_bytes(&cst,i,&raw,&length));
      assert(length==cst.tokens[i].end-end && !memcmp(raw,data+end,length));
      end=cst.tokens[i].end;
    }
    assert(end==size);
    mulib_mkir_cst_free(&cst);
  } else assert(bad<=size);
  exercise_ranges(data,size);
  exercise_graph(data,size);
  return 0;
}
