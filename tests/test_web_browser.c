#include "fixtures/web_primitives_impl.h"
#include <assert.h>
#include <fenv.h>
#define MULIB_IMPL_WEBSEARCH
#include "mulib-websearch.h"

typedef struct server
{
  const char *html, *type;
  unsigned status, calls;
  int fail;
} server_t;
static int
transport (void *p, const char *url, mulib_shylock_buffer_t *out,
           unsigned *status, char *type, size_t cap)
{
  server_t *s = p;
  assert (url);
  s->calls++;
  *status = s->status;
  assert (mulib_shylock_copy (type, cap, s->type));
  int ok = mulib_shylock_text (out, s->html);
  return ok && !s->fail;
}
static mulib_browser_fetch_t
fetcher (server_t *s)
{
  mulib_browser_fetch_t f = { 0 };
  f.client.transport = transport;
  f.client.ctx = s;
  f.client.policy.allow_https = 1;
  f.client.policy.allow_http = 1;
  assert (mulib_browser_origin_parse ("https://example.com", &f.initiator));
  f.enforce_same_origin = 1;
  return f;
}
static int
observer (void *p, const mulib_browser_event_t *e)
{
  assert (e->sequence && e->channel >= 1 && e->channel <= 5);
  (*(size_t *)p)++;
  return 1;
}
static int
rejecting (void *p, const mulib_browser_event_t *e)
{
  (void)p;
  (void)e;
  return 0;
}
static int
reentrant (void *p, const mulib_browser_event_t *e)
{
  mulib_browser_stream_t *s = p;
  (void)e;
  assert (!mulib_browser_stream_emit (s, MULIB_BROWSER_CHANNEL_TOKEN, 1, 0,
                                      NULL, 0));
  mulib_browser_stream_free (s);
  assert (s->dispatching);
  return 1;
}
static void
increment (void *p)
{
  (*(unsigned *)p)++;
}
typedef struct repost
{
  mulib_microjs_microtasks_t *q;
  unsigned count;
} repost_t;
static void
repost (void *p)
{
  repost_t *s = p;
  s->count++;
  assert (!mulib_microjs_microtask_drain (s->q, 100));
  mulib_microjs_microtask_free (s->q);
  assert (s->q->draining);
  if (s->count < 5)
    assert (mulib_microjs_microtask_post (s->q, repost, p));
}
static int
js (void *ctx, mulib_jsdom_op_t op, uint64_t id, const char *property,
    const char *value, size_t n, char *out, size_t cap)
{
  (void)ctx;
  (void)op;
  (void)id;
  (void)value;
  (void)n;
  assert (!strcmp (property, "name"));
  return mulib_shylock_copy (out, cap, "widget");
}

