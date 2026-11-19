/* Focused markup forest, lexical/CST and event tests, with allocation failures. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static size_t attempts, fail_at = SIZE_MAX, live;
static void *test_malloc(size_t n){
  void *p;
  if(attempts++ == fail_at)return NULL;
  p=malloc(n);
  if(p)live++;
  return p;
}
static void *test_realloc(void *p,size_t n){
  int new_allocation=p==NULL;
  if(attempts++ == fail_at)return NULL;
  p=realloc(p,n);
  if(p&&new_allocation)live++;
  return p;
}
static void test_free(void *p){
  if(p){assert(live);live--;}
  free(p);
}
#define malloc test_malloc
#define realloc test_realloc
#define free test_free
#define MULIB_IMPL_CDD_MARKUP_TREE
#define MULIB_IMPL_CDD_MARKUP_STREAM
#include "mulib-markup-stream.h"
#undef malloc
#undef realloc
#undef free

static size_t add(mulib_mkir_tree_t *t,size_t p,mulib_mkir_kind_t k,const char *name){
  size_t i;
  assert(mulib_mkir_tree_add(t,p,k,name,NULL,0,MULIB_MKIR_NONE,MULIB_MKIR_NONE,&i));
  return i;
}

static void test_topology(void){
  mulib_mkir_tree_t t={0},copy={0};
  size_t a=add(&t,MULIB_MKIR_NONE,MULIB_MKIR_DOCUMENT,NULL);
  size_t b=add(&t,a,MULIB_MKIR_ELEMENT,"b");
  size_t c=add(&t,a,MULIB_MKIR_ELEMENT,"c");
  size_t d=add(&t,a,MULIB_MKIR_ELEMENT,"d");
  size_t e=add(&t,b,MULIB_MKIR_ELEMENT,"e");
  size_t f=add(&t,MULIB_MKIR_NONE,MULIB_MKIR_ELEMENT,"f");
  uint64_t v=t.version;
  size_t id=110;
  assert(mulib_cdd_mkir_tree_insert_before(&t,b,a,b));
  assert(mulib_cdd_mkir_tree_insert_before(&t,b,a,c));
  assert(mulib_mkir_tree_attach(&t,d,a));
  assert(mulib_mkir_tree_attach(&t,f,MULIB_MKIR_NONE));
  assert(!mulib_cdd_mkir_tree_insert_before(&t,e,a,f));
  assert(!mulib_cdd_mkir_tree_insert_before(&t,b,e,MULIB_MKIR_NONE));
  assert(!mulib_cdd_mkir_tree_insert_before(&t,e,MULIB_MKIR_NONE,b));
  assert(!mulib_cdd_mkir_tree_insert_before(&t,e,a,t.count));
  assert(!mulib_cdd_mkir_tree_insert_before(&t,e,t.count,MULIB_MKIR_NONE));
  assert(!mulib_cdd_mkir_tree_insert_before(NULL,b,a,c));
  assert(!mulib_mkir_tree_add(&t,a,MULIB_MKIR_ELEMENT,"",NULL,0,0,1,&id));
  assert(!mulib_mkir_tree_add(&t,a,MULIB_MKIR_TEXT,NULL,"",SIZE_MAX,0,1,&id));
  assert(!mulib_mkir_tree_add(&t,a,MULIB_MKIR_TEXT,NULL,NULL,0,0,MULIB_MKIR_NONE,&id));
  assert(!mulib_mkir_tree_add(&t,a,MULIB_MKIR_TEXT,NULL,NULL,0,MULIB_MKIR_NONE,0,&id));
  assert(!mulib_mkir_tree_add(&t,a,MULIB_MKIR_TEXT,NULL,NULL,0,2,1,&id));
  assert(id==110&&v==t.version);
  assert(mulib_cdd_mkir_tree_insert_before(&t,d,a,b)); /* last to first */
  assert(mulib_cdd_mkir_tree_insert_before(&t,c,a,b)); /* last to middle */
  assert(mulib_cdd_mkir_tree_insert_before(&t,f,a,c)); /* insert a detached root */
  assert(t.nodes[a].first_child==d&&t.nodes[a].last_child==b);
  assert(t.nodes[d].next==f&&t.nodes[c].prev==f&&t.nodes[c].next==b);
  assert(mulib_mkir_tree_attach(&t,c,MULIB_MKIR_NONE)); /* splice out middle */
  assert(t.nodes[c].parent==MULIB_MKIR_NONE&&t.nodes[c].prev==MULIB_MKIR_NONE);
  assert(mulib_cdd_mkir_tree_insert_before(&t,b,f,MULIB_MKIR_NONE)); /* newer parent ID */
  assert(t.nodes[b].parent==f&&t.nodes[e].parent==b);
  assert(t.version==v+5&&mulib_mkir_tree_validate(&t));
  assert(mulib_mkir_tree_ast(&t,&copy,0)&&mulib_mkir_tree_validate(&copy));
  assert(copy.count==t.count);
  assert(!strcmp(copy.nodes[1].name,"d")&&!strcmp(copy.nodes[2].name,"f"));
  assert(!strcmp(copy.nodes[3].name,"b")&&!strcmp(copy.nodes[4].name,"e"));
  assert(!strcmp(copy.nodes[5].name,"c")&&copy.nodes[5].parent==MULIB_MKIR_NONE);
  mulib_mkir_tree_free(&copy);
  mulib_mkir_tree_free(&t);
  assert(!live);
}

