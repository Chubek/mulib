#include "fixtures/elvi-scrape-setup.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned response_status=200, calls;
static const char *response_type="text/html; charset=utf-8";
static char *read_fixture(const char *root,const char *name) {
  char path[4096];FILE *f;long n;char *s;
  assert(snprintf(path,sizeof path,"%s/tests/fixtures/elvi-scrape/%s",root,name)>0);
  f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));n=ftell(f);
  assert(n>=0 && n<2097152);rewind(f);s=malloc((size_t)n+1);assert(s);
  assert(fread(s,1,(size_t)n,f)==(size_t)n);fclose(f);s[n]=0;return s;
}
static void provider_language(void) {
  static const char dsl[]="# Surfraw's no-terms case opens the provider home.\r\n"
    "elvi \"Offline provider\"\r\nhost Example.org\r\nhome https://example.org/start\r\n"
    "search \"https://EXAMPLE.ORG/search/{query}?count={limit}\"\t# comment\r\n"
    "anchor any\r\n";
  static const struct {
    const char *home, *search, *extra;
    size_t error_line;
  } invalid[]={
    {"https://example.net/", "https://example.org/?q={query}", "", 3},
    {"https://example.org/", "https://example.org/?q=missing", "", 4},
    {"https://example.org/", "https://example.org/?q={query}&n={unknown}", "", 4},
    {"https://example.org/", "https://example.org/%a{limit}?q={query}", "", 4},
    {"https://example.org/", "https://example.org/?q={query}", "exclude_host bad..host\n", 6},
    {"https://example.org/", "https://example.org/?q={query}", "host example.org\n", 6},
  };
  static const char variable_host[]="elvi x\nhost x.example.org\nhome https://x.example.org/\n"
    "search https://{query}.example.org/\nanchor any a\n";
  mulib_elvi_scrape_program_t p,before;mulib_elvi_scrape_error_t error;
  char url[8192],src[1024];
  assert(mulib_elvi_scrape_compile(dsl,sizeof dsl-1,&p,&error));
  assert(!error.line && !error.message[0] && !strcmp(p.name,"Offline provider"));
  assert(p.max_results==20 && p.anchor_mode==MULIB_ELVI_ANCHOR_ANY);
  assert(mulib_elvi_scrape_url(&p,"",20,url,sizeof url));
  assert(!strcmp(url,"https://example.org/start"));
  strcpy(url,"unchanged");
  assert(!mulib_elvi_scrape_url(&p,"",0,url,sizeof url) && !strcmp(url,"unchanged"));
  assert(!mulib_elvi_scrape_url(&p,"",1,url,2) && !strcmp(url,"unchanged"));
  assert(mulib_elvi_scrape_url(&p,"C++ / \xc3\xa9",2,url,sizeof url));
  assert(!strcmp(url,"https://EXAMPLE.ORG/search/C%2B%2B%20%2F%20%C3%A9?count=2"));
  before=p;
  assert(!mulib_elvi_scrape_compile(variable_host,sizeof variable_host-1,&p,&error));
  assert(error.line==4 && !memcmp(&p,&before,sizeof p));
  for(size_t i=0;i<sizeof invalid/sizeof *invalid;i++) {
    int len=snprintf(src,sizeof src,"elvi x\nhost example.org\nhome \"%s\"\n"
      "search \"%s\"\nanchor any a\n%s",invalid[i].home,invalid[i].search,invalid[i].extra);
    assert(len>0 && (size_t)len<sizeof src);
    assert(!mulib_elvi_scrape_compile(src,(size_t)len,&p,&error));
    assert(error.line==invalid[i].error_line && error.message[0]);
    assert(!memcmp(&p,&before,sizeof p));
  }
}