static void
origins_and_network (void)
{
  mulib_browser_origin_t a, b;
  assert (mulib_browser_origin_parse ("HTTPS://EXAMPLE.com.:443/path", &a));
  assert (mulib_browser_origin_parse ("https://example.com", &b)
          && mulib_browser_origin_equal (&a, &b));
  assert (mulib_browser_origin_parse ("https://[2001:db8:0:0::1]", &a));
  assert (mulib_browser_origin_parse ("https://[2001:db8::1]:443", &b)
          && mulib_browser_origin_equal (&a, &b));
  const char *bad[]
      = { "https://",
          "https://user@example.com",
          "https://example.com:65536",
          "https://example.com:9999999999999999999999999999999999999",
          "https://[]/",
          "https://foo..com",
          "https://evil\\@example.com",
          "https://%6cocalhost",
          "https://127.1",
          "https://0177.0.0.1",
          "https://0x7f000001",
          "https://example.com\x01" };
  for (size_t i = 0; i < sizeof bad / sizeof *bad; i++)
    assert (!mulib_browser_origin_parse (bad[i], &a));
  mulib_fetch_policy_t policy = { 0 };
  policy.allow_https = 1;
  const char *private_urls[] = {
    "https://localhost.",  "https://foo.localhost.", "https://foo.local.",
    "https://127.0.0.1",   "https://10.1.1.1",       "https://172.16.0.1",
    "https://192.168.1.1", "https://[::1]",          "https://100.64.0.1"
  };
  for (size_t i = 0; i < sizeof private_urls / sizeof *private_urls; i++)
    assert (!mulib_fetch_policy_check (&policy, private_urls[i]));
  assert (mulib_fetch_policy_check (&policy, "https://example.com"));
  policy.allow_private = 1;
  assert (mulib_fetch_policy_check (&policy, "https://[::1]:8443"));
  assert (!mulib_fetch_policy_check (&policy, "http://example.com"));
  server_t server = { "<p>hi</p>", "text/html; charset=utf-8", 200, 0, 0 };
  mulib_browser_fetch_t f = fetcher (&server);
  mulib_shylock_buffer_t body = { 0 };
  unsigned status = 99;
  char type[128] = "old";
  assert (!mulib_browser_fetch_get (&f, "https://other.com", &body, &status,
                                    type, sizeof type)
          && server.calls == 0);
  assert (mulib_browser_fetch_get (&f, "https://example.com/", &body, &status,
                                   type, sizeof type)
          && status == 200);
  size_t old = body.len;
  server.fail = 1;
  status = 99;
  strcpy (type, "old");
  assert (!mulib_fetch_get (&f.client, "https://example.com", &body, &status,
                            type, sizeof type)
          && body.len == old && status == 99 && !strcmp (type, "old"));
  server.fail = 0;
  f.client.policy.max_bytes = 2;
  assert (!mulib_fetch_get (&f.client, "https://example.com", &body, &status,
                            type, sizeof type));
  mulib_shylock_buffer_free (&body);
  assert (mulib_websearch_known (MULIB_WEBSEARCH_GOOGLE));
  mulib_websearch_provider_config_t provider;
  assert (mulib_websearch_provider_init (&provider, "search",
                                         "https://example.com/search", 10));
  assert (
      mulib_websearch_provider_url (&provider, "a&b c", 2, &body)
      && !strcmp (body.data, "https://example.com/search?q=a%26b+c&start=2"));
  assert (!mulib_websearch_provider_init (&provider, "x",
                                          "https://example.com/#frag", 1));
  mulib_shylock_buffer_free (&body);
  mulib_websearch_engine_t engine = { 0 };
  engine.limit = 2;
  assert (mulib_websearch_engine_add (&engine, "https://b.com", "b", 1, 1));
  assert (mulib_websearch_engine_add (&engine, "https://a.com", "a", 1, 2));
  assert (mulib_websearch_engine_add (&engine, "https://a.com", "a", 1, 4));
  assert (!mulib_websearch_engine_add (&engine, "https://c.com", "c", 1, 1));
  mulib_websearch_engine_sort (&engine);
  assert (!strcmp (engine.hits[0].url, "https://a.com")
          && engine.hits[0].providers == 6);
  mulib_websearch_engine_free (&engine);
  mulib_crawler_t crawler = { 0 };
  size_t id = 99;
  assert (mulib_crawler_enqueue (&crawler, "https://example.com", 10));
  assert (!mulib_crawler_next (&crawler, 9, &id) && id == 99);
  assert (mulib_crawler_next (&crawler, 10, &id));
  assert (!mulib_crawler_finish (&crawler, id, 0, 9, 3)
          && crawler.items[id].state == 1);
  assert (mulib_crawler_finish (&crawler, id, 0, 20, 3));
  assert (!mulib_crawler_next (&crawler, 19, &id));
  assert (mulib_crawler_next (&crawler, 20, &id));
  assert (mulib_crawler_finish (&crawler, id, 1, 0, 3));
  assert (!mulib_crawler_next (&crawler, 100, &id));
  mulib_crawler_free (&crawler);
}