static void test_validation(void){
  mulib_mkir_tree_t t={0},copy={0};
  size_t root=add(&t,MULIB_MKIR_NONE,MULIB_MKIR_DOCUMENT,NULL);
  size_t a=add(&t,MULIB_MKIR_NONE,MULIB_MKIR_ELEMENT,"a");
  size_t b=add(&t,a,MULIB_MKIR_ELEMENT,"b");
  size_t c=add(&t,a,MULIB_MKIR_ELEMENT,"c");
  mulib_mkir_node_t a_save=t.nodes[a],b_save=t.nodes[b],c_save=t.nodes[c];
  char *key;
  assert(mulib_mkir_tree_attr(&t,a,"id","a"));
  assert(mulib_mkir_tree_attr(&t,b,"id","b")); /* same key, different owner */
  assert(mulib_mkir_tree_attr(&t,a,"class","hot"));
  assert(mulib_mkir_tree_validate(&t));
  key=t.attrs[2].key;
  t.attrs[2].key=t.attrs[0].key;
  assert(!mulib_mkir_tree_validate(&t));
  t.attrs[2].key=key;
  t.attrs[2].owner=root;
  assert(!mulib_mkir_tree_validate(&t));
  t.attrs[2].owner=a;
  t.nodes[a].next=t.count;
  assert(!mulib_mkir_tree_validate(&t));
  assert(!mulib_mkir_tree_attach(&t,a,root));
  t.nodes[a]=a_save;
  t.nodes[b].next=t.count;
  assert(!mulib_mkir_tree_validate(&t));
  assert(!mulib_mkir_tree_attach(&t,b,root));
  t.nodes[b]=b_save;
  t.nodes[a].parent=b;
  t.nodes[b].first_child=t.nodes[b].last_child=a;
  assert(!mulib_mkir_tree_validate(&t)); /* locally consistent disconnected cycle */
  assert(!mulib_mkir_tree_ast(&t,&copy,0)&&!copy.nodes);
  assert(!mulib_mkir_tree_sxml(&t,root));
  t.nodes[a]=a_save;t.nodes[b]=b_save;
  t.nodes[c].prev=MULIB_MKIR_NONE;
  assert(!mulib_mkir_tree_validate(&t));
  t.nodes[c]=c_save;
  t.nodes[a].source_begin=42;
  assert(!mulib_mkir_tree_validate(&t));
  t.nodes[a]=a_save;
  t.nodes[a].value_length=SIZE_MAX;
  assert(!mulib_mkir_tree_validate(&t));
  t.nodes[a]=a_save;
  assert(mulib_mkir_tree_validate(&t));
  copy.capacity=1;
  assert(!mulib_mkir_tree_ast(&t,&copy,0)&&copy.capacity==1);
  mulib_mkir_tree_free(&t);
  assert(!live);
}

