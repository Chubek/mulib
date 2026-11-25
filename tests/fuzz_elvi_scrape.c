/* libFuzzer: length-delimited DSL, HTML/JSON and URL/redirect paths, no network. */
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
static void elvi_fuzz_script_(const unsigned char *data, size_t size) {
  static const char prefix[] =
    "elvi fixture\nhost example.org\nhome https://example.org/\nmode scrape\n"
    "artifacts discard\nxpath provide{mulib-xpath}\nforward ipc{/private/socket}\n"
    "logic lua{";
  char source[sizeof prefix + 4096], resolved[4096], before[4096];
  mulib_elvi_scrape_program_t program, initial;
  mulib_elvi_scrape_error_t error;
  const size_t start = sizeof prefix - 1;
  if (size > 4093) return;
  memcpy(source, prefix, start);
  memcpy(source + start, data, size);
  memcpy(source + start + size, "}\n", 3);
  memset(&program, 0xa5, sizeof program);
  memcpy(&initial, &program, sizeof initial);
  if (mulib_elvi_scrape_compile(source, start + size + 2, &program, &error)) {
    assert(program.mode == MULIB_ELVI_MODE_SCRAPE);
    assert(program.logic.kind == MULIB_ELVI_STAGE_LUA);
    assert(program.forward.kind == MULIB_ELVI_STAGE_IPC);
    assert(mulib_elvi_script_url_allowed(&program, "https://example.org/"));
    assert(!mulib_elvi_script_url_allowed(&program, "https://example.org.evil.invalid/"));
    assert(!mulib_elvi_script_url_allowed(&program, "https://example.org@evil.invalid/"));
    memset(resolved, 0xa5, sizeof resolved);
    memcpy(before, resolved, sizeof before);
    if (mulib_elvi_script_resolve(program.logic.argument, "/fixture", resolved,
                                  sizeof resolved))
      assert(memchr(resolved, 0, sizeof resolved));
    else
      assert(!memcmp(resolved, before, sizeof resolved));
  } else {
    assert(!memcmp(&program, &initial, sizeof program));
  }
}
int LLVMFuzzerTestOneInput(const unsigned char *data,size_t size) {
  const char *dsl="elvi fixture\nhost example.org\nhome https://example.org/\n"
    "search https://example.org/search?q={query}&n={limit}\nanchor child h3\nrewrite google\n";
  mulib_elvi_scrape_program_t p={0},candidate={0},before={0};
  char *text;char url[8192];
  if(size>65536)return 0;
  elvi_fuzz_script_(data,size);
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
  dsl="elvi fixture\nhost example.org\nhome https://example.org/\n"
    "search https://example.org/?q={query}\nformat json\nresults /\ntitle /title\nurl /url\nrewrite relative\n";
  assert(mulib_elvi_scrape_compile(dsl,strlen(dsl),&p,NULL));
  elvi_fuzz_parse_(&p,(const char *)data,size);
  free(text);return 0;
}