static void
storage (void)
{
  mulib_browser_cookiejar_t jar = { 0 };
  jar.limit = 1;
  mulib_browser_cookie_t cookie
      = { "sid", "abc", "Example.COM", "/account", 100, 1, 0, 1 };
  assert (mulib_browser_cookiejar_put (&jar, &cookie));
  mulib_shylock_buffer_t b = { 0 };
  assert (mulib_browser_cookiejar_header (&jar, "www.EXAMPLE.com",
                                          "/accountant", 1, 10, &b)
          && b.len == 0);
  assert (mulib_browser_cookiejar_header (&jar, "www.example.com",
                                          "/account/x", 1, 10, &b)
          && !strcmp (b.data, "sid=abc"));
  mulib_shylock_buffer_free (&b);
  assert (mulib_browser_cookiejar_header (&jar, "badexample.com", "/account",
                                          1, 10, &b)
          && b.len == 0);
  assert (mulib_browser_cookiejar_header (&jar, "example.com", "/account", 0,
                                          10, &b)
          && b.len == 0);
  assert (mulib_browser_cookiejar_header (&jar, "example.com", "/account", 1,
                                          100, &b)
          && b.len == 0);
  cookie.name[0] = '=';
  assert (!mulib_browser_cookiejar_put (&jar, &cookie));
  mulib_browser_cookiejar_free (&jar);
  mulib_shylock_buffer_free (&b);
  mulib_browser_cache_t cache = { 0 };
  cache.max_bytes = 4;
  cache.max_entries = 2;
  assert (mulib_browser_cache_put (&cache, "a", "tag", 20, "abc", 3));
  assert (!mulib_browser_cache_put (&cache, "b", "", 20, "de", 2));
  const mulib_browser_cache_entry_t *entry
      = mulib_browser_cache_get (&cache, "a", 19);
  assert (entry && entry->body.len == 3);
  assert (
      mulib_browser_cache_put (&cache, "a", "new", 20, entry->body.data, 2));
  assert (mulib_browser_cache_put (&cache, "b", "", 30, "de", 2));
  assert (!mulib_browser_cache_get (&cache, "a", 20));
  assert (mulib_browser_cache_prune (&cache, 20) == 1 && cache.len == 1);
  mulib_browser_cache_free (&cache);
  mulib_browser_navigation_t nav = { 0 };
  nav.limit = 3;
  assert (mulib_browser_navigation_visit (&nav, "a"));
  assert (mulib_browser_navigation_visit (&nav, "b"));
  assert (mulib_browser_navigation_visit (&nav, "c"));
  assert (!strcmp (mulib_browser_navigation_move (&nav, -1), "b"));
  assert (mulib_browser_navigation_visit (&nav, nav.urls[0]));
  assert (nav.len == 3 && !strcmp (nav.urls[2], "a"));
  assert (!mulib_browser_navigation_move (&nav, 1));
  mulib_browser_navigation_free (&nav);
  mulib_browser_resources_t resources = { 0 };
  resources.limit = 1;
  size_t id;
  assert (mulib_browser_resource_register (&resources, "a", &id) && id == 0);
  assert (mulib_browser_resource_register (&resources, "a", &id));
  assert (!mulib_browser_resource_register (&resources, "b", &id));
  assert (
      mulib_browser_resource_complete (&resources, 0, 200, "text/html", 5));
  assert (
      !mulib_browser_resource_complete (&resources, 0, 200, "text/html", 6));
  mulib_browser_resources_free (&resources);
  mulib_browser_snapshot_t a = { 0 }, c = { 0 };
  assert (mulib_browser_snapshot_make (&a, "x\0y", 3, 1, 3));
  assert (mulib_browser_snapshot_make (&c, "x\0y", 3, 2, 3));
  assert (mulib_browser_snapshot_equal (&a, &c));
  c.bytes.data[0] = 'z';
  assert (!mulib_browser_snapshot_equal (&a, &c));
  mulib_browser_snapshot_free (&a);
  mulib_browser_snapshot_free (&c);
}