static void test_sxml_and_ast(void){
  const char bytes[]={'A',0,'\n','"','\\',(char)127};
  mulib_mkir_tree_t t={0},copy={0};
  size_t root=add(&t,MULIB_MKIR_NONE,MULIB_MKIR_ELEMENT,"not | an\\atom)");
  size_t text=add(&t,root,MULIB_MKIR_TEXT,NULL);
  size_t comment=add(&t,text,MULIB_MKIR_COMMENT,NULL);
  size_t trivia=add(&t,root,MULIB_MKIR_TRIVIA,NULL);
  char *sxml;
  add(&t,comment,MULIB_MKIR_ELEMENT,"omitCommentSubtree");
  add(&t,trivia,MULIB_MKIR_ELEMENT,"omitTriviaSubtree");
  assert(mulib_mkir_tree_value(&t,text,bytes,sizeof(bytes)));
  assert(mulib_mkir_tree_value(&t,text,t.nodes[text].value,t.nodes[text].value_length));
  assert(mulib_mkir_tree_attr(&t,root,"key )","< &"));
  assert(mulib_mkir_tree_attr(&t,root,"z","last"));
  assert(mulib_mkir_tree_attr(&t,root,"z",mulib_mkir_tree_get_attr(&t,root,"z")));
  assert(mulib_mkir_tree_ast(&t,&copy,1));
  assert(copy.count==2&&copy.attr_count==2&&copy.nodes[1].value!=t.nodes[text].value);
  assert(copy.nodes[1].value_length==sizeof(bytes)&&!memcmp(copy.nodes[1].value,bytes,sizeof(bytes)));
  sxml=mulib_mkir_tree_sxml(&t,root);
  assert(sxml&&!strcmp(sxml,"(|not \\| an\\\\atom)| (@ (|key )| \"< &\") (z \"last\"))"
                             " (*TEXT* \"A\\u0000\\u000a\\\"\\\\\\u007f\""
                             " (*COMMENT* (omitCommentSubtree))) (*TRIVIA* \"\" (omitTriviaSubtree)))"));
  test_free(sxml);
  mulib_mkir_tree_free(&copy);mulib_mkir_tree_free(&t);
  assert(!live);
}

static int stop_token(const mulib_mkir_token_t *token,void *user){
  size_t *count=(size_t*)user;
  assert(token->begin<token->end);
  (*count)++;
  return 0;
}

