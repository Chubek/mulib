#define MULIB_IMPL_CDD_MARKUP_TREE
#define MULIB_IMPL_CDD_MARKUP_STREAM
#define MULIB_IMPL_CDD_MARKUP_SPANS
#define MULIB_IMPL_CDD_MARKUP_GRAPH
#define MULIB_IMPL_CDD_MARKUP_BRIDGE
#include "mulib-markup-bridge.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int count_token_(const mulib_mkir_token_t *tok,void *user){
 size_t *p=(size_t*)user;assert(tok->end>=tok->begin);(*p)++;return 1;
}
static int count_event_(const mulib_mkir_event_t *event,void *user){
 size_t *p=(size_t*)user;assert(event->data);(*p)++;return 1;
}
static void test_token_tree(void){
 const char *src="<?pi z?><p id=\"a\">Hello <em class='hot'>world</em>!</p><!--end-->";
 mulib_mkir_scanner_t scanner={src,strlen(src),0,0};mulib_mkir_token_t tk;
 int nt=0;while(mulib_mkir_scan_next(&scanner,&tk)>0)nt++;
 assert(nt==9 && scanner.cursor==strlen(src));
 size_t pushed_tokens=0;assert(mulib_mkir_scan_walk(src,strlen(src),count_token_,&pushed_tokens,NULL));
 assert(pushed_tokens==9);
 mulib_mkir_cst_t c={0};size_t bad=99;
 assert(mulib_mkir_cst_parse(src,strlen(src),256,&c,&bad));
 assert(c.count==9 && c.tree.count==8 && !memcmp(c.source,src,strlen(src)));
 assert(mulib_mkir_tree_validate(&c.tree));
 assert(c.tree.nodes[0].kind==MULIB_MKIR_DOCUMENT);
 assert(c.tree.nodes[2].kind==MULIB_MKIR_ELEMENT);
 assert(!strcmp(mulib_mkir_tree_get_attr(&c.tree,2,"id"),"a"));
 assert(!strcmp(mulib_mkir_tree_get_attr(&c.tree,4,"class"),"hot"));
 char *sx=mulib_mkir_tree_sxml(&c.tree,0);
 assert(sx && strstr(sx,"(p (@ (id \"a\"))") && strstr(sx,"(em (@ (class \"hot\"))"));
 free(sx);
 mulib_mkir_tree_t ast={0};assert(mulib_mkir_tree_ast(&c.tree,&ast,1));
 assert(ast.count==7);assert(mulib_mkir_tree_validate(&ast));
 mulib_mkir_events_t ev={0};mulib_mkir_event_t e={0};size_t enters=0,leaves=0,texts=0,comments=0;
 assert(mulib_mkir_events_begin(&c.tree,0,&ev));
 for(int res;(res=mulib_mkir_events_next(&ev,&e))>0;){
   if(e.kind==MULIB_MKIR_E_ENTER)enters++;
   if(e.kind==MULIB_MKIR_E_LEAVE)leaves++;
   if(e.kind==MULIB_MKIR_E_TEXT)texts++;
   if(e.kind==MULIB_MKIR_E_COMMENT)comments++;
 }
 assert(enters==3&&leaves==3&&texts==3&&comments==1);
 size_t pushed_events=0;assert(mulib_mkir_events_walk(&c.tree,0,count_event_,&pushed_events));
 assert(pushed_events==11);
 assert(mulib_mkir_tree_attach(&ast,4,5)==0); /* own parent or cycle */
 assert(mulib_mkir_tree_attr(&c.tree,2,"id","b"));
 assert(!strcmp(mulib_mkir_tree_get_attr(&c.tree,2,"id"),"b"));
 assert(mulib_mkir_events_next(&ev,&e)==-1); /* mutation invalidates cursor */
 mulib_mkir_tree_free(&ast);mulib_mkir_cst_free(&c);
 assert(!mulib_mkir_parse("<p><em></p>",11,0,&ast,&bad));
 assert(!mulib_mkir_parse("<p x='1' x='2'/>",16,0,&ast,&bad));
 assert(!mulib_mkir_parse("<p",2,0,&ast,&bad));
 assert(mulib_mkir_parse("",0,0,&ast,&bad));mulib_mkir_tree_free(&ast);
}
static void test_spans_graph(void){
 mulib_mkir_spans_t s={0};mulib_mkir_runs_t runs={0};mulib_mkir_graph_t g={0};
 size_t x,y;
 assert(mulib_mkir_spans_init(&s,"abcdefgh",8));
 assert(mulib_mkir_spans_add(&s,0,5,"A",0,&x)&&x==0);
 assert(mulib_mkir_spans_add(&s,3,8,"B",0,&y)&&y==1);
 assert(mulib_mkir_spans_validate(&s));
 assert(mulib_mkir_spans_runs(&s,&runs));
 assert(runs.count==3);
 assert(runs.items[1].begin==3 && runs.items[1].end==5 && runs.items[1].count==2);
 mulib_mkir_runs_free(&runs);
 assert(mulib_mkir_spans_to_graph(&s,&g));
 assert(g.vertex_count==5 && g.edge_count==4 && g.hyper_count==3);
 assert(mulib_mkir_graph_validate(&g));
 assert(mulib_mkir_graph_path(&g,0,3)==1 && mulib_mkir_graph_path(&g,3,0)==0);
 assert(!mulib_mkir_graph_edge(&g,3,0,"cycle",NULL));
 assert(mulib_mkir_graph_property(&g,MULIB_MKIR_P_VERTEX,0,"color","red"));
 assert(!strcmp(mulib_mkir_graph_get_property(&g,MULIB_MKIR_P_VERTEX,0,"color"),"red"));
 size_t order[5];assert(mulib_mkir_graph_toposort(&g,order,5));
 mulib_mkir_tree_t tree={0};assert(!mulib_mkir_spans_to_tree(&s,&tree)); /* crossing */
 mulib_mkir_graph_free(&g);mulib_mkir_spans_free(&s);
 assert(mulib_mkir_spans_init(&s,"Hello world!",12));
 assert(mulib_mkir_spans_add(&s,0,12,"p",0,NULL));
 assert(mulib_mkir_spans_add(&s,6,11,"em",0,NULL));
 assert(mulib_mkir_spans_to_tree(&s,&tree));
 assert(mulib_mkir_tree_validate(&tree));
 mulib_mkir_spans_t flatten={0};assert(mulib_mkir_tree_to_spans(&tree,0,&flatten));
 assert(flatten.length==12 && !memcmp(flatten.text,s.text,12));
 assert(flatten.count==2);
 mulib_mkir_graph_t dag={0};assert(mulib_mkir_tree_to_graph(&tree,&dag));
 assert(dag.vertex_count==tree.count && dag.edge_count==tree.count-1);
 assert(mulib_mkir_graph_vertex(&dag,MULIB_MKIR_REF_FREE,0,&x));
 assert(mulib_mkir_graph_edge(&dag,x,2,"shared",NULL)); /* multiple incoming edges */
 assert(!mulib_mkir_graph_edge(&dag,2,0,"back",NULL));
 mulib_mkir_graph_free(&dag);
 mulib_mkir_spans_free(&flatten);mulib_mkir_tree_free(&tree);mulib_mkir_spans_free(&s);
}
static void test_hybrid(void){
 const char *src="<p>Hello <em>world</em>!</p>";mulib_mkir_tree_t t={0};
 mulib_mkir_hybrid_t h={0};size_t err=0;
 assert(mulib_mkir_parse(src,strlen(src),0,&t,&err));
 assert(mulib_mkir_hybrid_from_tree(&t,0,&h));
 assert(h.spans.length==12 && !memcmp(h.spans.text,"Hello world!",12));
 assert(h.spans.count==2);
 assert(h.graph.vertex_count>=t.count+h.spans.count);
 assert(mulib_mkir_hybrid_reference(&h,3,1,"xref"));
 assert(mulib_mkir_hybrid_anchor(&h,1,0,"annotated-by"));
 assert(mulib_mkir_graph_validate(&h.graph));
 assert(mulib_mkir_spans_add(&h.spans,3,8,"extra",0,NULL));
 assert(mulib_mkir_hybrid_index(&h));
 assert(h.graph.vertex_count>t.count+h.spans.count);
 assert(h.graph.hyper_count>0);
 mulib_mkir_hybrid_free(&h);mulib_mkir_tree_free(&t);
}

