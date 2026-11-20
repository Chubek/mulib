#include "fixtures/elvi-scrape-setup.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Synthetic responses exercise only the documented extraction rules. */
static const struct provider {
  const char *file, *name, *home, *url, *sample;
  const char *type; /* NULL for inline legacy HTML; otherwise sample is a fixture filename. */
  struct expected { const char *title, *url; } hits[2];
} providers[] = {
  {
    "Google.elvs", "google", "https://www.google.com/",
    "https://www.google.com/search?q=caf%C3%A9%20%26%20C%2B%2B%2F%23%3F&num=7",
    "<a href='https://example.org/navigation'>Plain link</a>"
    "<a href='https://images.google.com/'><h3>Internal</h3></a>"
    "<a href='https://lh3.googleusercontent.com/'><h3>Thumbnail</h3></a>"
    "<a href='/url?q=https%3A%2F%2Fexample.org%2Fguide&amp;sa=U'>"
    "Before<div><h3>Google &amp; tools</h3></div>After</a>",
    NULL, {{"Google & tools", "https://example.org/guide"}}
  },
  {
    "Bing.elvs", "bing", "https://www.bing.com/",
    "https://www.bing.com/search?q=caf%C3%A9%20%26%20C%2B%2B%2F%23%3F&go=&form=QBLH&filt=all&count=7",
    "<a href='https://example.org/navigation'>Plain link</a>"
    "<h2><a href='https://www.bing.com/ck/a?u=opaque'>Tracking link</a></h2>"
    "<h2><a href='http://example.org/insecure'>Insecure link</a></h2>"
    "<h2><a href='https://example.org/guide'><em>Bing</em> &amp; tools</a></h2>",
    NULL, {{"Bing & tools", "https://example.org/guide"}}
  },
  {
    "DuckDuckGo.elvs", "duckduckgo", "https://lite.duckduckgo.com/lite/",
    "https://lite.duckduckgo.com/lite/?q=caf%C3%A9%20%26%20C%2B%2B%2F%23%3F&kg=g&kd=-1",
    "<a href='https://example.org/navigation'>Plain link</a>"
    "<table><tr><td><a href='/l/?uddg=https%3A%2F%2Fexample.org'>Wrapper</a></td></tr>"
    "<tr><td><a href='https://duckduckgo.com/about'>Internal</a></td></tr>"
    "<tr><td><a href='https://example.org/guide' class='result-link'>Lite &amp; tools</a></td></tr></table>",
    NULL, {{"Lite & tools", "https://example.org/guide"}}
  },
  {
    "Scholar.elvs", "scholar", "https://scholar.google.com/",
    "https://scholar.google.com/scholar?q=caf%C3%A9%20%26%20C%2B%2B%2F%23%3F",
    "<h3 class='gs_rt'>Unlinked citation</h3>"
    "<a href='https://example.org/navigation'>Plain link</a>"
    "<h3><a href='https://scholar.google.com/citations'>Internal</a></h3>"
    "<h3 class='gs_rt'><span>[PDF]</span> <a href='https://example.org/paper'>Scholar &amp; tools</a></h3>",
    NULL, {{"Scholar & tools", "https://example.org/paper"}}
  },
  {
    "MDN.elvs", "mdn", "https://www.google.com/",
    "https://www.google.com/search?q=site%3Adeveloper.mozilla.org%20caf%C3%A9%20%26%20C%2B%2B%2F%23%3F&num=7",
    "<a href='https://developer.mozilla.org/en-US/'>Plain link</a>"
    "<a href='https://accounts.google.com/'><h3>Internal</h3></a>"
    "<a href='https://www.google.com/url?sa=U&amp;url="
    "https%3A%2F%2Fdeveloper.mozilla.org%2Fen-US%2Fdocs%2FWeb'><h3>MDN &amp; tools</h3></a>",
    NULL, {{"MDN & tools", "https://developer.mozilla.org/en-US/docs/Web"}}
  },
  {
    "Crossref.elvs", "crossref", "https://api.crossref.org/",
    "https://api.crossref.org/works?query=caf%C3%A9%20%26%20C%2B%2B%2F%23%3F&rows=7&select=title,URL",
    "crossref.json", "application/json",
    {{"Caf\xc3\xa9 & Tools", "https://doi.org/10.5555/cafe"},
     {"Second Work", "https://doi.org/10.5555/second"}}
  },
  {
    "FDroid.elvs", "fdroid", "https://search.f-droid.org/",
    "https://search.f-droid.org/?q=caf%C3%A9%20%26%20C%2B%2B%2F%23%3F&page=1&lang=",
    "fdroid.html", "text/html; charset=UTF-8",
    {{"Reader & Notes", "https://f-droid.org/packages/org.example.reader/"},
     {"Simple Browser", "https://f-droid.org/packages/org.example.browser/"}}
  },
  {
    "Hex.elvs", "hex", "https://hex.pm/",
    "https://hex.pm/api/packages/?search=caf%C3%A9%20%26%20C%2B%2B%2F%23%3F&page=1&per_page=7&sort=recent_downloads",
    "hex.json", "application/json",
    {{"demo_http", "https://hex.pm/packages/demo_http"},
     {"demo_json", "https://hex.pm/packages/demo_json"}}
  },
  {
    "LibRS.elvs", "librs", "https://lib.rs/",
    "https://lib.rs/search?q=caf%C3%A9%20%26%20C%2B%2B%2F%23%3F",
    "lib-rs.html", "text/html",
    {{"Example & HTTP", "https://lib.rs/crates/example-http"},
     {"Example JSON", "https://lib.rs/crates/example-json"}}
  },
  {
    "Packagist.elvs", "packagist", "https://packagist.org/",
    "https://packagist.org/search.json?q=caf%C3%A9%20%26%20C%2B%2B%2F%23%3F&page=1&per_page=7",
    "packagist.json", "application/json",
    {{"example/http", "https://packagist.org/packages/example/http"},
     {"example/json", "https://packagist.org/packages/example/json"}}
  },
  {
    "OpenLibrary.elvs", "openlibrary", "https://openlibrary.org/",
    "https://openlibrary.org/search.json?q=caf%C3%A9%20%26%20C%2B%2B%2F%23%3F&page=1&limit=7&fields=key,title",
    "openlibrary.json", "application/json; charset=utf-8",
    {{"Caf\xc3\xa9 at the Library", "https://openlibrary.org/works/OL112W"},
     {"Another Book", "https://openlibrary.org/works/OL113W"}}
  }
};

