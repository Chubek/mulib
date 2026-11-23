/* Loopback-only fixture, never link this policy into real applications. */
#include "../runtime.h"
#include "alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static mulib_x509_store_t trust;
static void destroy_trust(void) { mulib_x509_store_free(&trust); }
static int fail_snapshot;

void *on1x_http_test_alloc(size_t size)
{
  return fail_snapshot ? NULL : malloc(size);
}

int on1x_http_authorize_v1(const char *url, const char *address, unsigned port,
                           int tls, int head, on1x_http_grant_v1 *grant)
{
  const char *allowed_port = getenv("ON1X_TEST_PORT");
  const char *ca = getenv("ON1X_TEST_CA");
  fprintf(stderr, "auth %d %d %u\n", tls, head, port);
  if (strcmp(address, "127.0.0.1") || !allowed_port ||
      strtoul(allowed_port, NULL, 10) != port || strstr(url, "/deny"))
    return 0;
  grant->wait_ms = 750;
  fail_snapshot = 1;
  if (strstr(url, "/bad-grant")) grant->wait_ms = 0;
  if (tls && ca && mulib_x509_store_file(&trust, ca)) {
    if (atexit(destroy_trust)) { destroy_trust(); return 0; }
    grant->trust = &trust;
  }
  return 1;
}
