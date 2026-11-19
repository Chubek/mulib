/** @file mulib-markup-graph.h
 * @brief Sparse ordered property graph, acyclic mode (GODDAG-style), hyperedges.
 * ISO C11; define MULIB_IMPL_CDD_MARKUP_GRAPH in one implementation TU.
 * MULIB_IMPL_MARKUP_GRAPH remains an alias.
 * Vertices are stable IDs. Edges may have multiple parents; ordered insertion
 * is preserved. This is a generic graph substrate, not full GODDAG/TAG syntax.
 */
#ifndef MULIB_MARKUP_GRAPH_H
#define MULIB_MARKUP_GRAPH_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_mkir_ref_kind {MULIB_MKIR_REF_FREE=0,MULIB_MKIR_REF_NODE=1,
 MULIB_MKIR_REF_SPAN=2,MULIB_MKIR_REF_RUN=3} mulib_mkir_ref_kind_t;
typedef struct mulib_mkir_vertex {mulib_mkir_ref_kind_t kind;size_t ref;} mulib_mkir_vertex_t;
typedef struct mulib_mkir_edge {size_t from,to;char *type;} mulib_mkir_edge_t;
typedef struct mulib_mkir_hyperedge {size_t *members,count;char *type;} mulib_mkir_hyperedge_t;
typedef enum mulib_mkir_property_owner {MULIB_MKIR_P_VERTEX=1,MULIB_MKIR_P_EDGE=2,
 MULIB_MKIR_P_HYPEREDGE=3} mulib_mkir_property_owner_t;
typedef struct mulib_mkir_property {
 mulib_mkir_property_owner_t owner;size_t id;char *key,*value;
} mulib_mkir_property_t;
typedef struct mulib_mkir_graph {
 mulib_mkir_vertex_t *vertices;size_t vertex_count,vertex_capacity;
 mulib_mkir_edge_t *edges;size_t edge_count,edge_capacity;
 mulib_mkir_hyperedge_t *hyperedges;size_t hyper_count,hyper_capacity;
 mulib_mkir_property_t *properties;size_t property_count,property_capacity;
 int acyclic;
} mulib_mkir_graph_t;
/* Initialize with zeros; set acyclic=1 on an empty graph to reject directed
   cycles upon insertion. Hyperedge membership is ordered and not directional. */
int mulib_mkir_graph_vertex(mulib_mkir_graph_t *g,mulib_mkir_ref_kind_t kind,size_t ref,size_t *id);
int mulib_mkir_graph_edge(mulib_mkir_graph_t *g,size_t from,size_t to,const char *type,size_t *id);
int mulib_mkir_graph_hyperedge(mulib_mkir_graph_t *g,const size_t *members,size_t count,const char *type,size_t *id);
int mulib_mkir_graph_property(mulib_mkir_graph_t *g,mulib_mkir_property_owner_t owner,size_t id,const char *key,const char *value);
const char *mulib_mkir_graph_get_property(const mulib_mkir_graph_t *g,mulib_mkir_property_owner_t owner,size_t id,const char *key);
/* Directed edge reachability (including a zero-edge path from a vertex to
   itself). Hyperedges are not traversed. Returns 1 yes, 0 no, -1 on invalid
   vertex/edge storage, endpoints, edge type, or allocation failure. */
int mulib_mkir_graph_path(const mulib_mkir_graph_t *g,size_t from,size_t to);
/* Directed edges only; picks the smallest ready vertex ID at each step.
   Fails on cycles; output is untouched on failure. */
int mulib_mkir_graph_toposort(const mulib_mkir_graph_t *g,size_t *ids,size_t capacity);
/* Checks structural consistency; non-NULL arrays and strings must refer to
   valid storage. May allocate scratch space. */