static void test_edge_cases(void){
 mulib_mkir_tree_t forest={0},copy={0};size_t a,b,c,d,bad=0;
 assert(mulib_mkir_tree_add(&forest,MULIB_MKIR_NONE,MULIB_MKIR_DOCUMENT,NULL,NULL,0,0,4,&a));
 assert(mulib_mkir_tree_add(&forest,a,MULIB_MKIR_ELEMENT,"first",NULL,0,0,2,&b));
 assert(mulib_mkir_tree_add(&forest,a,MULIB_MKIR_ELEMENT,"second",NULL,0,2,4,&c));
 assert(mulib_mkir_tree_add(&forest,MULIB_MKIR_NONE,MULIB_MKIR_DOCUMENT,NULL,NULL,0,5,9,&d));
 assert(mulib_mkir_tree_attach(&forest,b,c));
 assert(!mulib_mkir_tree_attach(&forest,c,b));
 assert(mulib_mkir_tree_validate(&forest));
 assert(mulib_mkir_tree_ast(&forest,&copy,1));
 assert(copy.count==4 && mulib_mkir_tree_validate(&copy));
 assert(copy.nodes[1].name && !strcmp(copy.nodes[1].name,"second"));
 mulib_mkir_tree_free(&copy);mulib_mkir_tree_free(&forest);
 assert(!mulib_mkir_parse("<p><a>xx</a></p>",strlen("<p><a>xx</a></p>"),1,&copy,&bad));
 assert(mulib_mkir_parse("<p><a>xx</a></p>",strlen("<p><a>xx</a></p>"),2,&copy,&bad));
 assert(mulib_mkir_tree_validate(&copy));mulib_mkir_tree_free(&copy);
 assert(!mulib_mkir_parse("<p a='1' a='2'/>",strlen("<p a='1' a='2'/>"),0,&copy,&bad));
 assert(!mulib_mkir_parse("<p><a></p></a>",strlen("<p><a></p></a>"),0,&copy,&bad));
 assert(!mulib_mkir_parse("<!--unclosed",strlen("<!--unclosed"),0,&copy,&bad));
 assert(!mulib_mkir_parse("<p a='noend>",strlen("<p a='noend>"),0,&copy,&bad));
 assert(mulib_mkir_parse("<p a='>'>ok</p>",strlen("<p a='>'>ok</p>"),0,&copy,&bad));
 assert(mulib_mkir_tree_validate(&copy));mulib_mkir_tree_free(&copy);
 /* Duplicate-equal intervals are representable as deterministic nested tags. */
 mulib_mkir_spans_t s={0},flat={0};
 assert(mulib_mkir_spans_init(&s,"abc",3));
 assert(mulib_mkir_spans_add(&s,0,3,"outer",0,NULL));
 assert(mulib_mkir_spans_add(&s,0,3,"inner",0,NULL));
 assert(mulib_mkir_spans_add(&s,1,2,"strong",0,NULL));
 assert(mulib_mkir_spans_to_tree(&s,&copy));
 assert(mulib_mkir_tree_validate(&copy));
 assert(mulib_mkir_tree_to_spans(&copy,0,&flat));
 assert(flat.count==3 && flat.length==3 && !memcmp(flat.text,"abc",3));
 mulib_mkir_spans_free(&flat);mulib_mkir_tree_free(&copy);mulib_mkir_spans_free(&s);
 /* Binary text is length-aware, even with embedded NUL and an escaped quote. */
 char binary[]={'a','\0','"','\\','b'};
 assert(mulib_mkir_tree_add(&forest,MULIB_MKIR_NONE,MULIB_MKIR_TEXT,NULL,binary,sizeof(binary),0,sizeof(binary),&a));
 char *sx=mulib_mkir_tree_sxml(&forest,a);
 assert(sx && strstr(sx,"\\u0000") && strstr(sx,"\\\"") && strstr(sx,"\\\\"));
 free(sx);mulib_mkir_tree_free(&forest);
}
static void test_small_random_markup(void){
 const char alphabet[]="<>/'=\"!? abcxyz123&";uint32_t state=9123567u;
 char input[80];
 for(size_t n=0;n<75;n++)for(size_t k=0;k<22;k++){
   mulib_mkir_tree_t tree={0};size_t bad=0;
   for(size_t i=0;i<n;i++){state=state*1664525u+1013904223u;input[i]=alphabet[state%(sizeof(alphabet)-1)];}
   if(mulib_mkir_parse(input,n,8,&tree,&bad)){
     assert(mulib_mkir_tree_validate(&tree));
     char *sx=mulib_mkir_tree_sxml(&tree,0);assert(sx);free(sx);
   }else assert(bad<=n);
   mulib_mkir_tree_free(&tree);
  }
}

