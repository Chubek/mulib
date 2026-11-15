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
  const char *names[]={"layout.html","malformed.html","consent.html","bot.html","changed.html"};
  const size_t expected[]={2,1,0,0,0};
  for(size_t i=0;i<5;i++) {
    char *s=read_fixture(root,names[i]);
    assert(mulib_elvi_scrape_parse(&p,s,strlen(s),results,8,&count));if(count!=expected[i])fprintf(stderr,"fixture %s: %zu expected %zu\n",names[i],count,expected[i]);assert(count==expected[i]);
    if(!i) {assert(!strcmp(results[0].title,"C11 & \xf0\x9f\x9a\x80 caf\xc3\xa9"));
      assert(!strcmp(results[1].url,"https://example.net/guide"));}
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
  strcpy(html+n,"\xf0\x9f\x9a\x80</h3></a>");
  assert(mulib_elvi_scrape_parse(&p,html,strlen(html),results,8,&count));
  assert(count==1 && strlen(results[0].title)==510);
  assert(mulib_utf8_validate((unsigned char *)results[0].title,strlen(results[0].title)));
}
static const char *fixture;
static int mocked_transport(void*ctx,const char*url,mulib_shylock_buffer_t*b,
                            unsigned*status,char*type,size_t cap){
  (void)ctx;calls++;assert(strstr(url,"q=C11%20compilers")!=NULL);
  b->len=strlen(fixture);b->cap=b->len+1;
  b->data=(char*)malloc(b->cap);assert(b->data);
  memcpy(b->data,fixture,b->cap);
  *status=response_status;snprintf(type,cap,"%s",response_type);return 1;
}
int main(int argc,char **argv) {
  const char *root=argc>1?argv[1]:".";char path[4096];
  mulib_elvi_scrape_program_t p={0};mulib_elvi_scrape_error_t err={0};
  mulib_elvi_scrape_result_t out[10]={0};size_t count=0;char url[8192];
  snprintf(path,sizeof path,"%s/library/elvi-scrape/google.elvs",root);
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
  snprintf(path,sizeof path,"%s/library/elvi-scrape/bing.elvs",root);
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
