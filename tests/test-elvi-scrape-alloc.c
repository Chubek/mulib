/* Inject each allocation failure through actual MuLIB kernels, including TLS
 * lifecycle/HTTP parsing. No network and no third-party allocator. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static size_t allocation_calls,allocation_fail,allocation_live;
static int elvi_fail_(void) {return ++allocation_calls==allocation_fail;}
static void *elvi_malloc_(size_t n) {
  void *p;if(elvi_fail_())return NULL;p=malloc(n);if(p)allocation_live++;return p;
}
static void *elvi_calloc_(size_t n,size_t size) {
  void *p;if(elvi_fail_())return NULL;p=calloc(n,size);if(p)allocation_live++;return p;
}
static void elvi_free_(void *p) {if(p){assert(allocation_live);allocation_live--;}free(p);}
static void *elvi_realloc_(void *p,size_t n) {
  void *q;int had=p!=NULL;
  if(elvi_fail_())return NULL;
  if(!n){elvi_free_(p);return NULL;}
  q=realloc(p,n);if(q&&!had)allocation_live++;return q;
}
#define malloc elvi_malloc_
#define calloc elvi_calloc_
#define realloc elvi_realloc_
#define free elvi_free_
#define main elvi_https_allocation_fixture_main
#include "test-elvi-scrape-https.c"
#undef main

static const char elvi_dsl_[]="elvi fixture\nhost example.org\nhome https://example.org/\n"
  "search https://example.org/?q={query}&n={limit}\nanchor child h3\nrewrite google\n";
static const char elvi_html_[]="<a href='/url?q=https%3A%2F%2Fexample.net%2F'><h3>Safe &amp; local</h3></a>";
static const char elvi_json_dsl_[]="elvi fixture\nhost example.org\nhome https://example.org/\n"
  "search https://example.org/?q={query}\nformat json\nresults /\ntitle /label\nurl /href\n";
static const char elvi_json_[]="[{\"label\":\"Safe & local\",\"href\":\"https://example.net/\"}]";
static int elvi_fetch_(void *ctx,const char *url,mulib_bounded_text_buffer_t *body,
                       unsigned *status,char *type,size_t cap) {
  (void)ctx;(void)url;
  if(!mulib_bounded_text_text(body,elvi_html_) || cap<10)return 0;
  *status=200;memcpy(type,"text/html",10);return 1;
}
static void elvi_operation_(const mulib_elvi_scrape_program_t *p,int operation) {
  mulib_elvi_scrape_program_t out=*p;
  mulib_elvi_scrape_result_t result,original;
  mulib_elvi_scrape_error_t error;
  size_t count=123;int ok=0;char url[8192]="unchanged";
  memset(&result,0xa5,sizeof result);original=result;
  if(operation==0 || operation==1) {
    ok=operation==0?mulib_elvi_scrape_compile(elvi_dsl_,sizeof elvi_dsl_-1,&out,&error):
      mulib_elvi_scrape_load("elvi-allocation.elvs",&out,&error);
    if(!ok)assert(!memcmp(&out,p,sizeof out));
    else assert(!strcmp(out.host,"example.org"));
  } else if(operation==2) {
    ok=mulib_elvi_scrape_url(p,"a & b",1,url,sizeof url);
    if(!ok)assert(!strcmp(url,"unchanged"));
    else assert(strstr(url,"a%20%26%20b"));
  } else if(operation==3 || operation==4) {
    mulib_fetch_client_t client={{8192,2097152,0,1,0},elvi_fetch_,NULL};
    ok=operation==3?mulib_elvi_scrape_parse(p,elvi_html_,sizeof elvi_html_-1,&result,1,&count):
      mulib_elvi_scrape_search(p,&client,"test",1,&result,1,&count,NULL);
    if(!ok)assert(count==123 && !memcmp(&result,&original,sizeof result));
    else assert(count==1 && !strcmp(result.title,"Safe & local"));
  } else if(operation==5) {
    ok=mulib_elvi_scrape_parse(p,elvi_json_,sizeof elvi_json_-1,&result,1,&count);
    if(!ok)assert(count==123 && !memcmp(&result,&original,sizeof result));
    else assert(count==1 && !strcmp(result.title,"Safe & local"));
  } else {
    const char *wire="HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 2\r\n\r\nOK";
    fixture f={wire,0,0,0,0,0};mulib_elvi_scrape_https_t tls={connect_tls,peer_tls,&f};
    mulib_bounded_text_buffer_t body={0};char type[64]="unchanged";unsigned status=42;
    ok=mulib_elvi_scrape_https_transport(&tls,"https://example.org/",&body,&status,type,sizeof type);
    assert(f.closes==f.connects);
    if(!ok)assert(!body.data && status==42 && !strcmp(type,"unchanged"));
    else assert(status==200 && body.len==2 && !strcmp(body.data,"OK"));
    mulib_bounded_text_buffer_free(&body);
  }
  if(!allocation_fail)assert(ok);
}
int main(void) {
  mulib_elvi_scrape_program_t p,j;FILE *file;
  assert(mulib_elvi_scrape_compile(elvi_dsl_,sizeof elvi_dsl_-1,&p,NULL));
  assert(mulib_elvi_scrape_compile(elvi_json_dsl_,sizeof elvi_json_dsl_-1,&j,NULL));
  assert(!allocation_live);
  file=fopen("elvi-allocation.elvs","wb");assert(file);
  assert(fwrite(elvi_dsl_,1,sizeof elvi_dsl_-1,file)==sizeof elvi_dsl_-1 && !fclose(file));
  for(int operation=0;operation<7;operation++) {
    size_t allocations;allocation_calls=allocation_fail=0;
    const mulib_elvi_scrape_program_t *program=operation==5?&j:&p;
    elvi_operation_(program,operation);allocations=allocation_calls;assert(!allocation_live && allocations);
    for(size_t k=1;k<=allocations;k++) {
      allocation_calls=0;allocation_fail=k;elvi_operation_(program,operation);assert(!allocation_live);
    }
  }
  assert(!remove("elvi-allocation.elvs"));
  puts("elvi-scrape allocation failures: OK");return 0;
}
