#include "runtime.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static const char source[] =
    "elvi fixture\nhost www.example.org\nhome https://www.example.org/\n"
    "search https://www.example.org/?q={query}&n={limit}\nanchor any\n";
static const char document[] =
    "elvi fixture\nhost www.example.org\nhome https://www.example.org/\n"
    "mode document\nartifacts capture\n";
static const char html[] =
    "<html><a href='https://www.example.org/a'>An &amp; A</a></html>";

typedef struct fixture {
  unsigned calls, status;
} fixture;

static int fetch(void *ctx, const char *url, mulib_bounded_text_buffer_t *body,
    unsigned *status, char *type, size_t capacity)
{
  fixture *f = ctx;
  ++f->calls;
  assert(!strcmp(url, "https://www.example.org/?q=hello%20there&n=1"));
  assert(capacity >= sizeof "Text/HTML; charset=utf-8");
  memcpy(type, "Text/HTML; charset=utf-8", sizeof "Text/HTML; charset=utf-8");
  *status = f->status;
  return mulib_bounded_text_append(body, html, sizeof html - 1);
}

int main(int argc, char **argv)
{
  assert(argc == 2);
  mulib_elvi_scrape_program_t p = {0};
  mulib_elvi_scrape_error_t error = {0};
  assert(!on1x_elvi_load(argv[1], "/wrong/path", &p, &error));
  assert(!p.name[0] && !error.line && error.message[0]);
  assert(on1x_elvi_load(argv[1], argv[1], &p, &error));
  assert(p.name[0] && !error.message[0]);
  assert(on1x_elvi_compile(source, sizeof source - 1, &p, &error));
  assert(p.mode == MULIB_ELVI_MODE_SEARCH && !strcmp(p.name, "fixture"));

  char nul_source[sizeof source];
  memcpy(nul_source, source, sizeof source);
  nul_source[15] = '\0';
  assert(!on1x_elvi_compile(nul_source, sizeof nul_source - 1, &p, &error));
  assert(!strcmp(p.name, "fixture"));
  assert(!on1x_elvi_compile(source, SIZE_MAX, &p, &error));
  assert(!strcmp(p.name, "fixture"));

  char url[ON1X_ELVI_URL_CAPACITY] = "unchanged";
  assert(!on1x_elvi_render_search(&p, "", 0, url, sizeof url));
  assert(!on1x_elvi_render_search(&p, "hello there", 1, url, 7));
  assert(!strcmp(url, "unchanged"));
  assert(on1x_elvi_render_search(&p, "", 1, url, sizeof url));
  assert(!strcmp(url, p.home));
  assert(on1x_elvi_render_search(&p, "hello there", 1, url, sizeof url));
  assert(!strcmp(url, "https://www.example.org/?q=hello%20there&n=1"));

  mulib_elvi_scrape_result_t hit = {"canary", "unchanged"};
  size_t count = 42;
  assert(!on1x_elvi_extract_search(&p, "<a>\0</a>", 8, &hit, 1, &count));
  assert(!on1x_elvi_extract_search(&p, html, SIZE_MAX, &hit, 1, &count));
  assert(count == 42 && !strcmp(hit.title, "canary"));
  assert(on1x_elvi_extract_search(&p, html, sizeof html - 1, &hit, 1, &count));
  assert(count == 1 && !strcmp(hit.title, "An & A"));

  fixture f = {0, 200};
  mulib_fetch_client_t client = {{0, 0, 1, 1, 1}, fetch, &f};
  unsigned status = 0;
  count = 44;
  assert(!on1x_elvi_search(&p, "www.example.org.", &client, "hello there", 1,
                           &hit, 1, &count, &status));
  assert(f.calls == 0 && count == 44 && status == 0);
  client.policy.allow_https = 0;
  assert(!on1x_elvi_search(&p, "www.example.org", &client, "hello there", 1,
                           &hit, 1, &count, &status));
  assert(f.calls == 0 && count == 44 && status == 0);
  client.policy.allow_https = 1;
  f.status = 301;
  assert(!on1x_elvi_search(&p, "www.example.org", &client, "hello there", 1,
                           &hit, 1, &count, &status));
  assert(f.calls == 1 && count == 44 && status == 301);
  f.status = 200;
  assert(on1x_elvi_search(&p, "www.example.org", &client, "hello there", 1,
                          &hit, 1, &count, &status));
  assert(f.calls == 2 && count == 1 && status == 200);

  assert(on1x_elvi_compile(document, sizeof document - 1, &p, &error));
  assert(p.mode == MULIB_ELVI_MODE_DOCUMENT);
  assert(p.artifacts.kind == MULIB_ELVI_STAGE_CAPTURE);
  assert(!on1x_elvi_search(&p, "www.example.org", &client, "hello there", 1,
                           &hit, 1, &count, &status));
  assert(f.calls == 2);
  assert(!on1x_elvi_resolve_stage(&p.artifacts, "/approved", url, sizeof url));
  mulib_elvi_stage_t path = {MULIB_ELVI_STAGE_LUA, "${HOME}/test.lua", 4};
  assert(!on1x_elvi_resolve_stage(&path, NULL, url, sizeof url));
  assert(!on1x_elvi_resolve_stage(&path, "relative", url, sizeof url));
  assert(on1x_elvi_resolve_stage(&path, "/approved/", url, sizeof url));
  assert(!strcmp(url, "/approved/test.lua"));
  return 0;
}
