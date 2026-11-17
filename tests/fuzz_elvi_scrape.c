/* libFuzzer: length-delimited DSL, HTML and URL/redirect paths, no network. */
#include "fixtures/elvi-scrape-setup.h"
#include <assert.h>
static void elvi_fuzz_parse_(const mulib_elvi_scrape_program_t *p,const char *html,size_t size) {
  mulib_elvi_scrape_result_t results[8],before[8];size_t count=123;
  memset(results,0xa5,sizeof results);memcpy(before,results,sizeof results);
  if(mulib_elvi_scrape_parse(p,html,size,results,8,&count)) {
    assert(count<=8 && count<=p->max_results);
    for(size_t i=0;i<count;i++) {
      char host[256];assert(melvi_host_(results[i].url,host,sizeof host));
      assert(mulib_utf8_validate((const unsigned char *)results[i].title,strlen(results[i].title)));
      for(size_t j=0;j<i;j++)assert(strcmp(results[i].url,results[j].url));
    }
  } else assert(count==123 && !memcmp(results,before,sizeof results));
}
int LLVMFuzzerTestOneInput(const unsigned char *data,size_t size) {
  const char *dsl="elvi fixture\nhost example.org\nhome https://example.org/\n"
    "search https://example.org/search?q={query}&n={limit}\nanchor child h3\nrewrite google\n";
  mulib_elvi_scrape_program_t p={0},candidate={0},before={0};
  char *text;char url[8192];
  if(size>65536)return 0;
  assert(mulib_elvi_scrape_compile(dsl,strlen(dsl),&p,NULL));
  if(mulib_elvi_scrape_compile((const char *)data,size,&candidate,NULL)) {
    (void)mulib_elvi_scrape_url(&candidate,"&\"\\",1,url,sizeof url);
    elvi_fuzz_parse_(&candidate,(const char *)data,size);
  } else assert(!memcmp(&candidate,&before,sizeof candidate));
  elvi_fuzz_parse_(&p,(const char *)data,size);
  p.anchor_mode=MULIB_ELVI_ANCHOR_PARENT;elvi_fuzz_parse_(&p,(const char *)data,size);
  p.anchor_mode=MULIB_ELVI_ANCHOR_ANY;elvi_fuzz_parse_(&p,(const char *)data,size);
  text=malloc(size+1);if(!text)return 0;
  memcpy(text,data,size);text[size]=0;
  if(melvi_redirect_(&p,text,url,sizeof url)) {
    char host[256];assert(melvi_host_(url,host,sizeof host));
  }
  if(size<4096) {
    char wrapped[8192];
    int n=snprintf(wrapped,sizeof wrapped,"<a href='%s'><h3>fixture</h3></a>",text);
    assert(n>0 && (size_t)n<sizeof wrapped);elvi_fuzz_parse_(&p,wrapped,(size_t)n);
  }
  (void)mulib_elvi_scrape_url(&p,text,1,url,sizeof url);
  free(text);return 0;
}
