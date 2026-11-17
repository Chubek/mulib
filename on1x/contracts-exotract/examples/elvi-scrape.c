/* Separate declaration-only consumer of the generated one-TU archive. */
#include "mulib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  const char *dsl="elvi fixture\nhost example.org\nhome https://example.org/\n"
    "search https://example.org/search?q={query}\nanchor child h3\n";
  const char *html="<a href='https://example.net/docs'><h3>C11 &amp; tools</h3></a>";
  mulib_elvi_scrape_program_t p;mulib_elvi_scrape_result_t result;size_t count;
  char url[8192];
  assert(mulib_elvi_scrape_compile(dsl,strlen(dsl),&p,NULL));
  assert(mulib_elvi_scrape_url(&p,"C11 tools",1,url,sizeof url));
  assert(!strcmp(url,"https://example.org/search?q=C11%20tools"));
  assert(mulib_elvi_scrape_parse(&p,html,strlen(html),&result,1,&count));
  assert(count==1 && !strcmp(result.title,"C11 & tools"));
  html="<template><a href='https://decoy.example/'><h3>Hidden</h3></a></template>"
    "<a href='http://example.net/'><h3>HTTP</h3></a>"
    "<a href='https://example.net/%00'><h3>NUL</h3></a>"
    "<a href='https://example.net/docs'><h3>Visible</h3></a>";
  assert(mulib_elvi_scrape_parse(&p,html,strlen(html),&result,1,&count));
  assert(count==1 && !strcmp(result.title,"Visible"));
  puts("elvi-scrape: archive OK");return 0;
}
