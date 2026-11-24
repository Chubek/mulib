/** @file mulib-markup-bridge.h
 * @brief Conversions: tree <-> spans (laminar only), tree/spans -> graph,
 *        and a hybrid tree+stand-off annotations+cross-reference graph.
 * ISO C11. Define MULIB_IMPL_CDD_MARKUP_BRIDGE in one implementation TU and
 * link TREE, STREAM, SPANS and GRAPH. MULIB_IMPL_MARKUP_BRIDGE remains an alias.
 */
#ifndef MULIB_MARKUP_BRIDGE_H
#define MULIB_MARKUP_BRIDGE_H
#include "mulib-markup-stream.h"
#include "mulib-markup-spans.h"
#include "mulib-markup-graph.h"
#ifdef __cplusplus
extern "C" {
#endif
/* All conversion outputs must be zero-initialized and stay unchanged on failure.
   Text, CDATA and trivia contribute their bytes. Element spans are emitted in
   preorder, preserving the nesting of equal ranges on a round trip. */
int mulib_mkir_tree_to_spans(const mulib_mkir_tree_t *t,size_t root,mulib_mkir_spans_t *out);
/* Returns 0 if spans cross or are zero-width; no partial tree is published. */
int mulib_mkir_spans_to_tree(const mulib_mkir_spans_t *s,mulib_mkir_tree_t *out);
int mulib_mkir_tree_to_graph(const mulib_mkir_tree_t *tree,mulib_mkir_graph_t *out);
/* Span vertices precede run vertices. 'covers' edges are grouped by run, then
   by annotation ID. Each nonempty annotation-set lists the run vertex first. */
int mulib_mkir_spans_to_graph(const mulib_mkir_spans_t *s,mulib_mkir_graph_t *out);
/* As above, with the same inclusive limits as mulib_cdd_mkir_spans_runs_limited. */
int mulib_cdd_mkir_spans_to_graph_limited(const mulib_mkir_spans_t *s,
                                        size_t max_runs,size_t max_memberships,
                                        mulib_mkir_graph_t *out);
typedef struct mulib_mkir_hybrid {
 mulib_mkir_tree_t tree; mulib_mkir_spans_t spans;mulib_mkir_graph_t graph;
 size_t span_vertex_begin,run_vertex_begin;
} mulib_mkir_hybrid_t;
/* Copy the full tree, preserving node IDs, and flatten only the selected subtree. */
int mulib_mkir_hybrid_from_tree(const mulib_mkir_tree_t *source,size_t root,mulib_mkir_hybrid_t *out);
/* Rebuild graph to reflect changes. Success replaces vertex IDs and discards
   manually added graph data; failure keeps the previous index intact. */
int mulib_mkir_hybrid_index(mulib_mkir_hybrid_t *h);
/* Add arbitrary reference between two tree nodes, without constraining it to a DAG. */
int mulib_mkir_hybrid_reference(mulib_mkir_hybrid_t *h,size_t from,size_t to,const char *type);
/* Connect a tree node to a stand-off span after the last index build. */
int mulib_mkir_hybrid_anchor(mulib_mkir_hybrid_t *h,size_t node,size_t span,const char *type);
void mulib_mkir_hybrid_free(mulib_mkir_hybrid_t *h);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CDD_MARKUP_BRIDGE) || defined(MULIB_IMPL_MARKUP_BRIDGE)
#ifndef MULIB_MARKUP_BRIDGE_IMPLEMENTED
#define MULIB_MARKUP_BRIDGE_IMPLEMENTED
/* Revisit dependency gates if this is the second (implementation) inclusion. */
#include "mulib-markup-tree.h"
#include "mulib-markup-stream.h"
#include "mulib-markup-spans.h"
#include "mulib-markup-graph.h"
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_mkir_bridge_empty_tree_(const mulib_mkir_tree_t *t){
 return t&&!t->nodes&&!t->count&&!t->capacity&&!t->attrs&&!t->attr_count&&
   !t->attr_capacity&&!t->version;
}
static int mulib_mkir_bridge_empty_spans_(const mulib_mkir_spans_t *s){
 return s&&!s->text&&!s->length&&!s->capacity&&!s->items&&!s->count&&!s->item_capacity;
}
static int mulib_mkir_bridge_empty_graph_(const mulib_mkir_graph_t *g){
 return g&&!g->vertices&&!g->vertex_count&&!g->vertex_capacity&&
   !g->edges&&!g->edge_count&&!g->edge_capacity&&!g->hyperedges&&!g->hyper_count&&
   !g->hyper_capacity&&!g->properties&&!g->property_count&&!g->property_capacity;
}
int mulib_mkir_tree_to_spans(const mulib_mkir_tree_t *t,size_t root,mulib_mkir_spans_t *out){
 mulib_mkir_spans_t tmp={NULL,0,0,NULL,0,0};mulib_mkir_events_t it;mulib_mkir_event_t ev;
 size_t *ids;int r,ok=0;
 if(!mulib_mkir_bridge_empty_spans_(out)||!t||t->count>SIZE_MAX/sizeof(*ids)||
    !mulib_mkir_events_begin(t,root,&it))return 0;
 ids=(size_t*)malloc(t->count*sizeof(*ids));if(!ids)return 0;
 if(!mulib_mkir_spans_init(&tmp,"",0))goto done;
 while((r=mulib_mkir_events_next(&it,&ev))>0){
   const mulib_mkir_node_t *n=ev.data;
   if(ev.kind==MULIB_MKIR_E_ENTER){
     if(n->kind==MULIB_MKIR_ELEMENT&&
        !mulib_mkir_spans_add(&tmp,tmp.length,tmp.length,n->name,0,&ids[ev.node]))goto done;
     if(n->kind==MULIB_MKIR_TRIVIA&&n->value&&
        !mulib_mkir_spans_append(&tmp,n->value,n->value_length))goto done;
   }else if(ev.kind==MULIB_MKIR_E_TEXT){
     if(n->value&&!mulib_mkir_spans_append(&tmp,n->value,n->value_length))goto done;
   }else if(ev.kind==MULIB_MKIR_E_LEAVE){
     if(n->kind==MULIB_MKIR_ELEMENT)tmp.items[ids[ev.node]].end=tmp.length;
   }
 }
 if(r<0)goto done;
 *out=tmp;ok=1;
 done:free(ids);if(!ok)mulib_mkir_spans_free(&tmp);return ok;
}
typedef struct mulib_mkir_boundary_ {size_t at,id,other;int start;} mulib_mkir_boundary_;
static int mulib_mkir_boundary_cmp_(const void *ap,const void *bp){
 const mulib_mkir_boundary_ *a=(const mulib_mkir_boundary_*)ap,*b=(const mulib_mkir_boundary_*)bp;
 if(a->at!=b->at)return a->at<b->at?-1:1;
 if(a->start!=b->start)return a->start?1:-1;/* close before start */
 /* Open longer first; close innermost (later start) first. */
 if(a->other!=b->other)return a->other>b->other?-1:1;
 if(a->id==b->id)return 0;
 return (a->start?(a->id<b->id):(a->id>b->id))?-1:1;
}
int mulib_mkir_spans_to_tree(const mulib_mkir_spans_t *s,mulib_mkir_tree_t *out){
 mulib_mkir_boundary_ *ev=NULL;size_t *stack=NULL,*stack_span=NULL,root,pos=0,used=0;
 mulib_mkir_tree_t tmp={NULL,0,0,NULL,0,0,0};int ok=0;
 if(!mulib_mkir_bridge_empty_tree_(out)||!s||
    s->count>SIZE_MAX/(2*sizeof(*ev))||!mulib_mkir_spans_validate(s))return 0;
 for(size_t i=0;i<s->count;i++)if(s->items[i].begin==s->items[i].end)return 0;
 if(s->count){
   ev=(mulib_mkir_boundary_*)malloc(s->count*2*sizeof(*ev));
   stack=(size_t*)malloc((s->count+1)*sizeof(*stack));
   stack_span=(size_t*)malloc((s->count+1)*sizeof(*stack_span));
   if(!ev||!stack||!stack_span)goto done;
   for(size_t i=0;i<s->count;i++){
     const mulib_mkir_span_t *a=&s->items[i];
     mulib_mkir_boundary_ start={a->begin,i,a->end,1},end={a->end,i,a->begin,0};
     ev[2*i]=start;ev[2*i+1]=end;
   }
   qsort(ev,s->count*2,sizeof(*ev),mulib_mkir_boundary_cmp_);
 }
 if(!mulib_mkir_tree_add(&tmp,MULIB_MKIR_NONE,MULIB_MKIR_DOCUMENT,NULL,NULL,0,0,s->length,&root))goto done;
 if(stack)stack[0]=root;
 for(size_t i=0;i<s->count*2;i++){
   size_t at=ev[i].at,id=ev[i].id,node;
   if(at<pos||at>s->length)goto done;
   if(at>pos){
     if(!mulib_mkir_tree_add(&tmp,stack[used],MULIB_MKIR_TEXT,NULL,s->text+pos,at-pos,pos,at,&node))goto done;
     pos=at;
   }
   if(ev[i].start){
     if(used && s->items[id].end>s->items[stack_span[used]].end)goto done;
     if(!mulib_mkir_tree_add(&tmp,stack[used],MULIB_MKIR_ELEMENT,s->items[id].label,NULL,0,at,s->items[id].end,&node))goto done;
     stack[++used]=node;stack_span[used]=id;
   }else{
     if(!used||stack_span[used]!=id)goto done;
     used--;
   }
 }
 if(used)goto done;
 if(pos<s->length){size_t id;
   if(!mulib_mkir_tree_add(&tmp,root,MULIB_MKIR_TEXT,NULL,s->text+pos,s->length-pos,pos,s->length,&id))goto done;
 }
 *out=tmp;ok=1;
 done:free(ev);free(stack);free(stack_span);if(!ok)mulib_mkir_tree_free(&tmp);return ok;
}
/* The prevalidated structural links are already known to form a DAG; avoid
   per-edge reachability checks while building the temporary graph. */