static void anchor_attributes(void) {
  static const char dsl[]="elvi offline\nhost example.org\nhome https://example.org/\n"
    "search https://example.org/?q={query}\nanchor child result-title\n";
  static const char html[]=
    "<a data-href='https://decoy.example/' HREF = https://example.net/a"
    " href='https://decoy.example/'><RESULT-TITLE>A</RESULT-TITLE></a>"
    "<a href href='https://decoy.example/'><result-title>Missing</result-title></a>"
    "<a href='' href='https://decoy.example/'><result-title>Empty</result-title></a>"
    "<a href='http://decoy.example/' href='https://decoy.example/'>"
    "<result-title>Forbidden</result-title></a>"
    "<a href='https://example.net/ends-with/'><result-title>B</result-title></a>";
  mulib_elvi_scrape_program_t p;mulib_elvi_scrape_result_t results[4];size_t count;
  assert(mulib_elvi_scrape_compile(dsl,sizeof dsl-1,&p,NULL));
  assert(mulib_elvi_scrape_parse(&p,html,sizeof html-1,results,4,&count) && count==2);
  assert(!strcmp(results[0].title,"A") && !strcmp(results[0].url,"https://example.net/a"));
  assert(!strcmp(results[1].title,"B") && !strcmp(results[1].url,"https://example.net/ends-with/"));
}

static void anchor_classes(void) {
  static const char dsl[]="elvi offline\nhost example.org\nhome https://example.org/\n"
    "search https://example.org/?q={query}\nanchor %s\nanchor_class Result\n";
  static const char html[]=
    "<section><a href='https://example.net/first' CLASS='other\tResult\fmore'>"
      "<h3>First</h3></a></section>"
    "<section><a class=Result href=https://example.net/last><h3>Last</h3></a></section>"
    "<section class=Result><a data-class=Result href='https://decoy.example/'>"
      "<h3 class=Result>Class on another element</h3></a></section>"
    "<section><a class='result NotResult Result-ish' href='https://decoy.example/'>"
      "<h3>Not a matching token</h3></a></section>"
    "<section><a class='other' class=Result href='https://decoy.example/'>"
      "<h3>Duplicate class</h3></a></section>"
    "<section><a class class=Result href='https://decoy.example/'>"
      "<h3>Missing first value</h3></a></section>"
    "<section><a class='' class=Result href='https://decoy.example/'>"
      "<h3>Empty first value</h3></a></section>"
    "<section><a class='other\xc2\xa0Result' href='https://decoy.example/'>"
      "<h3>NBSP is not an HTML class separator</h3></a></section>"
    "<a class=Result href='https://example.net/outside'><h3>Outside</h3></a>"
    "<section><a class=Result href='https://example.net/nochild'>No heading</a></section>";
  static const struct {const char *selector,*third;size_t count;} modes[]={
    {"any","Outside",4},{"child h3","Outside",3},{"parent section","No heading",3}
  };
  mulib_elvi_scrape_program_t p;mulib_elvi_scrape_result_t results[8];size_t count;
  char src[1024];
  for(size_t i=0;i<sizeof modes/sizeof *modes;i++) {
    int n=snprintf(src,sizeof src,dsl,modes[i].selector);
    assert(n>0 && (size_t)n<sizeof src);
    assert(mulib_elvi_scrape_compile(src,(size_t)n,&p,NULL));
    assert(mulib_elvi_scrape_parse(&p,html,sizeof html-1,results,8,&count));
    assert(count==modes[i].count);
    assert(!strcmp(results[0].title,"First") && !strcmp(results[0].url,"https://example.net/first"));
    assert(!strcmp(results[1].title,"Last") && !strcmp(results[1].url,"https://example.net/last"));
    assert(!strcmp(results[2].title,modes[i].third));
    if(count==4)assert(!strcmp(results[3].title,"No heading"));
  }
}

