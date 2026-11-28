/* Offline policy and accounting checks; no transport or content execution. */
#define MULIB_IMPL_AST
#define MULIB_IMPL_BROWSER_LIMITS
#define MULIB_IMPL_BROWSER_ORIGIN
#define MULIB_IMPL_BROWSER_PERMISSIONS
#define MULIB_IMPL_BROWSER_REALM
#define MULIB_IMPL_BROWSER_SANDBOX
#define MULIB_IMPL_CAPBROKER
#define MULIB_IMPL_GLOB
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INTE6
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_SANDBOX_PROFILE
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_URI
#include "mulib-browser-permissions.h"
#include "mulib-browser-realm.h"
#include "mulib-browser-sandbox.h"
#include "mulib-capbroker.h"
#include <assert.h>
#include <string.h>

static void
origin_and_realm (void)
{
  mulib_browser_origin_t a, b;
  assert (mulib_browser_origin_parse ("HTTPS://Example.test./a", &a));
  assert (mulib_browser_origin_parse ("https://example.test:443/b", &b));
  assert (mulib_browser_origin_equal (&a, &b));
  assert (mulib_browser_origin_parse ("https://example.test:444", &b));
  assert (!mulib_browser_origin_equal (&a, &b));
  assert (mulib_browser_origin_parse ("http://example.test:443", &b));
  assert (!mulib_browser_origin_equal (&a, &b));
  assert (mulib_browser_origin_parse ("http://192.0.2.1", &a));
  assert (mulib_browser_origin_equal (&a, &a));
  assert (!mulib_browser_origin_parse ("http://example.123", &a));
  assert (!mulib_browser_origin_parse ("http://example.0x2", &a));
  assert (!strcmp (a.host, "192.0.2.1") && a.port == 80);
  assert (mulib_browser_origin_parse ("http://example.0xyz", &b));
  assert (mulib_browser_origin_parse ("https://[2001:DB8:0:0::1]", &a));
  assert (mulib_browser_origin_parse ("https://[2001:db8::1]:443", &b));
  assert (mulib_browser_origin_equal (&a, &b));
  strcpy (b.host, "2001:0db8::1");
  assert (!mulib_browser_origin_equal (&b, &b));
  b = a;
  strcpy (b.scheme, "ftp");
  assert (!mulib_browser_origin_equal (&b, &b));
  b = a;
  memset (b.host, 'a', sizeof b.host);
  assert (!mulib_browser_origin_equal (&b, &b));

  char url[8194];
  const char prefix[] = "https://example.test/";
  memcpy (url, prefix, sizeof prefix - 1);
  memset (url + sizeof prefix - 1, 'a', sizeof url - sizeof prefix);
  url[8193] = '\0';
  assert (!mulib_browser_origin_parse (url, &b));
  url[8192] = '\0';
  assert (mulib_browser_origin_parse (url, &b));

  mulib_browser_realm_t r, s;
  assert (mulib_browser_realm_init (&r, 1, 1, "https://[2001:db8::1]/a", 0));
  assert (mulib_browser_realm_init (&s, 2, 2, "https://[2001:db8::1]:443", 0));
  assert (mulib_browser_realm_same_origin (&r, &s));
  s.context_id = 0;
  assert (!mulib_browser_realm_same_origin (&r, &s));
  s = r;
  strcpy (s.origin, "https://example.test:443/path");
  assert (!mulib_browser_realm_same_origin (&s, &s));
  strcpy (s.origin, "null");
  assert (!mulib_browser_realm_same_origin (&s, &s));
}

static void
limits_and_permissions (void)
{
  mulib_browser_limits_t limits = { 1024, 2, 1, 2, SIZE_MAX, 1 };
  mulib_browser_sandbox_t s;
  assert (mulib_browser_sandbox_init (&s, &limits));
  assert (mulib_browser_sandbox_charge (&s, 1023, 1, SIZE_MAX, 1));
  assert (!mulib_browser_sandbox_charge (&s, 1, 1, 1, 1));
  assert (s.response_bytes == 1023 && s.nodes == 1
          && s.script_steps == SIZE_MAX && s.tasks == 1);
  assert (mulib_browser_sandbox_charge (&s, 1, 1, 0, 1));
  mulib_browser_sandbox_cancel (&s);
  assert (!mulib_browser_sandbox_charge (&s, 0, 0, 0, 0));
  limits.max_depth = 0;
  assert (!mulib_browser_sandbox_init (&s, &limits));
  assert (s.cancelled && s.limits.max_depth == 1);
  assert (mulib_browser_sandbox_init (&s, &s.limits));
  assert (!s.cancelled && !s.response_bytes && !s.nodes && !s.script_steps
          && !s.tasks && s.limits.max_urls == 1);
  assert (mulib_browser_sandbox_charge (&s, 1024, 2, SIZE_MAX, 2));
  s.nodes++;
  assert (!mulib_browser_sandbox_charge (&s, 0, 0, 0, 0));

  const uint32_t unknown = UINT32_C (0x80000000);
  mulib_browser_permissions_t p = { 0 };
  assert (!mulib_browser_permissions_check (p, MULIB_BROWSER_ALLOW_STORAGE));
  assert (!mulib_browser_permissions_grant (
      &p, unknown | MULIB_BROWSER_ALLOW_STORAGE));
  assert (!p.allowed);
  assert (mulib_browser_permissions_grant (&p, MULIB_BROWSER_ALLOW_STORAGE));
  assert (mulib_browser_permissions_check (p, MULIB_BROWSER_ALLOW_STORAGE));
  p.allowed |= unknown;
  assert (!mulib_browser_permissions_grant (&p, MULIB_BROWSER_ALLOW_SCRIPT));
  mulib_browser_permissions_revoke (&p, unknown | MULIB_BROWSER_ALLOW_SCRIPT);
  assert (p.allowed == (unknown | MULIB_BROWSER_ALLOW_STORAGE));
  assert (!mulib_browser_permissions_check (p, MULIB_BROWSER_ALLOW_STORAGE));
  mulib_browser_permissions_revoke (&p, MULIB_BROWSER_ALLOW_STORAGE);
  assert (p.allowed == unknown);
}

