#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_AST
#define MULIB_IMPL_EVTSTREAM
#define MULIB_IMPL_LISTENER
#define MULIB_IMPL_VISITOR
#define MULIB_IMPL_GRAPH
#define MULIB_IMPL_SEARCH
#define MULIB_IMPL_SORT
#include "mulib-listener.h"
#include "mulib-visitor.h"
#include "mulib-graph.h"
#include "mulib-search.h"
#include "mulib-sort.h"
#include <assert.h>

static int action(const mulib_ast_t *node,void *user) {(void)node;++*(size_t *)user;return 1;}
static mulib_visit_result_t visit(const mulib_ast_t *node,void *user) {
    ++*(size_t *)user;return !strcmp(node->production,"skip")?MULIB_VISIT_SKIP:MULIB_VISIT_DESCEND;
}
typedef struct {int key,sequence;} record;
static int compare(const void *a,const void *b) {
    const record *x=a,*y=b;return (x->key>y->key)-(x->key<y->key);
}
/* Deliberately heterogeneous key and element types: same contract as bsearch. */
static int key_compare(const void *key,const void *element) {
    int a=*(const int *)key,b=((const record *)element)->key;return (a>b)-(a<b);
}
typedef struct {size_t offsets[8],count,limit;} matches;
static int match(size_t offset,void *user) {
    matches *m=user;m->offsets[m->count++]=offset;return m->count<m->limit;
}
int main(void) {
    /* Retain source compatibility with the original public tags. */
    struct mulib_graph g={0};struct mulib_event event;struct mulib_evtstream stream;
    struct mulib_listener listener;enum mulib_visit_result result=MULIB_VISIT_DESCEND;
    assert(result==MULIB_VISIT_DESCEND);
    mulib_ast_t *root=mulib_ast_create("root","",0),*child=mulib_ast_create("skip","",0),*leaf=mulib_ast_create("leaf","",0);
    assert(root&&child&&leaf);assert(mulib_ast_append(root,child));assert(mulib_ast_append(child,leaf));
    stream=mulib_evtstream_from_ast(root);event.depth=99;
    assert(mulib_evtstream_next_bounded(&stream,&event,0)==-2&&event.depth==99&&stream.node==root);
    size_t depths[]={0,1,2,2,1,0};
    for(size_t i=0;i<6;i++) {
        assert(mulib_evtstream_next_bounded(&stream,&event,2)==1&&event.depth==depths[i]);
        assert(event.kind==(i<3?MULIB_EVENT_ENTER:MULIB_EVENT_LEAVE));
    }
    assert(!mulib_evtstream_next(&stream,&event));
    stream=mulib_evtstream_from_ast(child);size_t events=0;
    while(mulib_evtstream_next(&stream,&event)==1)events++;
    assert(events==4);
    size_t calls=0;listener=(mulib_listener_t){NULL,action,action,&calls};
    assert(mulib_listener_walk_bounded(root,&listener,1,5)==-2&&calls==5);
    calls=0;assert(mulib_listener_walk_bounded(root,&listener,1,6)==1&&calls==6);
    listener.production="leaf";calls=0;
    assert(mulib_listener_walk(root,&listener,1)==1&&calls==2);
    calls=0;assert(mulib_visitor_walk_bounded(root,visit,&calls,1)==-2&&calls==1);
    calls=0;assert(mulib_visitor_walk_bounded(root,visit,&calls,2)==1&&calls==2);
    mulib_ast_destroy(root);
    assert(mulib_graph_init(&g,4));
    assert(mulib_graph_edge(&g,0,2,1)&&mulib_graph_edge(&g,0,1,1)&&mulib_graph_edge(&g,2,3,1));
    size_t order[4];assert(mulib_graph_bfs(&g,0,order,4)==4&&order[1]==1&&order[2]==2);
    assert(mulib_graph_toposort(&g,order,4)&&order[0]==0&&order[3]==3);
    mulib_graph_t reverse={0};assert(mulib_graph_reverse(&g,&reverse));
    assert(reverse.edges[3*4+2]&&!reverse.edges[2*4+3]);
    assert(mulib_graph_edge(&g,3,0,1));order[0]=99;
    assert(!mulib_graph_toposort(&g,order,4)&&order[0]==99);
    mulib_graph_free(&g);mulib_graph_free(&reverse);
    assert(mulib_graph_init(&g,0)&&mulib_graph_toposort(&g,NULL,0));mulib_graph_free(&g);
    matches m={{0},0,8};
    assert(mulib_search_all("aaaa",4,"aa",2,match,&m)==3&&m.offsets[0]==0&&m.offsets[2]==2);
    m.count=0;m.limit=1;assert(mulib_search_all("aaaa",4,"aa",2,match,&m)==1&&m.count==1);
    assert(mulib_search_kmp("a\0b\0b",5,"\0b",2)==1);
    assert(mulib_search_kmp("abc",3,"z",1)==SIZE_MAX);
    m.count=0;assert(mulib_search_all(NULL,0,NULL,0,match,&m)==1&&m.offsets[0]==0);
    assert(mulib_search_all(NULL,1,"x",1,match,&m)==SIZE_MAX);
    for(size_t n=0;n<100;n++) {
        record records[100];for(size_t i=0;i<n;i++)records[i]=(record){(int)((n-i)%7),(int)i};
        assert(mulib_sort_stable(records,n,sizeof *records,compare));
        for(size_t i=1;i<n;i++)assert(records[i-1].key<records[i].key||
            (records[i-1].key==records[i].key&&records[i-1].sequence<records[i].sequence));
        for(int key=-1;key<9;key++) {
            size_t expected=0;while(expected<n&&records[expected].key<key)expected++;
            assert(mulib_sort_lower_bound(&key,records,n,sizeof *records,key_compare)==expected);
        }
    }
    int key=1;assert(mulib_sort_lower_bound(&key,NULL,0,sizeof(record),key_compare)==0);
    assert(!mulib_sort_stable(&key,SIZE_MAX,2,compare));
    return 0;
}