static void test_path(char *out, size_t cap, const char *root,
                      const char *dir, const char *name) {
  int n=snprintf(out,cap,"%s/%s/%s",root,dir,name);
  assert(n>0 && (size_t)n<cap);
}

static size_t read_fixture(const char *root, const char *name, char *buf, size_t cap) {
  char path[4096];
  test_path(path,sizeof path,root,"tests/fixtures/elvi-scrape",name);
  FILE *file=fopen(path,"rb");
  if(!file) {
    perror(path);
    return 0;
  }
  size_t n=fread(buf,1,cap,file);
  int ok=feof(file) && !ferror(file) && n>0;
  if(fclose(file)!=0) ok=0;
  if(!ok) fprintf(stderr,"%s: empty, unreadable, or too large\n",path);
  return ok?n:0;
}

static int check_hits(const struct provider *provider, const mulib_elvi_scrape_result_t *results,
                      size_t count, size_t expected) {
  if(count!=expected) {
    fprintf(stderr,"%s: expected %zu results, got %zu\n",provider->file,expected,count);
    return 0;
  }
  for(size_t i=0;i<count;i++) {
    const struct expected *hit=&provider->hits[i];
    if(strcmp(results[i].title,hit->title) || strcmp(results[i].url,hit->url)) {
      fprintf(stderr,"%s: expected \"%s\" <%s>, got \"%s\" <%s>\n",
              provider->file,hit->title,hit->url,results[i].title,results[i].url);
      return 0;
    }
  }
  return 1;
}

struct fixture_response {
  const char *url, *type, *body;
  size_t length, calls;
};

static int fixture_transport(void *ctx, const char *url, mulib_bounded_text_buffer_t *body,
                            unsigned *status, char *type, size_t cap) {
  struct fixture_response *fixture=ctx;
  if(++fixture->calls!=1 || strcmp(url,fixture->url) || strlen(fixture->type)>=cap)
    return 0;
  strcpy(type,fixture->type);
  *status=200;
  return mulib_bounded_text_append(body,fixture->body,fixture->length);
}

int main(int argc, char **argv) {
  const char *root=argc>1?argv[1]:".";
  const char query[]="caf\xc3\xa9 & C++/#?";
  for(size_t i=0;i<sizeof providers/sizeof *providers;i++) {
    const struct provider *provider=&providers[i];
    char path[4096],url[8192],fixture[4096];
    mulib_elvi_scrape_program_t p;
    mulib_elvi_scrape_error_t error;
    mulib_elvi_scrape_result_t results[4];
    size_t count;
    test_path(path,sizeof path,root,"library/elvis",provider->file);
    if(!mulib_elvi_scrape_load(path,&p,&error)) {
      fprintf(stderr,"%s:%zu: %s\n",path,error.line,error.message);
      return 1;
    }
    assert(!strcmp(p.name,provider->name) && !strcmp(p.home,provider->home));
    assert(mulib_elvi_scrape_url(&p,"",7,url,sizeof url));
    assert(!strcmp(url,provider->home));
    assert(p.max_results==30);
    assert(!mulib_elvi_scrape_url(&p,query,0,url,sizeof url));
    assert(!mulib_elvi_scrape_url(&p,query,31,url,sizeof url));
    assert(mulib_elvi_scrape_url(&p,query,7,url,sizeof url));
    if(strcmp(url,provider->url)) {
      fprintf(stderr,"%s: expected URL:\n%s\nActual URL:\n%s\n",path,provider->url,url);
      return 1;
    }
    const char *body=provider->type?fixture:provider->sample;
    size_t length=provider->type?read_fixture(root,provider->sample,fixture,sizeof fixture):strlen(body);
    if(!length) return 1;
    if(!mulib_elvi_scrape_parse(&p,body,length,results,sizeof results/sizeof *results,&count)) {
      fprintf(stderr,"%s: failed to parse sample\n",path);
      return 1;
    }
    if(!check_hits(provider,results,count,provider->hits[1].title?2:1)) return 1;
    if(provider->type) {
      struct fixture_response response={provider->url,provider->type,body,length,0};
      mulib_fetch_client_t client={0};
      unsigned status=0;
      client.policy.allow_https=1;
      client.transport=fixture_transport;
      client.ctx=&response;
      if(!mulib_elvi_scrape_search(&p,&client,query,7,results,1,&count,&status) ||
         status!=200 || response.calls!=1) {
        fprintf(stderr,"%s: offline search failed (status %u, calls %zu)\n",
                path,status,response.calls);
        return 1;
      }
      if(!check_hits(provider,results,count,1)) return 1;
    }
  }
  puts("ElviScrape providers: OK");
  return 0;
}
