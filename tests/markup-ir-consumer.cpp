#include "mulib-markup-bridge.h"
#include <cassert>
#include <cstring>
int main(){
 mulib_mkir_tree_t t{};mulib_mkir_spans_t spans{};mulib_mkir_graph_t graph{};
 mulib_mkir_cst_t snapshot{};mulib_mkir_runs_t runs{};
 const char *xml="<div>Text</div>";size_t bad=0;
 assert(mulib_mkir_parse(xml,std::strlen(xml),32,&t,&bad));
 assert(mulib_mkir_tree_validate(&t));
 assert(mulib_mkir_tree_to_spans(&t,0,&spans));
 assert(spans.length==4 && std::memcmp(spans.text,"Text",4)==0);
 assert(mulib_mkir_spans_validate(&spans));
 assert(mulib_cdd_mkir_spans_runs_limited(&spans,1,1,&runs) && runs.count==1);
 assert(mulib_mkir_cst_parse(xml,std::strlen(xml),32,&snapshot,&bad));
 const char *token=nullptr;size_t token_length=0;
 assert(mulib_cdd_mkir_cst_token_bytes(&snapshot,0,&token,&token_length));
 assert(token_length==5 && std::memcmp(token,"<div>",token_length)==0);
 assert(mulib_mkir_tree_to_graph(&t,&graph) && mulib_mkir_graph_validate(&graph));
 assert(mulib_mkir_graph_path(&graph,0,2)==1);
 mulib_mkir_cst_free(&snapshot);mulib_mkir_runs_free(&runs);mulib_mkir_graph_free(&graph);
 mulib_mkir_spans_free(&spans);mulib_mkir_tree_free(&t);
 return 0;
}