static void test_lossless_cst(void){
  static const char expected[]="<?pi?><!decl><a z=\" > \" n='&quot;'>a\0b"
                               "<![CDATA[<x> &\0]]]> <!--  ! --><b /></a>\n";
  char input[sizeof(expected)],joined[sizeof(expected)];
  mulib_mkir_cst_t cst={0};
  mulib_mkir_tree_t parsed={0};
  size_t total=0,err=42,cb_count=0;
  const char *p=NULL;
  size_t n=42;
  char *one,*two;
  memcpy(input,expected,sizeof(input));
  assert(mulib_mkir_scan_walk(input,sizeof(input)-1,stop_token,&cb_count,&err)==-1);
  assert(cb_count==1&&err==0);
  assert(mulib_mkir_cst_parse(input,sizeof(input)-1,2,&cst,&err)&&err==0);
  assert(cst.length==sizeof(input)-1&&cst.count==10);
  assert(cst.tokens[4].kind==MULIB_MKIR_T_CDATA);
  assert(cst.tokens[4].content_begin-cst.tokens[4].begin==9);
  assert(cst.tokens[4].end-cst.tokens[4].content_end==3);
  assert(cst.tree.nodes[5].kind==MULIB_MKIR_CDATA);
  assert(cst.tree.nodes[5].source_begin==cst.tokens[4].begin);
  assert(cst.tree.nodes[5].source_end==cst.tokens[4].end);
  assert(cst.tree.nodes[5].value_length==7&&!memcmp(cst.tree.nodes[5].value,"<x> &\0]",7));
  memset(input,'x',sizeof(input)); /* caller's source is not borrowed */
  for(size_t i=0;i<cst.count;i++){
    assert(mulib_cdd_mkir_cst_token_bytes(&cst,i,&p,&n));
    assert(n&&n<=cst.length-total&&p==cst.source+total);
    memcpy(joined+total,p,n);total+=n;
  }
  assert(total==cst.length&&!memcmp(joined,expected,total)&&cst.source[total]==0);
  assert(!mulib_cdd_mkir_cst_token_bytes(&cst,cst.count,&p,&n));
  assert(!mulib_cdd_mkir_cst_token_bytes(&cst,0,NULL,&n));
  assert(p==cst.source+total-1&&n==1);
  cst.tokens[0].end=SIZE_MAX;
  assert(!mulib_cdd_mkir_cst_token_bytes(&cst,0,&p,&n));
  cst.tokens[0].end=6;
  assert(mulib_mkir_parse(joined,total,2,&parsed,&err));
  one=mulib_mkir_tree_sxml(&cst.tree,0);two=mulib_mkir_tree_sxml(&parsed,0);
  assert(one&&two&&!strcmp(one,two)&&strstr(one,"a\\u0000b"));
  test_free(one);test_free(two);
  assert(cst.tree.nodes[3].source_begin==13&&cst.tree.nodes[3].source_end==total-1);
  assert(!strcmp(mulib_mkir_tree_get_attr(&cst.tree,3,"z")," > "));
  assert(mulib_mkir_tree_attr(&cst.tree,3,"z","changed"));
  assert(mulib_mkir_tree_attach(&cst.tree,3,MULIB_MKIR_NONE));
  assert(!memcmp(cst.source,expected,total));
  assert(mulib_cdd_mkir_cst_token_bytes(&cst,2,&p,&n)&&p==cst.source+13&&n>0);
  assert(!mulib_mkir_cst_parse(NULL,0,0,&cst,&err)&&err==0);
  assert(mulib_mkir_cst_parse(NULL,0,0,&(mulib_mkir_cst_t){.length=1},&err)==0);
  mulib_mkir_tree_free(&parsed);mulib_mkir_cst_free(&cst);
  assert(!live);
}

static void check_parse_failure(const char *s,size_t n,size_t limit,size_t offset){
  mulib_mkir_tree_t tree={0};
  mulib_mkir_cst_t cst={0};
  size_t bad=SIZE_MAX;
  assert(!mulib_mkir_parse(s,n,limit,&tree,&bad)&&bad==offset);
  assert(!tree.nodes&&!tree.attrs&&!tree.count&&!tree.capacity&&!tree.attr_capacity&&!tree.version);
  bad=SIZE_MAX;
  assert(!mulib_mkir_cst_parse(s,n,limit,&cst,&bad)&&bad==offset);
  assert(!cst.source&&!cst.tokens&&!cst.length&&!cst.count&&!cst.capacity&&!cst.tree.nodes);
  assert(!live);
}

static void test_lexical_failures(void){
  mulib_mkir_scanner_t s={"ok<!--",6,0,99};
  mulib_mkir_token_t token;
  mulib_mkir_cst_t empty={0};
  size_t bad=SIZE_MAX;
  assert(mulib_mkir_scan_next(&s,&token)==1&&token.end==2&&token.kind==MULIB_MKIR_T_TEXT);
  assert(mulib_mkir_scan_next(&s,&token)==-1&&s.error_offset==2&&s.cursor==2&&token.end==2);
  s.cursor=7;
  assert(mulib_mkir_scan_next(&s,&token)==-1&&s.error_offset==6&&s.cursor==7);
  s.cursor=2;
  assert(mulib_mkir_scan_next(&s,NULL)==-1&&s.error_offset==2);
  s.source=NULL;
  assert(mulib_mkir_scan_next(&s,&token)==-1&&s.error_offset==2);
  assert(mulib_mkir_scan_next(NULL,&token)==-1);
  check_parse_failure(NULL,1,0,0);
  check_parse_failure("",SIZE_MAX,0,0);
  check_parse_failure("<a><b/></a>",strlen("<a><b/></a>"),1,3);
  check_parse_failure("<a><b></b></a>",strlen("<a><b></b></a>"),1,3);
  check_parse_failure("<a x='q'y='r'/>",strlen("<a x='q'y='r'/>"),0,8);
  check_parse_failure("<a x='q' x='r'/>",strlen("<a x='q' x='r'/>"),0,9);
  check_parse_failure("<a x='q\0r'/>",sizeof("<a x='q\0r'/>")-1,0,7);
  check_parse_failure("<a>",3,0,3);
  check_parse_failure("<a></b>",7,0,3);
  check_parse_failure("<?",2,0,0);
  check_parse_failure("x<![CDATA[abc]]",strlen("x<![CDATA[abc]]"),0,1);
  assert(mulib_mkir_cst_parse("<![CDATA[]]>",12,0,&empty,&bad)&&bad==0);
  assert(empty.count==1&&empty.tokens[0].kind==MULIB_MKIR_T_CDATA);
  assert(empty.tokens[0].content_begin==empty.tokens[0].content_end);
  assert(empty.tree.nodes[1].kind==MULIB_MKIR_CDATA&&!empty.tree.nodes[1].value_length);
  mulib_mkir_cst_free(&empty);
  assert(mulib_mkir_cst_parse(NULL,0,0,&empty,&bad)&&bad==0);
  assert(empty.source&&empty.source[0]==0&&!empty.count&&empty.tree.count==1);
  mulib_mkir_cst_free(&empty);
  assert(!live);
}

