/* Standalone pipeline: XML-like markup -> CST/tree -> spans -> graph/hybrid. */
#define MULIB_IMPL_CDD_MARKUP_TREE
#define MULIB_IMPL_CDD_MARKUP_STREAM
#define MULIB_IMPL_CDD_MARKUP_SPANS
#define MULIB_IMPL_CDD_MARKUP_GRAPH
#define MULIB_IMPL_CDD_MARKUP_BRIDGE
#include "mulib-markup-bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
 const char *input="<p id=\"intro\">Hello <em>world</em>!</p>";
 mulib_mkir_cst_t cst={0};mulib_mkir_hybrid_t h={0};size_t offset=0;
 if(!mulib_mkir_cst_parse(input,strlen(input),128,&cst,&offset)){
   fprintf(stderr,"parse error at byte %zu\n",offset);return 1;
 }
 char *sxml=mulib_mkir_tree_sxml(&cst.tree,0);
 if(!sxml||!mulib_mkir_hybrid_from_tree(&cst.tree,0,&h)){free(sxml);mulib_mkir_cst_free(&cst);return 1;}
 puts(sxml);
  fputs("Text: ",stdout);
  if(fwrite(h.spans.text,1,h.spans.length,stdout)!=h.spans.length){
    free(sxml);mulib_mkir_hybrid_free(&h);mulib_mkir_cst_free(&cst);return 1;
  }
  putchar('\n');
 printf("Nodes: %zu, ranges: %zu, graph vertices: %zu, hyperedges: %zu\n",
   h.tree.count,h.spans.count,h.graph.vertex_count,h.graph.hyper_count);
 /* Add an overlapping stand-off annotation; no need to rewrite the tree. */
 if(!mulib_mkir_spans_add(&h.spans,3,9,"selection",0,NULL)||!mulib_mkir_hybrid_index(&h)){
   free(sxml);mulib_mkir_hybrid_free(&h);mulib_mkir_cst_free(&cst);return 1;
 }
 printf("With overlapping selection: %zu ranges, %zu hyperedges\n",h.spans.count,h.graph.hyper_count);
 free(sxml);mulib_mkir_hybrid_free(&h);mulib_mkir_cst_free(&cst);return 0;
}