static void
runtime (void)
{
  mulib_browser_stream_t stream = { 0 };
  size_t count = 0;
  assert (!mulib_browser_stream_subscribe (&stream, 32, observer, &count));
  assert (mulib_browser_stream_subscribe (&stream, 31, observer, &count));
  assert (mulib_browser_stream_subscribe (&stream, 31, reentrant, &stream));
  assert (mulib_browser_tokenstream_emit (&stream, 1, 1, "x", 1));
  assert (mulib_browser_domstream_emit (&stream, 1, 1, NULL, 0));
  assert (mulib_browser_layoutstream_emit (&stream, 1, 1, NULL, 0));
  assert (mulib_browser_networkstream_emit (&stream, 200, 1, NULL, 0));
  assert (mulib_browser_executionstream_emit (&stream, 1, 1, NULL, 0));
  assert (count == 5);
  stream.sequence = UINT64_MAX;
  assert (!mulib_browser_tokenstream_emit (&stream, 1, 0, NULL, 0));
  mulib_browser_stream_free (&stream);
  mulib_microjs_microtasks_t q = { 0 };
  q.limit = 1;
  repost_t state = { &q, 0 };
  assert (mulib_microjs_microtask_post (&q, repost, &state));
  assert (mulib_microjs_microtask_drain (&q, 3) == 3 && state.count == 3);
  assert (mulib_microjs_microtask_drain (&q, 3) == 2 && q.len == 0);
  mulib_microjs_microtask_free (&q);
  mulib_microjs_promise_t promise = { 0 };
  assert (mulib_microjs_promise_settle (&promise, 0, &count));
  assert (!mulib_microjs_promise_settle (&promise, 1, NULL));
  mulib_browser_eventloop_t loop = { 0 };
  unsigned a = 0, b = 0;
  loop.max_tasks_per_turn = 1;
  assert (mulib_browser_eventloop_post (&loop, increment, &a, 0));
  assert (mulib_browser_eventloop_post (&loop, increment, &b, 1));
  assert (mulib_browser_eventloop_step (&loop) == 1 && a == 0 && b == 1);
  assert (mulib_browser_eventloop_step (&loop) == 1 && a == 1);
  mulib_browser_eventloop_free (&loop);
  mulib_microjs_modules_t modules = { 0 };
  modules.limit = 1;
  assert (mulib_microjs_module_register (&modules, "main", "export default 1",
                                         16));
  assert (!mulib_microjs_module_register (&modules, "main", "", 0));
  assert (!mulib_microjs_module_register (&modules, "bad", "\xff", 1));
  assert (mulib_microjs_module_find (&modules, "main")->source_length == 16);
  mulib_microjs_modules_free (&modules);
  mulib_jsdom_t bridge = { js, NULL };
  char out[32] = "old";
  assert (mulib_jsdom_invoke (&bridge, MULIB_JSDOM_GET, 1, "name", NULL, 0,
                              out, sizeof out)
          && !strcmp (out, "widget"));
  assert (!mulib_jsdom_invoke (&bridge, MULIB_JSDOM_GET, 1, "", NULL, 0, out,
                               sizeof out));
  mulib_browser_realm_t r1, r2;
  assert (mulib_browser_realm_init (&r1, 1, 1, "https://EXAMPLE.com", 0));
  assert (mulib_browser_realm_init (&r2, 2, 2, "https://example.com:443", 0));
  assert (mulib_browser_realm_same_origin (&r1, &r2));
  mulib_browser_limits_t limits;
  assert (mulib_browser_limits_default (&limits));
  mulib_browser_sandbox_t sandbox;
  assert (mulib_browser_sandbox_init (&sandbox, &limits));
  assert (mulib_browser_sandbox_charge (&sandbox, limits.max_response_bytes, 1,
                                        1, 1));
  assert (!mulib_browser_sandbox_charge (&sandbox, 1, 1, 1, 1)
          && sandbox.nodes == 1);
  mulib_browser_sandbox_cancel (&sandbox);
  assert (!mulib_browser_sandbox_charge (&sandbox, 0, 0, 0, 0));
  mulib_browser_permissions_t permissions = { 0 };
  assert (mulib_browser_permissions_grant (&permissions, 3));
  assert (mulib_browser_permissions_check (permissions, 3));
  assert (!mulib_browser_permissions_grant (&permissions, 32));
  mulib_browser_permissions_revoke (&permissions, 1);
  assert (!mulib_browser_permissions_check (permissions, 1));
  permissions.allowed = UINT32_MAX;
  assert (!mulib_browser_permissions_check (permissions, 32));
  mulib_dom_mutation_journal_t journal = { 0 };
  journal.limit = 1;
  assert (mulib_dom_mutation_push (&journal, MULIB_DOM_MUTATION_INSERT, 1, 0));
  assert (!mulib_dom_mutation_push (&journal, MULIB_DOM_MUTATION_TEXT, 1, 0));
  assert (journal.sequence == 1);
  mulib_dom_mutation_clear (&journal);
  mulib_dom_range_t range, shifted, other;
  assert (mulib_dom_range_make (1, 4, 8, 1, &range));
  assert (mulib_dom_range_shift (range, 2, 8, &shifted) && shifted.start == 3);
  assert (mulib_dom_range_intersect (range, shifted, &other)
          && other.start == 3 && other.end == 4);
  assert (!mulib_dom_range_shift (range, INT64_MIN, 8, &other));
  shifted.start = 8;
  assert (!mulib_dom_range_intersect (range, shifted, &other));
  mulib_shylock_buffer_t generated = { 0 };
  const char *idl = "/* interface Fake {}; */ interface Node { attribute "
                    "DOMString name; }; interface Window;";
  assert (mulib_webidl_bindgen_interfaces (idl, strlen (idl), &generated, 2));
  assert (!strstr (generated.data, "Fake"));
  size_t old = generated.len;
  assert (
      !mulib_webidl_bindgen_interfaces ("interface 123 {};", 17, &generated, 1)
      && generated.len == old);
  mulib_shylock_buffer_free (&generated);
  int32_t signed_value;
  uint32_t unsigned_value;
  assert (fesetround (FE_UPWARD) == 0);
  assert (mulib_webidl_to_int32 (2.5, 1, &signed_value) && signed_value == 2);
  assert (fesetround (FE_TONEAREST) == 0);
  assert (mulib_webidl_to_uint32 (-2, 1, &unsigned_value)
          && unsigned_value == 0);
  assert (!mulib_webidl_to_int32 (1.5, 0, &signed_value));
  assert (!mulib_webidl_domstring_utf8 ("\xed\xa0\x80", 3, &generated));
  assert (mulib_webidl_domstring_utf8 ("a\0b", 3, &generated));
  mulib_shylock_buffer_free (&generated);
}