static void test_attribute_order(void){
  const char ordered[]="<n z='1' y='2' x='3' w='4' v='5' u='6' t='7' s='8' r='9'><n z='10'/></n>";
  const char duplicate[]="<n z='1' a='2' y='3' z='4' a='5'/>";
  mulib_mkir_tree_t t={0};
  size_t bad=SIZE_MAX;
  check_parse_failure(duplicate,sizeof(duplicate)-1,0,21); /* first repeated key in source order */
  assert(mulib_mkir_parse(ordered,sizeof(ordered)-1,0,&t,&bad)&&bad==0);
  assert(mulib_mkir_tree_validate(&t)&&t.attr_count==10);
  for(size_t i=0;i<9;i++){
    assert(t.attrs[i].owner==1&&t.attrs[i].key[0]=='z'-(int)i&&t.attrs[i].key[1]==0);
  }
  assert(t.attrs[9].owner==2&&!strcmp(t.attrs[9].key,"z")&&!strcmp(t.attrs[9].value,"10"));
  mulib_mkir_tree_free(&t);
  assert(!live);
}

typedef struct walk_state {
  size_t count;
  int stop;
  mulib_mkir_tree_t *mutate;
} walk_state;

static int count_event(const mulib_mkir_event_t *e,void *user){
  walk_state *state=(walk_state*)user;
  assert(e->data);
  state->count++;
  if(state->mutate)assert(mulib_mkir_tree_value(state->mutate,e->node,"x",1));
  return !state->stop;
}