int mulib_mkir_graph_validate(const mulib_mkir_graph_t *g);
void mulib_mkir_graph_free(mulib_mkir_graph_t *g);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CDD_MARKUP_GRAPH) || defined(MULIB_IMPL_MARKUP_GRAPH)
#ifndef MULIB_MARKUP_GRAPH_IMPLEMENTED
#define MULIB_MARKUP_GRAPH_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
static void *mulib_mkir_graph_grow_(void *data,size_t *capacity,size_t need,size_t width){
 size_t max=SIZE_MAX/width,n=*capacity;void *p;
 if(need>max)return NULL;
 if(need<=n)return data;
 if(!n)n=max<8?need:8;
 while(n<need){if(n>max/2){n=need;break;}n*=2;}
 p=realloc(data,n*width);if(!p)return NULL;
 *capacity=n;return p;
}
static int mulib_mkir_graph_array_(const void *p,size_t count,size_t cap,size_t width){
 return count<=cap&&cap<=SIZE_MAX/width&&(p!=NULL)==(cap!=0);
}
static int mulib_mkir_graph_edge_shape_(const mulib_mkir_graph_t *g){
 return g&&mulib_mkir_graph_array_(g->vertices,g->vertex_count,g->vertex_capacity,sizeof(*g->vertices))&&
           mulib_mkir_graph_array_(g->edges,g->edge_count,g->edge_capacity,sizeof(*g->edges));
}
static int mulib_mkir_graph_shape_(const mulib_mkir_graph_t *g){
 return mulib_mkir_graph_edge_shape_(g)&&
   mulib_mkir_graph_array_(g->hyperedges,g->hyper_count,g->hyper_capacity,sizeof(*g->hyperedges))&&
   mulib_mkir_graph_array_(g->properties,g->property_count,g->property_capacity,sizeof(*g->properties));
}
static int mulib_mkir_graph_edge_valid_(const mulib_mkir_graph_t *g,const mulib_mkir_edge_t *e){
 return e->from<g->vertex_count&&e->to<g->vertex_count&&e->type&&*e->type;
}
static int mulib_mkir_graph_kind_(mulib_mkir_ref_kind_t kind){
 return kind==MULIB_MKIR_REF_FREE||kind==MULIB_MKIR_REF_NODE||
        kind==MULIB_MKIR_REF_SPAN||kind==MULIB_MKIR_REF_RUN;
}
static char *mulib_mkir_graph_dup_(const char *s){
 size_t n;char *p;if(!s)return NULL;n=strlen(s);if(n==SIZE_MAX)return NULL;
 p=(char*)malloc(n+1);if(p)memcpy(p,s,n+1);return p;
}
int mulib_mkir_graph_vertex(mulib_mkir_graph_t *g,mulib_mkir_ref_kind_t kind,size_t ref,size_t *id){
 mulib_mkir_vertex_t *p;size_t i;
 if(!mulib_mkir_graph_shape_(g)||!mulib_mkir_graph_kind_(kind))return 0;
 p=(mulib_mkir_vertex_t*)mulib_mkir_graph_grow_(g->vertices,&g->vertex_capacity,g->vertex_count+1,sizeof(*g->vertices));
 if(!p)return 0;
 g->vertices=p;i=g->vertex_count++;
 {mulib_mkir_vertex_t item={kind,ref};p[i]=item;}
 if(id)*id=i;
 return 1;
}
/* Adjacency scratch, built in O(V+E), without changing public graph order. */
static size_t *mulib_mkir_graph_links_(const mulib_mkir_graph_t *g){
 size_t n=g->vertex_count,m=g->edge_count,max=SIZE_MAX/sizeof(size_t),*links;
 if(n>max||m>max-n)return NULL;
 links=(size_t*)malloc((n+m)*sizeof(*links));if(!links)return NULL;
 for(size_t i=0;i<n;i++)links[i]=SIZE_MAX;
 for(size_t i=0;i<m;i++){
   size_t v=g->edges[i].from;
   links[n+i]=links[v];links[v]=i;
 }
 return links;
}
int mulib_mkir_graph_path(const mulib_mkir_graph_t *g,size_t from,size_t to){
 unsigned char *seen;size_t *queue,*links,head=0,tail=0,n;int result=0,direct=0,has_out=0;
 if(!mulib_mkir_graph_edge_shape_(g)||from>=g->vertex_count||to>=g->vertex_count)return -1;
 for(size_t i=0;i<g->edge_count;i++){
   const mulib_mkir_edge_t *e=&g->edges[i];
   if(!mulib_mkir_graph_edge_valid_(g,e))return -1;
   if(e->from==from){has_out=1;if(e->to==to)direct=1;}
 }
 if(from==to||direct)return 1;
 if(!has_out)return 0;
 n=g->vertex_count;
 links=mulib_mkir_graph_links_(g);if(!links)return -1;
 seen=(unsigned char*)calloc(n,1);queue=(size_t*)malloc(n*sizeof(size_t));
 if(!seen||!queue){result=-1;goto done;}
 queue[tail++]=from;seen[from]=1;
 while(head<tail&&!result){size_t v=queue[head++];
   for(size_t i=links[v];i!=SIZE_MAX;i=links[n+i]){size_t w=g->edges[i].to;
     if(w==to){result=1;break;}
     if(!seen[w]){seen[w]=1;queue[tail++]=w;}
   }
 }
 done:free(links);free(queue);free(seen);return result;
}
int mulib_mkir_graph_edge(mulib_mkir_graph_t *g,size_t from,size_t to,const char *type,size_t *id){
 mulib_mkir_edge_t *p;char *label;size_t i;
 if(!mulib_mkir_graph_shape_(g)||from>=g->vertex_count||to>=g->vertex_count||!type||!*type)return 0;
 if(g->acyclic&&mulib_mkir_graph_path(g,to,from)!=0)return 0;
 label=mulib_mkir_graph_dup_(type);if(!label)return 0;
 p=(mulib_mkir_edge_t*)mulib_mkir_graph_grow_(g->edges,&g->edge_capacity,g->edge_count+1,sizeof(*g->edges));
 if(!p){free(label);return 0;}
 g->edges=p;i=g->edge_count++;
 {mulib_mkir_edge_t item={from,to,label};p[i]=item;}
 if(id)*id=i;
 return 1;
}
int mulib_mkir_graph_hyperedge(mulib_mkir_graph_t *g,const size_t *members,size_t n,const char *type,size_t *id){
 mulib_mkir_hyperedge_t *p;char *label;size_t *copy,i;
 if(!mulib_mkir_graph_shape_(g)||!type||!*type||!members||!n||n>SIZE_MAX/sizeof(size_t))return 0;
 for(size_t i=0;i<n;i++)if(members[i]>=g->vertex_count)return 0;
 label=mulib_mkir_graph_dup_(type);if(!label)return 0;
 copy=(size_t*)malloc(n*sizeof(size_t));if(!copy){free(label);return 0;}
 memcpy(copy,members,n*sizeof(size_t));
 p=(mulib_mkir_hyperedge_t*)mulib_mkir_graph_grow_(g->hyperedges,&g->hyper_capacity,g->hyper_count+1,sizeof(*g->hyperedges));
 if(!p){free(label);free(copy);return 0;}
 g->hyperedges=p;i=g->hyper_count++;
 {mulib_mkir_hyperedge_t item={copy,n,label};p[i]=item;}
 if(id)*id=i;
 return 1;
}
static size_t mulib_mkir_graph_owner_limit_(const mulib_mkir_graph_t *g,mulib_mkir_property_owner_t owner){
 return owner==MULIB_MKIR_P_VERTEX?g->vertex_count:
        owner==MULIB_MKIR_P_EDGE?g->edge_count:
        owner==MULIB_MKIR_P_HYPEREDGE?g->hyper_count:0;
}
int mulib_mkir_graph_property(mulib_mkir_graph_t *g,mulib_mkir_property_owner_t owner,size_t id,const char *key,const char *value){
 mulib_mkir_property_t *p;char *v,*k;
 if(!mulib_mkir_graph_shape_(g)||!key||!*key||!value||id>=mulib_mkir_graph_owner_limit_(g,owner))return 0;
 v=mulib_mkir_graph_dup_(value);if(!v)return 0;
 for(size_t i=0;i<g->property_count;i++)if(g->properties[i].owner==owner&&g->properties[i].id==id){
   if(!g->properties[i].key){free(v);return 0;}
   if(strcmp(g->properties[i].key,key))continue;
   free(g->properties[i].value);g->properties[i].value=v;return 1;
 }
 k=mulib_mkir_graph_dup_(key);if(!k){free(v);return 0;}
 p=(mulib_mkir_property_t*)mulib_mkir_graph_grow_(g->properties,&g->property_capacity,g->property_count+1,sizeof(*g->properties));
 if(!p){free(k);free(v);return 0;}
 g->properties=p;
 {mulib_mkir_property_t item={owner,id,k,v};p[g->property_count++]=item;}
 return 1;
}
const char *mulib_mkir_graph_get_property(const mulib_mkir_graph_t *g,mulib_mkir_property_owner_t owner,size_t id,const char *key){
 if(!mulib_mkir_graph_shape_(g)||!key||!*key||id>=mulib_mkir_graph_owner_limit_(g,owner))return NULL;
 for(size_t i=0;i<g->property_count;i++)if(g->properties[i].owner==owner&&g->properties[i].id==id&&
     g->properties[i].key&&!strcmp(g->properties[i].key,key))return g->properties[i].value;
 return NULL;
}
static void mulib_mkir_graph_ready_(size_t *heap,size_t *len,size_t v){
 size_t i=(*len)++;
 while(i&&heap[(i-1)/2]>v){heap[i]=heap[(i-1)/2];i=(i-1)/2;}
 heap[i]=v;
}
static size_t mulib_mkir_graph_take_(size_t *heap,size_t *len){
 size_t result=heap[0],i=0;
 size_t last=heap[--(*len)];
 while(i<*len/2){
   size_t j=2*i+1;
   if(j+1<*len&&heap[j+1]<heap[j])j++;
   if(heap[j]>=last)break;
   heap[i]=heap[j];i=j;
 }
 heap[i]=last;return result;
}
/* All directed edges have been validated. If out is NULL, only test acyclicity. */
static int mulib_mkir_graph_topo_(const mulib_mkir_graph_t *g,size_t *out){
 size_t n=g->vertex_count,len=0,*links,*scratch,*deg,*heap,*tmp;
 size_t width=(out?3u:2u)*sizeof(size_t);int ok=0;
 if(!g->edge_count){
   if(out)for(size_t i=0;i<n;i++)out[i]=i;
   return 1;
 }
 if(n>SIZE_MAX/width)return 0;
 links=mulib_mkir_graph_links_(g);if(!links)return 0;
 scratch=(size_t*)calloc(n,width);if(!scratch){free(links);return 0;}
 deg=scratch;heap=deg+n;tmp=heap+n;
 for(size_t i=0;i<g->edge_count;i++)deg[g->edges[i].to]++;
 for(size_t i=0;i<n;i++)if(!deg[i])mulib_mkir_graph_ready_(heap,&len,i);
 for(size_t k=0;k<n;k++){
   size_t v;
   if(!len)goto done;
   v=mulib_mkir_graph_take_(heap,&len);
   if(out)tmp[k]=v;
   for(size_t i=links[v];i!=SIZE_MAX;i=links[n+i]){
     size_t w=g->edges[i].to;
     if(!--deg[w])mulib_mkir_graph_ready_(heap,&len,w);
   }
 }
 if(out)memcpy(out,tmp,n*sizeof(*out));
 ok=1;
 done:free(links);free(scratch);return ok;
}
int mulib_mkir_graph_toposort(const mulib_mkir_graph_t *g,size_t *out,size_t cap){
 if(!mulib_mkir_graph_edge_shape_(g)||(!out&&g->vertex_count)||cap<g->vertex_count)return 0;
 for(size_t i=0;i<g->edge_count;i++)if(!mulib_mkir_graph_edge_valid_(g,&g->edges[i]))return 0;
 return mulib_mkir_graph_topo_(g,out);
}
static int mulib_mkir_graph_property_cmp_(const void *pa,const void *pb){
 const mulib_mkir_property_t *a=*(const mulib_mkir_property_t *const*)pa;
 const mulib_mkir_property_t *b=*(const mulib_mkir_property_t *const*)pb;
 if(a->owner!=b->owner)return a->owner<b->owner?-1:1;
 if(a->id!=b->id)return a->id<b->id?-1:1;
 return strcmp(a->key,b->key);
}
int mulib_mkir_graph_validate(const mulib_mkir_graph_t *g){
 if(!mulib_mkir_graph_shape_(g))return 0;
 for(size_t i=0;i<g->vertex_count;i++)if(!mulib_mkir_graph_kind_(g->vertices[i].kind))return 0;
 for(size_t i=0;i<g->edge_count;i++)if(!mulib_mkir_graph_edge_valid_(g,&g->edges[i]))return 0;
 for(size_t i=0;i<g->hyper_count;i++){
   const mulib_mkir_hyperedge_t *h=&g->hyperedges[i];
   if(!h->count||h->count>SIZE_MAX/sizeof(*h->members)||!h->members||!h->type||!*h->type)return 0;
   for(size_t j=0;j<h->count;j++)if(h->members[j]>=g->vertex_count)return 0;
 }
 for(size_t i=0;i<g->property_count;i++){
   const mulib_mkir_property_t *p=&g->properties[i];
   if(p->id>=mulib_mkir_graph_owner_limit_(g,p->owner)||!p->key||!*p->key||!p->value)return 0;
 }
 if(g->property_count>1){
   const mulib_mkir_property_t **p;int duplicate=0;
   if(g->property_count>SIZE_MAX/sizeof(*p))return 0;
   p=(const mulib_mkir_property_t**)malloc(g->property_count*sizeof(*p));if(!p)return 0;
   for(size_t i=0;i<g->property_count;i++)p[i]=&g->properties[i];
   qsort(p,g->property_count,sizeof(*p),mulib_mkir_graph_property_cmp_);
   for(size_t i=1;i<g->property_count;i++)if(!mulib_mkir_graph_property_cmp_(&p[i-1],&p[i])){
     duplicate=1;break;
   }
   free(p);if(duplicate)return 0;
 }
 return !g->acyclic||mulib_mkir_graph_topo_(g,NULL);
}
void mulib_mkir_graph_free(mulib_mkir_graph_t *g){
 mulib_mkir_graph_t empty={NULL,0,0,NULL,0,0,NULL,0,0,NULL,0,0,0};
 if(!g)return;
 for(size_t i=0;i<g->edge_count;i++)free(g->edges[i].type);
 for(size_t i=0;i<g->hyper_count;i++){free(g->hyperedges[i].members);free(g->hyperedges[i].type);}
 for(size_t i=0;i<g->property_count;i++){free(g->properties[i].key);free(g->properties[i].value);}
 free(g->vertices);free(g->edges);free(g->hyperedges);free(g->properties);*g=empty;
}
#ifdef __cplusplus
}
#endif
#endif
#endif
