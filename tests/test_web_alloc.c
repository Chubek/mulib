#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef union allocation
{
  max_align_t align;
  size_t size;
} allocation_t;
static size_t attempts, fail_at, live;
static void *
moving_malloc (size_t n)
{
  if (++attempts == fail_at || n > SIZE_MAX - sizeof (allocation_t))
    return NULL;
  allocation_t *p = malloc (sizeof *p + n);
  if (!p)
    return NULL;
  p->size = n;
  live++;
  return p + 1;
}
static void
moving_free (void *p)
{
  if (p)
    {
      assert (live);
      live--;
      free ((allocation_t *)p - 1);
    }
}
static void *
moving_calloc (size_t n, size_t size)
{
  if (size && n > SIZE_MAX / size)
    return NULL;
  void *p = moving_malloc (n * size);
  if (p)
    memset (p, 0, n * size);
  return p;
}
static void *
moving_realloc (void *p, size_t size)
{
  if (!size)
    {
      moving_free (p);
      return NULL;
    }
  void *next = moving_malloc (size);
  if (!next)
    return NULL;
  if (p)
    {
      size_t old = ((allocation_t *)p - 1)->size;
      memcpy (next, p, old < size ? old : size);
      moving_free (p);
    }
  return next;
}
#define malloc moving_malloc
#define calloc moving_calloc
#define realloc moving_realloc
#define free moving_free
#include "fixtures/web_primitives_impl.h"
#undef malloc
#undef calloc
#undef realloc
#undef free

static void
workload (void)
{
  mulib_shylock_buffer_t b = { 0 };
  if (mulib_shylock_text (&b, "abcdef"))
    {
      for (int i = 0; i < 8; i++)
        {
          size_t old = b.len;
          int ok = mulib_shylock_append (&b, b.data, old);
          assert (ok ? b.len == old * 2 : b.len == old);
        }
      size_t old = b.len;
      int ok = mulib_shylock_json_string (&b, b.data, b.len);
      assert (ok || b.len == old);
    }
  mulib_shylock_buffer_free (&b);
  mulib_browser_cache_t cache = { 0 };
  if (mulib_browser_cache_put (&cache, "a", "", 20, "old", 3))
    {
      int ok = mulib_browser_cache_put (&cache, "a", "", 20, "newer", 5);
      const mulib_browser_cache_entry_t *e
          = mulib_browser_cache_get (&cache, "a", 0);
      assert (e && e->body.len == (ok ? 5u : 3u));
    }
  mulib_browser_cache_free (&cache);
  mulib_crawlarchive_t archive = { 0 };
  (void)mulib_crawlarchive_add (&archive, "a", 200, "body", 4);
  mulib_crawlarchive_free (&archive);
  mulib_microjs_modules_t modules = { 0 };
  (void)mulib_microjs_module_register (&modules, "main", "source", 6);
  mulib_microjs_modules_free (&modules);
  mulib_browser_navigation_t nav = { 0 };
  if (mulib_browser_navigation_visit (&nav, "a"))
    {
      (void)mulib_browser_navigation_visit (&nav, nav.urls[0]);
      assert (nav.len >= 1);
    }
  mulib_browser_navigation_free (&nav);
  const char *html
      = "<div class='a b'><p>text</p><script>x < y</script></div>";
  mulib_html_tree_node_t *root = NULL;
  (void)mulib_html_tree_parse (html, strlen (html), 20, 10, &root);
  mulib_html_tree_free (root);
  mulib_css_parsed_sheet_t sheet = { 0 };
  const char *css = "/*x*/ p {color: red} .a{width: 1px}";
  (void)mulib_css_parse_sheet (css, strlen (css), 10, &sheet);
  mulib_css_sheet_free (&sheet);
  (void)mulib_readability_extract (html, strlen (html), &b);
  mulib_webextract_rule_t rule = { ".b", 100 };
  (void)mulib_webextract_rules_text (html, strlen (html), &rule, &b);
  mulib_shylock_buffer_free (&b);
  (void)mulib_webextract_llm_request ("model", "text", "{}", 4, &b);
  mulib_shylock_buffer_free (&b);
  const char *json = "{\"price\":\"1.2\",\"priceCurrency\":\"USD\"}";
  mulib_schemaorg_offer_t offer;
  (void)mulib_schemaorg_offer_parse (json, strlen (json), &offer);
  const char *idl = "interface Node {};";
  (void)mulib_webidl_bindgen_interfaces (idl, strlen (idl), &b, 1);
  mulib_shylock_buffer_free (&b);
}
int
main (void)
{
  fail_at = 0;
  workload ();
  assert (live == 0);
  size_t count = attempts;
  for (size_t i = 1; i <= count; i++)
    {
      attempts = 0;
      fail_at = i;
      workload ();
      assert (live == 0);
    }
  return 0;
}