static void test_events(void){
  mulib_mkir_tree_t t={0},replayed={0};
  mulib_mkir_events_t it;
  mulib_mkir_event_t ev;
  walk_state walked={0,0,NULL};
  size_t ancestors[6];
  size_t root=add(&t,MULIB_MKIR_NONE,MULIB_MKIR_DOCUMENT,NULL);
  size_t text=add(&t,root,MULIB_MKIR_TEXT,NULL);
  size_t comment=add(&t,text,MULIB_MKIR_COMMENT,NULL);
  size_t elem=add(&t,comment,MULIB_MKIR_ELEMENT,"x");
  size_t pi=add(&t,elem,MULIB_MKIR_PI,NULL);
  size_t trivia=add(&t,elem,MULIB_MKIR_TRIVIA,NULL);
  size_t cdata=add(&t,trivia,MULIB_MKIR_CDATA,NULL);
  size_t doctype=add(&t,root,MULIB_MKIR_DOCTYPE,NULL);
  const size_t nodes[]={root,text,comment,elem,pi,trivia,cdata,trivia,elem,doctype,root};
  const size_t depths[]={0,1,2,3,4,4,5,4,3,1,0};
  const mulib_mkir_event_kind_t kinds[]={MULIB_MKIR_E_ENTER,MULIB_MKIR_E_TEXT,
    MULIB_MKIR_E_COMMENT,MULIB_MKIR_E_ENTER,MULIB_MKIR_E_OTHER,MULIB_MKIR_E_ENTER,
    MULIB_MKIR_E_TEXT,MULIB_MKIR_E_LEAVE,MULIB_MKIR_E_LEAVE,MULIB_MKIR_E_OTHER,MULIB_MKIR_E_LEAVE};
  assert(mulib_mkir_events_begin(&t,root,&it));
  assert(!mulib_mkir_events_begin(&t,t.count,&it)); /* failing begin leaves it usable */
  for(size_t i=0;i<sizeof(nodes)/sizeof(nodes[0]);i++){
    assert(mulib_mkir_events_next(&it,&ev)==1);
    assert(ev.node==nodes[i]&&ev.data==&t.nodes[nodes[i]]&&ev.depth==depths[i]&&ev.kind==kinds[i]);
    if(ev.kind!=MULIB_MKIR_E_LEAVE){
      const mulib_mkir_node_t *n=ev.data;
      size_t parent=ev.depth?ancestors[ev.depth-1]:MULIB_MKIR_NONE;
      assert(mulib_mkir_tree_add(&replayed,parent,n->kind,n->name,n->value,n->value_length,
                                 n->source_begin,n->source_end,&ancestors[ev.depth]));
    }
  }
  assert(!mulib_mkir_events_next(&it,&ev)&&ev.node==root&&ev.kind==MULIB_MKIR_E_LEAVE);
  {
    char *original=mulib_mkir_tree_sxml(&t,root),*replay=mulib_mkir_tree_sxml(&replayed,0);
    assert(original&&replay&&!strcmp(original,replay)&&replayed.count==t.count);
    test_free(original);test_free(replay);
    mulib_mkir_tree_free(&replayed);
  }
  assert(mulib_mkir_events_begin(&t,text,&it)); /* payload root has no synthetic LEAVE */
  for(size_t i=1;i<=8;i++){
    assert(mulib_mkir_events_next(&it,&ev)==1&&ev.node==nodes[i]&&ev.depth==depths[i]-1);
  }
  assert(!mulib_mkir_events_next(&it,&ev)); /* must not reach root's next child */
  assert(mulib_cdd_mkir_events_walk_bounded(&t,root,5,11,count_event,&walked)==1&&walked.count==11);
  walked.count=0;
  assert(mulib_cdd_mkir_events_walk_bounded(&t,root,4,SIZE_MAX,count_event,&walked)==-2&&walked.count==6);
  walked.count=0;
  assert(mulib_cdd_mkir_events_walk_bounded(&t,root,5,3,count_event,&walked)==-2&&walked.count==3);
  walked.count=0;
  assert(mulib_cdd_mkir_events_walk_bounded(&t,root,0,0,count_event,&walked)==-2&&!walked.count);
  assert(!mulib_cdd_mkir_events_walk_bounded(&t,t.count,5,11,count_event,&walked)&&!walked.count);
  walked.stop=1;
  assert(mulib_mkir_events_walk(&t,root,count_event,&walked)==-1&&walked.count==1);
  walked.count=0;walked.stop=0;walked.mutate=&t;
  assert(!mulib_mkir_events_walk(&t,root,count_event,&walked)&&walked.count==1);
  assert(mulib_mkir_events_next(&it,&ev)==-1&&ev.node==elem);
  assert(mulib_mkir_events_begin(&t,root,&it));
  it.depth=SIZE_MAX;
  assert(mulib_mkir_events_next(&it,&ev)==-1&&ev.node==elem);
  assert(mulib_mkir_events_begin(&t,root,&it));
  assert(mulib_mkir_events_next(&it,&ev)==1&&ev.node==root);
  assert(mulib_mkir_tree_attr(&t,elem,"key","value"));
  assert(mulib_mkir_events_next(&it,&ev)==-1&&ev.node==root);
  mulib_mkir_tree_free(&t);
  assert(!live);
}