static int mulib_mkir_bridge_tree_vertices_(const mulib_mkir_tree_t *t,mulib_mkir_graph_t *g){
 for(size_t i=0;i<t->count;i++)if(!mulib_mkir_graph_vertex(g,MULIB_MKIR_REF_NODE,i,NULL))return 0;
 for(size_t i=0;i<t->count;i++)for(size_t c=t->nodes[i].first_child;c!=MULIB_MKIR_NONE;c=t->nodes[c].next){
   if(!mulib_mkir_graph_edge(g,i,c,"child",NULL))return 0;
 }
 return 1;
}
int mulib_mkir_tree_to_graph(const mulib_mkir_tree_t *t,mulib_mkir_graph_t *out){
 mulib_mkir_graph_t tmp={NULL,0,0,NULL,0,0,NULL,0,0,NULL,0,0,0};
 if(!mulib_mkir_bridge_empty_graph_(out)||!mulib_mkir_tree_validate(t))return 0;
 if(!mulib_mkir_bridge_tree_vertices_(t,&tmp)){mulib_mkir_graph_free(&tmp);return 0;}
 tmp.acyclic=1;
 *out=tmp;return 1;
}
/* Runs are sinks; memberships cannot create directed cycles. */
static int mulib_mkir_bridge_span_vertices_(const mulib_mkir_spans_t *s,mulib_mkir_graph_t *g,
                                          size_t max_runs,size_t max_memberships){
 mulib_mkir_runs_t runs={NULL,0};size_t base=g->vertex_count,largest=0,*members=NULL;int ok=0;
 if(!mulib_cdd_mkir_spans_runs_limited(s,max_runs,max_memberships,&runs))return 0;
 if(s->count>SIZE_MAX-base||runs.count>SIZE_MAX-base-s->count)goto done;
 for(size_t i=0;i<s->count;i++)if(!mulib_mkir_graph_vertex(g,MULIB_MKIR_REF_SPAN,i,NULL))goto done;
 for(size_t i=0;i<runs.count;i++){
   if(!mulib_mkir_graph_vertex(g,MULIB_MKIR_REF_RUN,i,NULL))goto done;
   if(runs.items[i].count>largest)largest=runs.items[i].count;
 }
 if(largest){
   if(largest>=SIZE_MAX/sizeof(*members))goto done;
   members=(size_t*)malloc((largest+1)*sizeof(*members));if(!members)goto done;
 }
 for(size_t i=0;i<runs.count;i++){
   size_t r=base+s->count+i,n=runs.items[i].count;
   if(n){
     members[0]=r;
     for(size_t j=0;j<n;j++){
       members[j+1]=base+runs.items[i].annotations[j];
       if(!mulib_mkir_graph_edge(g,members[j+1],r,"covers",NULL))goto done;
     }
     if(!mulib_mkir_graph_hyperedge(g,members,n+1,"annotation-set",NULL))goto done;
   }
 }
 ok=1;
 done:free(members);mulib_mkir_runs_free(&runs);return ok;
}
int mulib_cdd_mkir_spans_to_graph_limited(const mulib_mkir_spans_t *s,size_t max_runs,
                                        size_t max_memberships,mulib_mkir_graph_t *out){
 mulib_mkir_graph_t tmp={NULL,0,0,NULL,0,0,NULL,0,0,NULL,0,0,0};
 if(!mulib_mkir_bridge_empty_graph_(out))return 0;
 if(!mulib_mkir_bridge_span_vertices_(s,&tmp,max_runs,max_memberships)){
   mulib_mkir_graph_free(&tmp);return 0;
 }
 tmp.acyclic=1;
 *out=tmp;return 1;
}
int mulib_mkir_spans_to_graph(const mulib_mkir_spans_t *s,mulib_mkir_graph_t *out){
 return mulib_cdd_mkir_spans_to_graph_limited(s,SIZE_MAX,SIZE_MAX,out);
}
int mulib_mkir_hybrid_index(mulib_mkir_hybrid_t *h){
 mulib_mkir_graph_t next={NULL,0,0,NULL,0,0,NULL,0,0,NULL,0,0,0};
 if(!h||!mulib_mkir_tree_validate(&h->tree))return 0;
 if(!mulib_mkir_bridge_tree_vertices_(&h->tree,&next)||
    !mulib_mkir_bridge_span_vertices_(&h->spans,&next,SIZE_MAX,SIZE_MAX)){
   mulib_mkir_graph_free(&next);return 0;
 }
 /* Keep the graph general: references need not be hierarchical. */
 mulib_mkir_graph_free(&h->graph);h->graph=next;
 h->span_vertex_begin=h->tree.count;h->run_vertex_begin=h->tree.count+h->spans.count;
 return 1;
}
/* Copy payloads through the builders; copy only validated index links after
   all nodes exist. DFS copies can renumber nodes after insertion or moves. */