static void relative_links(void) {
  static const char dsl[]="elvi offline\nhost Example.org\nhome https://example.org/base/\n"
    "search https://example.org/search/?q={query}\nanchor any\nrewrite %s\n"
    "exclude_host blocked.example\n";
  static const char html[]="<base href='https://decoy.example/'>"
    "<a href='/wiki/Test?a=1&amp;b=%2F#part'>Relative</a>"
    "<a href='https://example.org/wiki/Test?a=1&amp;b=%2F#part'>Duplicate</a>"
    "<a href='&#47;from-entity'>Slash entity</a><a href='/'>Root</a>"
    "<a href='https://example.net/elsewhere'>External</a>"
    "<a href='//evil.example/x'>Network path</a>"
    "<a href='/&#x2f;evil.example/x'>Entity-encoded network path</a>"
    "<a href='/\\evil.example/x'>Backslash</a>"
    "<a href='/%5Cevil.example/x'>Escaped backslash</a>"
    "<a href='/bad%0Apath'>Escaped control</a>"
    "<a href='/bad%0'>Malformed escape</a>"
    "<a href='/bad&#10;path'>Entity-encoded control</a>"
    "<a href='path'>No leading slash</a><a href='../path'>Parent relative</a>"
    "<a href='?q=1'>Query only</a><a href='#part'>Fragment only</a>"
    "<a href='http://example.org/'>Insecure absolute</a>"
    "<a href='https://127.0.0.1/'>Private absolute</a>"
    "<a href='https://a.blocked.example/'>Excluded absolute</a>";
  mulib_elvi_scrape_program_t p;mulib_elvi_scrape_result_t results[8];size_t count;
  char src[1024];
  int n=snprintf(src,sizeof src,dsl,"relative");
  assert(n>0 && (size_t)n<sizeof src);
  assert(mulib_elvi_scrape_compile(src,(size_t)n,&p,NULL));
  assert(mulib_elvi_scrape_parse(&p,html,sizeof html-1,results,8,&count) && count==4);
  assert(!strcmp(results[0].title,"Relative"));
  assert(!strcmp(results[0].url,"https://example.org/wiki/Test?a=1&b=%2F#part"));
  assert(!strcmp(results[1].url,"https://example.org/from-entity"));
  assert(!strcmp(results[2].url,"https://example.org/"));
  assert(!strcmp(results[3].url,"https://example.net/elsewhere"));
  strcpy(p.excluded_hosts[p.excluded_count++],"example.org");
  assert(mulib_elvi_scrape_parse(&p,html,sizeof html-1,results,8,&count) && count==1);
  assert(!strcmp(results[0].url,"https://example.net/elsewhere"));
  n=snprintf(src,sizeof src,dsl,"direct");
  assert(n>0 && (size_t)n<sizeof src);
  assert(mulib_elvi_scrape_compile(src,(size_t)n,&p,NULL));
  assert(mulib_elvi_scrape_parse(&p,html,sizeof html-1,results,8,&count) && count==2);
  assert(!strcmp(results[0].title,"Duplicate") && !strcmp(results[1].title,"External"));
}

static void table_cell_context(void) {
  /* Lite pages can omit cell and row end tags. A nested table is still in
   * the outer cell, but a new cell/row in the same table ends that context. */
  static const char dsl[]="elvi lite\nhost example.org\nhome https://example.org/\n"
    "search https://example.org/?q={query}\nanchor parent td\n";
  static const char html[]="<table><tr><td><a href='https://example.net/1'>First</a>"
    "<table><tr><th><a href='https://example.net/2'>Nested</a></table>"
    "<a href='https://example.net/3'>Back in outer cell</a>"
    "<th><a href='https://decoy.example/'>Sibling header</a>"
    "<tr><td><a href='https://example.net/4'>Last</a></table>"
    "<a href='https://decoy.example/'>Outside the table</a>";
  mulib_elvi_scrape_program_t p;mulib_elvi_scrape_result_t results[8];size_t count;
  assert(mulib_elvi_scrape_compile(dsl,sizeof dsl-1,&p,NULL));
  assert(mulib_elvi_scrape_parse(&p,html,sizeof html-1,results,8,&count) && count==4);
  assert(!strcmp(results[0].title,"First") && !strcmp(results[1].title,"Nested"));
  assert(!strcmp(results[2].title,"Back in outer cell") && !strcmp(results[3].title,"Last"));
}

