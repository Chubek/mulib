/* Test-only ABI-exact scalar adapter; provider data is never executable code. */
#include "mulib-elvi-scrape.h"
#include <stdint.h>
#include <string.h>
int64_t fixture_elvi_provider(const char *source) {
  const char *html="<h2><a href='https://example.org/guide'>C11 &amp; tools</a></h2>"
    "<a href='https://example.org/guide'><h3>C11 &amp; tools</h3></a>";
  mulib_elvi_scrape_program_t p;mulib_elvi_scrape_result_t result;size_t count;
  char url[8192];
  if(!mulib_elvi_scrape_compile(source,strlen(source),&p,NULL) ||
     !mulib_elvi_scrape_url(&p,"C11 tools",1,url,sizeof url) ||
     !mulib_elvi_scrape_parse(&p,html,strlen(html),&result,1,&count))return 0;
  return count==1 && !strcmp(result.title,"C11 & tools") &&
    !strcmp(result.url,"https://example.org/guide") && strstr(url,"C11%20tools")!=NULL;
}