static int mulib_mkir_bridge_copy_tree_(const mulib_mkir_tree_t *src,mulib_mkir_tree_t *out){
 for(size_t i=0;i<src->count;i++){
   const mulib_mkir_node_t *n=&src->nodes[i];size_t node;
   if(!mulib_mkir_tree_add(out,MULIB_MKIR_NONE,n->kind,n->name,n->value,n->value_length,
                          n->source_begin,n->source_end,&node))return 0;
 }
 for(size_t i=0;i<src->count;i++){
   const mulib_mkir_node_t *n=&src->nodes[i];mulib_mkir_node_t *m=&out->nodes[i];
   m->parent=n->parent;m->first_child=n->first_child;m->last_child=n->last_child;
   m->prev=n->prev;m->next=n->next;
 }
 for(size_t i=0;i<src->attr_count;i++){
   const mulib_mkir_attr_t *a=&src->attrs[i];
   if(!mulib_mkir_tree_attr(out,a->owner,a->key,a->value))return 0;
 }
 return 1;
}
int mulib_mkir_hybrid_from_tree(const mulib_mkir_tree_t *src,size_t root,mulib_mkir_hybrid_t *out){
 mulib_mkir_hybrid_t h={
   {NULL,0,0,NULL,0,0,0},{NULL,0,0,NULL,0,0},{NULL,0,0,NULL,0,0,NULL,0,0,NULL,0,0,0},0,0
 };
 if(!out||!mulib_mkir_bridge_empty_tree_(&out->tree)||!mulib_mkir_bridge_empty_spans_(&out->spans)||
    !mulib_mkir_bridge_empty_graph_(&out->graph)||out->span_vertex_begin||out->run_vertex_begin)return 0;
 if(!mulib_mkir_tree_to_spans(src,root,&h.spans)||!mulib_mkir_bridge_copy_tree_(src,&h.tree)||
    !mulib_mkir_hybrid_index(&h)){
   mulib_mkir_hybrid_free(&h);return 0;
 }
 *out=h;return 1;
}
static int mulib_mkir_bridge_vertex_(const mulib_mkir_graph_t *g,size_t index,
                                    mulib_mkir_ref_kind_t kind,size_t ref){
 return g->vertices&&g->vertex_count<=g->vertex_capacity&&
   g->vertex_capacity<=SIZE_MAX/sizeof(*g->vertices)&&index<g->vertex_count&&
   g->vertices[index].kind==kind&&g->vertices[index].ref==ref;
}
int mulib_mkir_hybrid_reference(mulib_mkir_hybrid_t *h,size_t from,size_t to,const char *kind){
 if(!h||from>=h->tree.count||to>=h->tree.count||from>=h->span_vertex_begin||to>=h->span_vertex_begin||
    !mulib_mkir_bridge_vertex_(&h->graph,from,MULIB_MKIR_REF_NODE,from)||
    !mulib_mkir_bridge_vertex_(&h->graph,to,MULIB_MKIR_REF_NODE,to))return 0;
 return mulib_mkir_graph_edge(&h->graph,from,to,kind,NULL);
}
int mulib_mkir_hybrid_anchor(mulib_mkir_hybrid_t *h,size_t node,size_t span,const char *type){
 if(!h||node>=h->tree.count||span>=h->spans.count||node>=h->span_vertex_begin||
    h->run_vertex_begin<h->span_vertex_begin||span>=h->run_vertex_begin-h->span_vertex_begin||
    !mulib_mkir_bridge_vertex_(&h->graph,node,MULIB_MKIR_REF_NODE,node)||
    !mulib_mkir_bridge_vertex_(&h->graph,h->span_vertex_begin+span,MULIB_MKIR_REF_SPAN,span))return 0;
 return mulib_mkir_graph_edge(&h->graph,node,h->span_vertex_begin+span,type,NULL);
}
void mulib_mkir_hybrid_free(mulib_mkir_hybrid_t *h){
 if(!h)return;
 mulib_mkir_tree_free(&h->tree);mulib_mkir_spans_free(&h->spans);
 mulib_mkir_graph_free(&h->graph);h->span_vertex_begin=0;h->run_vertex_begin=0;
}
#ifdef __cplusplus
}
#endif
#endif
#endif