static void hardening(const char *root,const mulib_elvi_scrape_program_t *program) {
  mulib_elvi_scrape_program_t p=*program,before;mulib_elvi_scrape_error_t error;
  mulib_elvi_scrape_result_t results[8];char html[8192],url[8192];size_t count;
  const char *bad[]={"https://example.org/%", "https://example.org/%0", "https://example.org/%GG",
    "https://example.org/%00suffix", "https://example.org/%0a", "https://example.org/%5cfoo",
    "https://user@example.org/", "https://example.org\\@evil.test/", "http://example.org/",
    "https://127.0.0.1/", "https://10.1.2.3/", "https://172.16.1.1/", "https://192.168.0.1/",
    "https://169.254.169.254/", "https://localhost/", "https://x.local/", "https://2130706433/",
    "https://0x7f000001/", "https://[::1]/", "https://bad..example/", "https://-bad.example/",
    "/url?q=https%3A%2F%2F127.0.0.1%2F", "/url?q=https%3A%2F%2Fexample.org%2F%00tail",
    "/url?q=http%3A%2F%2Fexample.org%2F", "https://example.org/&#10;bad"};
  for(size_t i=0;i<sizeof bad/sizeof *bad;i++) {
    snprintf(html,sizeof html,"<a href='%s'><h3>Reject</h3></a>",bad[i]);
    assert(mulib_elvi_scrape_parse(&p,html,strlen(html),results,8,&count));assert(!count);
  }
  const char *names[]={"layout.html","malformed.html","consent.html","bot.html","changed.html",
    "inert.html","redirects.html"};
  const size_t expected[]={2,1,0,0,0,2,1};
  for(size_t i=0;i<sizeof names/sizeof *names;i++) {
    char *s=read_fixture(root,names[i]);
    assert(mulib_elvi_scrape_parse(&p,s,strlen(s),results,8,&count));if(count!=expected[i])fprintf(stderr,"fixture %s: %zu expected %zu\n",names[i],count,expected[i]);assert(count==expected[i]);
    if(!i) {assert(!strcmp(results[0].title,"C11 & \xf0\x9f\x9a\x80 caf\xc3\xa9"));
      assert(!strcmp(results[1].url,"https://example.net/guide"));}
    if(i==5) {assert(!strcmp(results[0].title,"Visible \xf0\x9f\x9a\x80 guide"));
      assert(!strcmp(results[1].title,"One two"));}
    if(i==6)assert(!strcmp(results[0].url,"https://example.org/safe?x=1&y=2"));
    free(s);
  }
  before=p;
  const char *invalid[]={"elvi x\n", "elvi \"bad\\n\"\n", "elvi \"unterminated\n",
    "elvi x extra\n", "shell echo\n", "max_results 9999999999999\n"};
  for(size_t i=0;i<sizeof invalid/sizeof *invalid;i++) {
    assert(!mulib_elvi_scrape_compile(invalid[i],strlen(invalid[i]),&p,&error));
    assert(!memcmp(&p,&before,sizeof p));assert(error.line);
  }
  assert(!mulib_elvi_scrape_compile("elvi\0x",6,&p,&error));
  assert(!mulib_elvi_scrape_compile("",65537,&p,&error));
  strcpy(url,"unchanged");assert(!mulib_elvi_scrape_url(&p,"query",1,url,2));assert(!strcmp(url,"unchanged"));
  assert(mulib_elvi_scrape_url(&p,"a &\"#%\xc3\xa9",1,url,sizeof url));
  assert(strstr(url,"a%20%26%22%23%25%C3%A9"));
  p.excluded_count=9;assert(!mulib_elvi_scrape_parse(&p,"",0,results,8,&count));p=before;
  memset(p.anchor_tag,'a',sizeof p.anchor_tag);assert(!mulib_elvi_scrape_parse(&p,"",0,results,8,&count));p=before;
  assert(!mulib_elvi_scrape_parse(&p,"x\0y",3,results,8,&count));
  /* UTF-8 clipping never leaves an incomplete code point. */
  strcpy(html,"<a href='https://example.org/'><h3>");
  size_t n=strlen(html);memset(html+n,'a',510);n+=510;
  strcpy(html+n,"\xf0\x9f\x9a\x80<em>Z</em></h3></a>");
  assert(mulib_elvi_scrape_parse(&p,html,strlen(html),results,8,&count));
  assert(count==1 && strlen(results[0].title)==510);
  assert(mulib_utf8_validate((unsigned char *)results[0].title,strlen(results[0].title)));
  /* A request fragment is never sent as a query by any transport. */
  strcpy(p.search_template,"https://www.google.com/#q={query}");
  strcpy(url,"unchanged");assert(!mulib_elvi_scrape_url(&p,"test",1,url,sizeof url));
  assert(!strcmp(url,"unchanged"));p=before;
  /* Scan budget is explicit, including raw-text scanning and malformed tags. */
  {const char *cursor="<a x='unterminated";mulib_webscan_tag_ tag;size_t work=4;
    assert(mulib_webscan_next_work_(&cursor,&tag,&work)==-1 && !work);}
  {const char *cursor="<script>lots of raw text</script>";mulib_webscan_tag_ tag;size_t work=20;
    assert(mulib_webscan_next_work_(&cursor,&tag,&work)==-1 && !work);}
  {mulib_webscan_tag_ tag;const char *value;size_t len,work=100;
    assert(mulib_webscan_tag_parse_("<a class='pre Target post'>",&tag));
    assert(mulib_webscan_attr_first_work_(&tag,"class",&value,&len,&work)==1);
    work=100-work; /* Sufficient for the attribute, but not its tokens. */
    assert(mulib_webscan_class_work_(&tag,"Target",&work)==-1 && !work);}
  /* Length-delimited input, arbitrary capacity, invalid programs and NUL all
   * leave caller storage untouched, including the count sentinel. */
  memset(results,0x5a,sizeof results);count=123;
  assert(!mulib_elvi_scrape_parse(&p,"\0",1,results,8,&count));assert(count==123);
  for(size_t i=0;i<sizeof results;i++)assert(((unsigned char *)results)[i]==0x5a);
  assert(mulib_elvi_scrape_parse(&p,NULL,0,NULL,0,&count) && count==0);
  /* Raw NBSP and numeric entities have the same whitespace behavior. */
  p.max_results=8;
  strcpy(html,"<a href='https://example.org/'><h3>A\xc2\xa0 B&#000000000000160; C</h3></a>");
  assert(mulib_elvi_scrape_parse(&p,html,strlen(html),results,SIZE_MAX,&count));
  assert(count==1 && !strcmp(results[0].title,"A B C"));
  p.anchor_mode=MULIB_ELVI_ANCHOR_PARENT;strcpy(p.anchor_tag,"h2");
  {char *s=read_fixture(root,"headings.html");
    assert(mulib_elvi_scrape_parse(&p,s,strlen(s),results,8,&count));assert(count==2);
    assert(!strcmp(results[0].title,"First") && !strcmp(results[1].title,"Second"));free(s);}
  /* No implicit following of pagination, including in all-anchor mode. */
  p.anchor_mode=MULIB_ELVI_ANCHOR_ANY;
  strcpy(html,"<a href='/search?q=x&amp;start=10'>Next</a><a href='https://example.org/'>Result</a>");
  assert(mulib_elvi_scrape_parse(&p,html,strlen(html),results,8,&count) && count==1);
  /* Exact maximum HTML extent and nonrecursive nesting remain offline/bounded. */
  {size_t bytes=2097152;char *large=malloc(bytes+1);assert(large);
    memset(large,' ',bytes);large[bytes]=0;
    assert(mulib_elvi_scrape_parse(&p,large,bytes,results,8,&count) && count==0);
    count=123;assert(!mulib_elvi_scrape_parse(&p,large,bytes+1,results,8,&count) && count==123);
    free(large);}
}
static const char *fixture,*expected_home;
static int mocked_transport(void*ctx,const char*url,mulib_bounded_text_buffer_t*b,
                            unsigned*status,char*type,size_t cap){
  (void)ctx;calls++;
  if(expected_home)assert(!strcmp(url,expected_home));
  else assert(strstr(url,"q=C11%20compilers")!=NULL);
  b->len=strlen(fixture);b->cap=b->len+1;
  b->data=(char*)malloc(b->cap);assert(b->data);
  memcpy(b->data,fixture,b->cap);
  *status=response_status;snprintf(type,cap,"%s",response_type);return 1;
}
int main(int argc,char **argv) {
  const char *root=argc>1?argv[1]:".";char path[4096];
  mulib_elvi_scrape_program_t p={0};mulib_elvi_scrape_error_t err={0};
  mulib_elvi_scrape_result_t out[10]={0};size_t count=0;char url[8192];
  provider_language();
  anchor_attributes();
  anchor_classes();
  relative_links();
  table_cell_context();
  snprintf(path,sizeof path,"%s/library/elvis/Google.elvs",root);
  assert(mulib_elvi_scrape_load(path,&p,&err));
  hardening(root,&p);
  assert(!strcmp(p.name,"google"));
  assert(mulib_elvi_scrape_url(&p,"C11 compilers",10,url,sizeof url));
  assert(!strcmp(url,"https://www.google.com/search?q=C11%20compilers&num=10"));
  assert(!mulib_elvi_scrape_url(&p,"abc",31,url,sizeof url));
  fixture="<html><script>var s='<a href=\"https://bad.example\"><h3>wrong</h3></a>';</script>"
    "<a href='/url?q=https%3A%2F%2Fexample.org%2Fdocs&amp;sa=U'><div><h3>C11 &amp; more</h3></div></a>"
    "<a href='https://example.net/guide'><h3>Other &#35; <!--invisible-->Guide</h3></a>"
    "<a href='https://www.google.com/settings'><h3>Internal</h3></a>"
    "<a href='https://example.org/docs'><h3>Duplicate</h3></a></html>";
  assert(mulib_elvi_scrape_parse(&p,fixture,strlen(fixture),out,10,&count));
  assert(count==2);
  assert(!strcmp(out[0].url,"https://example.org/docs"));
  assert(!strcmp(out[0].title,"C11 & more"));
  assert(!strcmp(out[1].title,"Other # Guide"));
  mulib_fetch_client_t client={0};client.transport=mocked_transport;client.policy.allow_https=1;
  assert(mulib_elvi_scrape_search(&p,&client,"C11 compilers",10,out,10,&count,NULL));
  assert(count==2);
  assert(mulib_elvi_scrape_search(&p,&client,"C11 compilers",1,out,10,&count,NULL));
  assert(count==1);
  expected_home=p.home;
  assert(mulib_elvi_scrape_search(&p,&client,"",1,out,10,&count,NULL) && count==1);
  expected_home=NULL;
  /* Never retry rate limits, follow redirects, or relax caller policy. */
  unsigned status=0;
  for(unsigned i=0;i<4;i++) {
    const unsigned codes[]={301,302,429,503};response_status=codes[i];unsigned old=calls;
    assert(!mulib_elvi_scrape_search(&p,&client,"C11 compilers",1,out,10,&count,&status));
    assert(status==codes[i] && calls==old+1);
  }
  response_status=200;response_type="text/html-evil";
  assert(!mulib_elvi_scrape_search(&p,&client,"C11 compilers",1,out,10,&count,NULL));
  response_type="Text/HTML; charset=utf-8";
  assert(mulib_elvi_scrape_search(&p,&client,"C11 compilers",1,out,10,&count,NULL));
  client.policy.allow_https=0;unsigned old=calls;
  assert(!mulib_elvi_scrape_search(&p,&client,"C11 compilers",1,out,10,&count,NULL));assert(old==calls);
  client.policy.allow_https=1;client.policy.max_url=10;
  assert(!mulib_elvi_scrape_search(&p,&client,"C11 compilers",1,out,10,&count,NULL));assert(old==calls);
  client.policy.max_url=0;client.policy.max_bytes=8;
  assert(!mulib_elvi_scrape_search(&p,&client,"C11 compilers",1,out,10,&count,NULL));
  snprintf(path,sizeof path,"%s/library/elvis/Bing.elvs",root);
  assert(mulib_elvi_scrape_load(path,&p,&err));
  fixture="<h2><a href='https://example.com/a'>Example <em>Bing</em> page</a></h2>"
    "<h2><a href='https://www.bing.com/account'>Internal</a></h2>"
    "<p><a href='https://ignored.example/'>not h2</a></p>";
  assert(mulib_elvi_scrape_parse(&p,fixture,strlen(fixture),out,10,&count));
  assert(count==1);
  assert(!strcmp(out[0].title,"Example Bing page"));
  assert(!strcmp(out[0].url,"https://example.com/a"));
  { const char bad[]="elvi x\nhost www.google.com\nhome https://www.google.com/\nsearch https://evil.test/?q={query}\nanchor child h3\n";
    assert(!mulib_elvi_scrape_compile(bad,strlen(bad),&p,&err));
    assert(err.line>0);
  }
  puts("elvi-scrape: OK");return 0;
}