/* Compare every resulting arc and hyperedge with the interval model for all
 * pairs of byte ranges on three bytes, including empty, equal, nested, adjacent
 * and crossing ranges. Keep the input binary so C-string truncation is visible. */
static void test_range_oracle(void){
  const char text[]={'a','\0','b'};
  for(size_t a=0;a<=3;a++)for(size_t b=a;b<=3;b++)
  for(size_t c=0;c<=3;c++)for(size_t d=c;d<=3;d++){
    const size_t lo[]={a,c},hi[]={b,d};
    const char *labels[]={"a","b"};
    mulib_mkir_spans_t s={0},flat={0};
    mulib_mkir_runs_t runs={0};
    mulib_mkir_graph_t g={0};
    mulib_mkir_tree_t t={0};
    assert(mulib_mkir_spans_init(&s,text,sizeof text));
    for(size_t i=0;i<2;i++)assert(mulib_mkir_spans_add(&s,lo[i],hi[i],labels[i],0,NULL));
    assert(mulib_mkir_spans_runs(&s,&runs) && mulib_mkir_spans_to_graph(&s,&g));
    assert(mulib_mkir_graph_validate(&g) && g.vertex_count==2+runs.count);
    size_t edge=0,hyper=0,start=0,boundaries=0;
    for(size_t x=1;x<3;x++)
      boundaries+=(a<b && (x==a || x==b)) || (c<d && (x==c || x==d));
    assert(runs.count==1+boundaries);
    for(size_t j=0;j<runs.count;j++) {
      const mulib_mkir_run_t *r=&runs.items[j];
      size_t active=0;
      assert(r->begin==start && r->end>start && r->end<=3);
      start=r->end;
      assert(g.vertices[j+2].kind==MULIB_MKIR_REF_RUN && g.vertices[j+2].ref==j);
      for(size_t i=0;i<2;i++)if(lo[i]<=r->begin && hi[i]>=r->end){
        assert(active<r->count && r->annotations[active++]==i);
        assert(edge<g.edge_count && g.edges[edge].from==i &&
               g.edges[edge].to==j+2 && !strcmp(g.edges[edge].type,"covers"));
        edge++;
      }
      assert(active==r->count);
      if(active){
        assert(hyper<g.hyper_count);
        const mulib_mkir_hyperedge_t *h=&g.hyperedges[hyper++];
        assert(h->count==active+1 && h->members[0]==j+2 &&
               !strcmp(h->type,"annotation-set"));
        for(size_t i=0;i<active;i++)assert(h->members[i+1]==r->annotations[i]);
      }
    }
    assert(edge==g.edge_count && hyper==g.hyper_count && start==sizeof text);
    int fits=a<b && c<d && !(a<c && c<b && b<d) && !(c<a && a<d && d<b);
    assert(mulib_mkir_spans_to_tree(&s,&t)==fits);
    if(fits){
      assert(mulib_mkir_tree_to_spans(&t,0,&flat) && flat.count==2 &&
             flat.length==sizeof text && !memcmp(text,flat.text,sizeof text));
      for(size_t i=0;i<2;i++) {
        size_t matches=0;
        for(size_t j=0;j<2;j++)
          matches+=!strcmp(labels[i],flat.items[j].label) &&
                   lo[i]==flat.items[j].begin && hi[i]==flat.items[j].end;
        assert(matches==1);
      }
    }else assert(!t.nodes && !t.count);
    mulib_mkir_tree_free(&t);mulib_mkir_spans_free(&flat);
    mulib_mkir_graph_free(&g);mulib_mkir_runs_free(&runs);mulib_mkir_spans_free(&s);
  }
}

int main(void){
  test_token_tree();test_spans_graph();test_hybrid();test_edge_cases();
  test_small_random_markup();test_range_oracle();puts("markup IR tests passed");
  return 0;
}