static void
navigation (void)
{
  server_t server
      = { "<div><p>ok</p></div>", "Text/HTML; charset=utf-8", 200, 0, 0 };
  mulib_browser_fetch_t f = fetcher (&server);
  mulib_browser_limits_t limits;
  assert (mulib_browser_limits_default (&limits));
  limits.max_depth = 1;
  limits.max_urls = 1;
  mulib_browser_engine_t e;
  assert (mulib_browser_engine_init (&e, &f, &limits));
  unsigned status = 99;
  assert (!mulib_browser_engine_navigate (&e, "https://example.com", &status)
          && !e.document && status == 99);
  e.limits.max_depth = 3;
  assert (mulib_browser_stream_subscribe (&e.stream, 8, rejecting, NULL));
  assert (mulib_browser_engine_navigate (&e, "https://example.com", &status)
          && status == 200 && e.document);
  mulib_html_tree_node_t *saved = e.document;
  assert (
      !mulib_browser_engine_navigate (&e, "https://example.com/next", &status)
      && e.document == saved);
  server.type = "text/html-evil";
  mulib_html_tree_node_t *root = NULL;
  assert (!mulib_browser_documentload (&f, "https://example.com", 10, &root,
                                       &status));
  server.type = "text/html";
  server.html = "";
  assert (mulib_browser_documentload (&f, "https://example.com", 10, &root,
                                      &status));
  mulib_html_tree_free (root);
  mulib_browser_engine_destroy (&e);
}
int
main (void)
{
  origins_and_network ();
  storage ();
  runtime ();
  navigation ();
  return 0;
}
