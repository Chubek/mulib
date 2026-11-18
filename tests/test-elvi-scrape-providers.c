#include "fixtures/elvi-scrape-setup.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Synthetic result pages exercise only the documented extraction rules. */
static const struct {
  const char *file, *name, *home, *url, *html, *title, *result;
} providers[] = {
  {
    "Google.elvs", "google", "https://www.google.com/",
    "https://www.google.com/search?q=caf%C3%A9%20%26%20C%2B%2B%2F%23%3F&num=7",
    "<a href='https://example.org/navigation'>Plain link</a>"
    "<a href='https://images.google.com/'><h3>Internal</h3></a>"
    "<a href='https://lh3.googleusercontent.com/'><h3>Thumbnail</h3></a>"
    "<a href='/url?q=https%3A%2F%2Fexample.org%2Fguide&amp;sa=U'>"
    "Before<div><h3>Google &amp; tools</h3></div>After</a>",
    "Google & tools", "https://example.org/guide"
  },
  {
    "Bing.elvs", "bing", "https://www.bing.com/",
    "https://www.bing.com/search?q=caf%C3%A9%20%26%20C%2B%2B%2F%23%3F&go=&form=QBLH&filt=all&count=7",
    "<a href='https://example.org/navigation'>Plain link</a>"
    "<h2><a href='https://www.bing.com/ck/a?u=opaque'>Tracking link</a></h2>"
    "<h2><a href='http://example.org/insecure'>Insecure link</a></h2>"
    "<h2><a href='https://example.org/guide'><em>Bing</em> &amp; tools</a></h2>",
    "Bing & tools", "https://example.org/guide"
  },
  {
    "DuckDuckGo.elvs", "duckduckgo", "https://lite.duckduckgo.com/lite/",
    "https://lite.duckduckgo.com/lite/?q=caf%C3%A9%20%26%20C%2B%2B%2F%23%3F&kg=g&kd=-1",
    "<a href='https://example.org/navigation'>Plain link</a>"
    "<table><tr><td><a href='/l/?uddg=https%3A%2F%2Fexample.org'>Wrapper</a></td></tr>"
    "<tr><td><a href='https://duckduckgo.com/about'>Internal</a></td></tr>"
    "<tr><td><a href='https://example.org/guide' class='result-link'>Lite &amp; tools</a></td></tr></table>",
    "Lite & tools", "https://example.org/guide"
  },
  {
    "Scholar.elvs", "scholar", "https://scholar.google.com/",
    "https://scholar.google.com/scholar?q=caf%C3%A9%20%26%20C%2B%2B%2F%23%3F",
    "<h3 class='gs_rt'>Unlinked citation</h3>"
    "<a href='https://example.org/navigation'>Plain link</a>"
    "<h3><a href='https://scholar.google.com/citations'>Internal</a></h3>"
    "<h3 class='gs_rt'><span>[PDF]</span> <a href='https://example.org/paper'>Scholar &amp; tools</a></h3>",
    "Scholar & tools", "https://example.org/paper"
  },
  {
    "MDN.elvs", "mdn", "https://www.google.com/",
    "https://www.google.com/search?q=site%3Adeveloper.mozilla.org%20caf%C3%A9%20%26%20C%2B%2B%2F%23%3F&num=7",
    "<a href='https://developer.mozilla.org/en-US/'>Plain link</a>"
    "<a href='https://accounts.google.com/'><h3>Internal</h3></a>"
    "<a href='https://www.google.com/url?sa=U&amp;url="
    "https%3A%2F%2Fdeveloper.mozilla.org%2Fen-US%2Fdocs%2FWeb'><h3>MDN &amp; tools</h3></a>",
    "MDN & tools", "https://developer.mozilla.org/en-US/docs/Web"
  }
};

static void provider_path(char *out, size_t cap, const char *root, const char *name) {
  int n=snprintf(out,cap,"%s/library/elvis/%s",root,name);
  assert(n>0 && (size_t)n<cap);
}

int main(int argc, char **argv) {
  const char *root=argc>1?argv[1]:".";
  const char query[]="caf\xc3\xa9 & C++/#?";
  for(size_t i=0;i<sizeof providers/sizeof *providers;i++) {
    char path[4096],url[8192];
    mulib_elvi_scrape_program_t p;
    mulib_elvi_scrape_error_t error;
    mulib_elvi_scrape_result_t results[4];
    size_t count;
    provider_path(path,sizeof path,root,providers[i].file);
    if(!mulib_elvi_scrape_load(path,&p,&error)) {
      fprintf(stderr,"%s:%zu: %s\n",path,error.line,error.message);
      return 1;
    }
    assert(!strcmp(p.name,providers[i].name) && !strcmp(p.home,providers[i].home));
    assert(mulib_elvi_scrape_url(&p,"",7,url,sizeof url));
    assert(!strcmp(url,providers[i].home));
    assert(p.max_results==30);
    assert(!mulib_elvi_scrape_url(&p,query,0,url,sizeof url));
    assert(!mulib_elvi_scrape_url(&p,query,31,url,sizeof url));
    assert(mulib_elvi_scrape_url(&p,query,7,url,sizeof url));
    if(strcmp(url,providers[i].url)) {
      fprintf(stderr,"%s: expected URL:\n%s\nActual URL:\n%s\n",path,providers[i].url,url);
      return 1;
    }
    assert(mulib_elvi_scrape_parse(&p,providers[i].html,strlen(providers[i].html),
                                  results,sizeof results/sizeof *results,&count));
    if(count!=1 || strcmp(results[0].title,providers[i].title) ||
       strcmp(results[0].url,providers[i].result)) {
      fprintf(stderr,"%s: expected just \"%s\" <%s>, got %zu results\n",path,
              providers[i].title,providers[i].result,count);
      return 1;
    }
  }
  puts("ElviScrape providers: OK");
  return 0;
}
