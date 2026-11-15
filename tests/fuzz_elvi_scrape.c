/* libFuzzer: length-delimited DSL, HTML and URL/redirect paths, no network. */
#include "fixtures/elvi-scrape-setup.h"
#include <assert.h>
int LLVMFuzzerTestOneInput(const unsigned char *data,size_t size) {
  const char *dsl="elvi fixture\nhost example.org\nhome https://example.org/\n"
    "search https://example.org/search?q={query}&n={limit}\nanchor child h3\nrewrite google\n";
  mulib_elvi_scrape_program_t p={0},candidate={0};
  mulib_elvi_scrape_result_t results[8];size_t count=0;
  char *text;char url[8192];
  if(size>65536)return 0;
  assert(mulib_elvi_scrape_compile(dsl,strlen(dsl),&p,NULL));
  if(mulib_elvi_scrape_compile((const char *)data,size,&candidate,NULL)) {
    (void)mulib_elvi_scrape_url(&candidate,"&\"\\",1,url,sizeof url);
    (void)mulib_elvi_scrape_parse(&candidate,(const char *)data,size,results,8,&count);
  }
  (void)mulib_elvi_scrape_parse(&p,(const char *)data,size,results,8,&count);
  text=malloc(size+1);if(!text)return 0;
  memcpy(text,data,size);text[size]=0;
  if(melvi_redirect_(&p,text,url,sizeof url)) {
    char host[256];assert(melvi_host_(url,host,sizeof host));
  }
  (void)mulib_elvi_scrape_url(&p,text,1,url,sizeof url);
  free(text);return 0;
}