typedef struct broker_probe
{
  int begins, calls, commits, aborts, audits, result;
  size_t reply_cap;
} broker_probe_t;

static int
begin (void *ctx, const char *subject)
{
  assert (!strcmp (subject, "fixture"));
  ((broker_probe_t *)ctx)->begins++;
  return 0;
}
static int
commit (void *ctx)
{
  broker_probe_t *p = ctx;
  p->commits++;
  return p->result;
}
static void
abort_job (void *ctx)
{
  ((broker_probe_t *)ctx)->aborts++;
}
static void
audit (void *ctx, const char *subject, const char *operation, int status)
{
  assert (!strcmp (subject, "fixture") && !strcmp (operation, "sample"));
  assert (!status || status == MULIB_SB_LIMIT || status == MULIB_SB_DENIED);
  ((broker_probe_t *)ctx)->audits++;
}
static int
handle (void *ctx, const char *subject, const void *req, size_t n, void *reply,
        size_t cap, size_t *len)
{
  broker_probe_t *p = ctx;
  assert (!strcmp (subject, "fixture"));
  p->calls++;
  p->reply_cap = cap;
  *len = n < cap ? n : cap;
  if (*len)
    memcpy (reply, req, *len);
  return 0;
}

static void
broker_policy (void)
{
  broker_probe_t probe = { 0 };
  mulib_capbroker_t b;
  mulib_sandbox_profile_t profile = mulib_sandbox_profile_default ();
  profile.max_frame_bytes = 8;
  profile.max_broker_bytes = 12;
  profile.max_broker_requests = 2;
  const mulib_capbroker_hooks_t hooks = { begin, commit, abort_job, audit,
                                         &probe };
  const mulib_capbroker_op_t ops[] = { { "sample", "sample.read", handle,
                                        &probe } };
  const char *const grants[] = { "sample.read" };
  char reply[10];
  size_t len = SIZE_MAX;

  assert (!mulib_capbroker_init (&b, "fixture", ops, 1, NULL, 0, &hooks,
                                 &profile));
  assert (!mulib_capbroker_begin (&b));
  assert (mulib_capbroker_call (&b, "sample", NULL, 0, NULL, 0, &len)
          == MULIB_SB_DENIED);
  assert (len == SIZE_MAX && !probe.calls && probe.audits == 1);
  assert (mulib_capbroker_finish (&b, 1) == MULIB_SB_DENIED);

  assert (!mulib_capbroker_init (&b, "fixture", ops, 1, grants, 1, &hooks,
                                 &profile));
  assert (!mulib_capbroker_begin (&b));
  assert (!mulib_capbroker_call (&b, "sample", "abcdefgh", 8, reply,
                                 sizeof reply, &len));
  assert (probe.reply_cap == 4 && len == 4 && b.bytes == 12);
  assert (!mulib_capbroker_call (&b, "sample", NULL, 0, NULL, 0, &len));
  assert (!len && !probe.reply_cap && b.requests == 2);
  len = SIZE_MAX;
  assert (mulib_capbroker_call (&b, "sample", NULL, 0, NULL, 0, &len)
          == MULIB_SB_LIMIT);
  assert (len == SIZE_MAX && probe.calls == 2 && probe.audits == 4);
  assert (mulib_capbroker_finish (&b, 1) == MULIB_SB_DENIED);

  assert (!mulib_capbroker_begin (&b));
  b.bytes = b.max_bytes + 1;
  assert (mulib_capbroker_call (&b, "sample", NULL, 0, NULL, 0, &len)
          == MULIB_SB_LIMIT);
  assert (probe.calls == 2 && len == SIZE_MAX);
  assert (mulib_capbroker_finish (&b, 1) == MULIB_SB_DENIED);
  assert (!mulib_capbroker_begin (&b));
  b.requests = b.max_requests + 1;
  assert (mulib_capbroker_finish (&b, 1) == MULIB_SB_DENIED);
  assert (!mulib_capbroker_begin (&b));
  assert (mulib_capbroker_finish (&b, MULIB_SB_SYSTEM) == MULIB_SB_DENIED);
  assert (!probe.commits && probe.aborts == 5);

  assert (!mulib_capbroker_begin (&b));
  probe.result = MULIB_SB_SYSTEM;
  assert (mulib_capbroker_finish (&b, 1) == MULIB_SB_SYSTEM);
  assert (probe.commits == 1 && probe.aborts == 6);
  assert (!mulib_capbroker_begin (&b));
  probe.result = MULIB_SB_UNCERTAIN;
  assert (mulib_capbroker_finish (&b, 1) == MULIB_SB_UNCERTAIN);
  assert (probe.commits == 2 && probe.aborts == 6);
  assert (mulib_capbroker_finish (&b, 1) == MULIB_SB_STATE);
  assert (mulib_capbroker_begin (&b) == MULIB_SB_STATE);
  assert (probe.begins == 7 && probe.commits == 2 && probe.aborts == 6);
}

int
main (void)
{
  origin_and_realm ();
  limits_and_permissions ();
  broker_policy ();
  return 0;
}