static void test_deep_tree(void){
  enum { NODE_COUNT=24000 };
  mulib_mkir_tree_t t={0},copy={0};
  mulib_mkir_events_t it;
  mulib_mkir_event_t ev;
  size_t p=MULIB_MKIR_NONE;
  for(size_t i=0;i<NODE_COUNT;i++)p=add(&t,p,MULIB_MKIR_TEXT,NULL);
  assert(mulib_mkir_tree_validate(&t));
  assert(mulib_mkir_events_begin(&t,0,&it));
  for(size_t i=0;i<NODE_COUNT;i++){
    assert(mulib_mkir_events_next(&it,&ev)==1&&ev.node==i&&ev.depth==i&&ev.kind==MULIB_MKIR_E_TEXT);
  }
  assert(!mulib_mkir_events_next(&it,&ev));
  assert(mulib_mkir_tree_ast(&t,&copy,0)&&copy.count==NODE_COUNT&&mulib_mkir_tree_validate(&copy));
  mulib_mkir_tree_free(&copy);mulib_mkir_tree_free(&t);
  assert(!live);
}

static void test_allocation_failures(void){
  static const char source[]="<p z='1' a='2'>abc<em value='v'/>x<!--end--></p>";
  size_t success=0;
  /* Exercise each allocation site until a complete CST build succeeds. */
  for(size_t i=0;i<128;i++){
    mulib_mkir_cst_t cst={0};
    size_t bad=SIZE_MAX;
    attempts=0;fail_at=i;
    if(mulib_mkir_cst_parse(source,sizeof(source)-1,0,&cst,&bad)){
      success=i;
      mulib_mkir_cst_free(&cst);break;
    }
    assert(bad<=sizeof(source)-1&&!cst.source&&!cst.tokens&&!cst.tree.nodes&&!cst.tree.attrs&&!live);
  }
  assert(success>5&&!live);
  {
    mulib_mkir_tree_t src={0};
    size_t baseline;
    uint64_t version;
    fail_at=SIZE_MAX;
    assert(mulib_mkir_parse(source,sizeof(source)-1,0,&src,NULL));
    baseline=live;version=src.version;success=0;
    for(size_t i=0;i<128;i++){
      mulib_mkir_tree_t copy={0};
      attempts=0;fail_at=i;
      if(mulib_mkir_tree_ast(&src,&copy,0)){
        success=i;
        mulib_mkir_tree_free(&copy);break;
      }
      assert(!copy.nodes&&!copy.attrs&&live==baseline&&src.version==version);
    }
    assert(success>5&&live==baseline);
    success=0;
    for(size_t i=0;i<128;i++){
      char *sxml;
      attempts=0;fail_at=i;
      sxml=mulib_mkir_tree_sxml(&src,0);
      if(sxml){success=i;test_free(sxml);break;}
      assert(live==baseline&&src.version==version);
    }
    assert(success>3&&live==baseline);
    attempts=0;fail_at=0;
    assert(!mulib_mkir_tree_value(&src,2,"replacement",11));
    assert(src.nodes[2].value_length==3&&!strcmp(src.nodes[2].value,"abc"));
    attempts=0;
    assert(!mulib_mkir_tree_attr(&src,1,"z","replacement"));
    assert(!strcmp(mulib_mkir_tree_get_attr(&src,1,"z"),"1")&&src.version==version);
    fail_at=SIZE_MAX;
    mulib_mkir_tree_free(&src);
  }
  for(size_t i=0;i<3;i++){
    mulib_mkir_tree_t t={0};
    size_t id=42;
    attempts=0;fail_at=i;
    assert(!mulib_mkir_tree_add(&t,MULIB_MKIR_NONE,MULIB_MKIR_ELEMENT,"tag","data",4,0,4,&id));
    assert(id==42&&!t.count&&!t.nodes&&!t.capacity&&!t.version&&!live);
    fail_at=SIZE_MAX;
    id=add(&t,MULIB_MKIR_NONE,MULIB_MKIR_ELEMENT,"tag");
    attempts=0;fail_at=i;
    assert(!mulib_mkir_tree_attr(&t,id,"key","value"));
    assert(!t.attr_count&&!t.attr_capacity&&!t.attrs&&t.version==1);
    mulib_mkir_tree_free(&t);
  }
  fail_at=SIZE_MAX;
  assert(!live);
}

int main(void){
  test_topology();
  test_validation();
  test_sxml_and_ast();
  test_lossless_cst();
  test_lexical_failures();
  test_attribute_order();
  test_events();
  test_deep_tree();
  test_allocation_failures();
  return 0;
}
