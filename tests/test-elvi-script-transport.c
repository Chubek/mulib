/* Real MuLIB TLS lifecycle and HTTP parsing, with an offline TLS fixture. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static size_t allocation_calls, allocation_fail, allocation_live;
static int fail_alloc(void) { return ++allocation_calls == allocation_fail; }
static void *track_malloc(size_t n) {
  void *p = fail_alloc() ? NULL : malloc(n);
  if (p) ++allocation_live;
  return p;
}
static void *track_calloc(size_t n, size_t size) {
  void *p = fail_alloc() ? NULL : calloc(n, size);
  if (p) ++allocation_live;
  return p;
}
static void track_free(void *p) {
  if (p) {
    assert(allocation_live);
    --allocation_live;
  }
  free(p);
}
static void *track_realloc(void *p, size_t n) {
  int was_new = !p;
  void *q;
  if (fail_alloc()) return NULL;
  if (!n) { track_free(p); return NULL; }
  q = realloc(p, n);
  if (q && was_new) ++allocation_live;
  return q;
}

static time_t test_clock = 2000;
static time_t fixture_time(time_t *out) {
  if (out) *out = test_clock;
  return test_clock;
}

#define malloc track_malloc
#define calloc track_calloc
#define realloc track_realloc
#define free track_free
#include "fixtures/elvi-scrape-setup.h"
#define time fixture_time
#define MULIB_IMPL_ELVI_SCRIPT_TRANSPORT
#include "mulib-elvi-script-transport.h"
#undef time
#undef malloc
#undef calloc
#undef realloc
#undef free

#ifdef ON1X_TEST_ELVI_CAPTURE_BRIDGE
#include "capture.h"
#endif

static const char dsl[] = "elvi fixture\nhost www.example.org\n"
  "home https://www.example.org/\nsearch https://www.example.org/?q={query}\nanchor any\n";

#define COOKIE(n, v, d, p, s, site, more) \
  "{\"name\":\"" n "\",\"value\":\"" v "\",\"domain\":\"" d "\",\"path\":\"" p \
  "\",\"secure\":" s ",\"sameSite\":\"" site "\"" more "}"

static const char cookie_json[] = "["
  COOKIE("global", "one", "WWW.example.org", "/", "true", "Strict", "")
  "," COOKIE("session", "safe", ".example.org", "/app", "true", "Lax", ",\"hostOnly\":false")
  "," COOKIE("other", "safe", "www.example.org", "/app/", "true", "None", ",\"expires\":2001")
  "," COOKIE("expired", "no", "www.example.org", "/", "true", "Strict", ",\"expires\":2000")
  "," COOKIE("insecure", "no", "www.example.org", "/", "false", "Lax", "")
  "," COOKIE("empty", "", "www.example.org", "/", "true", "Lax", ",\"expires\":null")
  "," COOKIE("__Host-valid", "\\u0061", "www.example.org", "\\/", "true", "Lax", "")
  "]";

static const char valid_wire[] = "HTTP/1.1 200 OK\r\n"
  "Content-Type: text/html; charset=utf-8\r\nContent-Encoding: Identity\r\n"
  "Set-Cookie: ignored=new; Secure\r\nTransfer-Encoding: chunked\r\n\r\n"
  "3\r\na\0b\r\n0\r\n\r\n";

typedef struct fixture {
  const char *wire;
  size_t length, read, written, read_cap;
  unsigned connects, closes, mode;
  char request[16384];
} fixture;

enum {
  CONNECT_FAIL = 1, HANDSHAKE_FAIL, HANDSHAKE_AGAIN, UNVERIFIED_HANDSHAKE,
  PEER_FAIL, BAD_CERT, BAD_HOST, WRONG_HOST, UNTERMINATED_HOST, BAD_VERSION,
  BAD_ALPN, UNTERMINATED_ALPN, WRITE_FAIL, ABRUPT_EOF, TLS12_NO_ALPN
};

static int handshake_tls(void *context, int *verified) {
  fixture *f = context;
  *verified = f->mode != UNVERIFIED_HANDSHAKE;
  if (f->mode == HANDSHAKE_FAIL) return MULIB_TSL_EBACKEND;
  if (f->mode == HANDSHAKE_AGAIN) return MULIB_TSL_AGAIN;
  return MULIB_TSL_OK;
}
static int read_tls(void *context, void *out, size_t cap, size_t *n) {
  fixture *f = context;
  size_t left = f->length - f->read;
  *n = 0;
  if (!left) return f->mode == ABRUPT_EOF ? MULIB_TSL_EBACKEND : MULIB_TSL_EOF;
  if (cap > f->read_cap) cap = f->read_cap;
  if (left > cap) left = cap;
  memcpy(out, f->wire + f->read, left);
  f->read += left;
  *n = left;
  return MULIB_TSL_OK;
}
static int write_tls(void *context, const void *in, size_t n, size_t *got) {
  fixture *f = context;
  if (f->mode == WRITE_FAIL) { *got = 0; return MULIB_TSL_EBACKEND; }
  if (n > 17) n = 17;
  assert(f->written + n < sizeof f->request);
  memcpy(f->request + f->written, in, n);
  f->written += n;
  f->request[f->written] = '\0';
  *got = n;
  return MULIB_TSL_OK;
}
static void close_tls(void *context) { ++((fixture *)context)->closes; }
static int connect_tls(void *context, const mulib_uri_t *uri, mulib_tsl_t *tls) {
  fixture *f = context;
  const mulib_tsl_ops_t ops = {handshake_tls, read_tls, write_tls, close_tls};
  assert(uri->host.length == 15 && (!memcmp(uri->host.data, "www.example.org", 15) ||
                                    !memcmp(uri->host.data, "WWW.EXAMPLE.ORG", 15)));
  assert(uri->has_port && uri->port.length == 3 && !memcmp(uri->port.data, "443", 3));
  ++f->connects;
  assert(mulib_tsl_init(tls, &ops, context));
  return f->mode != CONNECT_FAIL;
}
static int peer_tls(void *context, const mulib_tsl_t *tls, mulib_elvi_scrape_peer_t *peer) {
  fixture *f = context;
  assert(tls->context == context && f->written == 0);
  peer->certificate_verified = f->mode != BAD_CERT;
  peer->hostname_verified = f->mode != BAD_HOST;
  peer->tls_version = f->mode == BAD_VERSION ? 0x0302 :
                      f->mode == TLS12_NO_ALPN ? 0x0303 : 0x0304;
  strcpy(peer->hostname, f->mode == WRONG_HOST ? "example.org" : "WWW.EXAMPLE.ORG");
  strcpy(peer->alpn, f->mode == BAD_ALPN ? "h2" : f->mode == TLS12_NO_ALPN ? "" : "http/1.1");
  if (f->mode == UNTERMINATED_HOST) memset(peer->hostname, 'x', sizeof peer->hostname);
  if (f->mode == UNTERMINATED_ALPN) memset(peer->alpn, 'x', sizeof peer->alpn);
  return f->mode != PEER_FAIL;
}

static fixture make_fixture(const char *wire, size_t length) {
  fixture f = {0};
  f.wire = wire;
  f.length = length;
  f.read_cap = 7;
  return f;
}
static int fetch(const mulib_elvi_scrape_program_t *program,
                 const mulib_elvi_script_cookies_t *store, fixture *f, const char *url,
                 mulib_elvi_script_response_t *response) {
  mulib_elvi_scrape_https_t https = {connect_tls, peer_tls, f};
  mulib_elvi_script_transport_t transport = {program, &https, store};
  mulib_elvi_script_request_t request;
  if (url) strcpy(request.url, url);
  else memset(request.url, 'a', sizeof request.url);
  return mulib_elvi_script_fetch_https(&transport, &request, response);
}
static void rejected(const mulib_elvi_scrape_program_t *program,
                     const mulib_elvi_script_cookies_t *store, fixture *f, const char *url) {
  mulib_elvi_script_response_t response = {0};
  unsigned char snapshot[sizeof response];
  strcpy(response.effective_url, "untouched");
  memcpy(snapshot, &response, sizeof snapshot);
  assert(!fetch(program, store, f, url, &response));
  assert(!memcmp(snapshot, &response, sizeof response));
  assert(f->connects == f->closes);
}

static void cookie_requests(const mulib_elvi_scrape_program_t *p,
                            const mulib_elvi_script_cookies_t *store) {
  static const struct { const char *url, *cookies; } cases[] = {
    {"https://www.example.org/app/",
     "Cookie: other=safe; session=safe; global=one; empty=; __Host-valid=a\r\n"},
    {"https://www.example.org/app?x=/",
     "Cookie: session=safe; global=one; empty=; __Host-valid=a\r\n"},
    {"https://www.example.org/application?x=/app",
     "Cookie: global=one; empty=; __Host-valid=a\r\n"},
    {"https://www.example.org?x=/app",
     "Cookie: global=one; empty=; __Host-valid=a\r\n"}
  };
  for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
    mulib_elvi_script_response_t response = {0};
    fixture f = make_fixture(valid_wire, sizeof valid_wire - 1);
    assert(fetch(p, store, &f, cases[i].url, &response));
    assert(f.connects == 1 && f.closes == 1);
    assert(strstr(f.request, "GET /") == f.request && strstr(f.request, cases[i].cookies));
    assert(!strstr(f.request, "ignored=") && !strstr(f.request, "expired="));
    assert(response.message.is_response && response.message.chunked &&
      response.message.status == 200 && response.message.body_length == 3);
    assert(!memcmp(response.message.body, "a\0b", 3) && !strcmp(response.effective_url, cases[i].url));
    assert(!strcmp(mulib_http_header(&response.message, "Content-Type"), "text/html; charset=utf-8"));
    mulib_http_message_free(&response.message);
  }
  {
    mulib_elvi_script_response_t response = {0};
    fixture f = make_fixture(valid_wire, sizeof valid_wire - 1);
    assert(fetch(p, NULL, &f, cases[0].url, &response));
    assert(!strstr(f.request, "Cookie:"));
    mulib_http_message_free(&response.message);
  }
  {
    mulib_elvi_script_response_t response = {0};
    fixture f = make_fixture(valid_wire, sizeof valid_wire - 1);
    test_clock = 2001;
    assert(fetch(p, store, &f, cases[0].url, &response));
    assert(!strstr(f.request, "other="));
    mulib_http_message_free(&response.message);
    f = make_fixture(valid_wire, sizeof valid_wire - 1);
    test_clock = (time_t)-1;
    rejected(p, store, &f, cases[0].url);
    assert(!f.connects);
    test_clock = 2000;
  }
}

static void policy(const mulib_elvi_scrape_program_t *p,
                   const mulib_elvi_script_cookies_t *store) {
  const char *bad_urls[] = {
    "http://www.example.org/", "https://localhost/", "https://127.0.0.1/",
    "https://example.org/", "https://a.www.example.org/", "https://www.example.org.evil.test/",
    "https://www.example.org:443/", "https://www.example.org/x#fragment",
    "https://www.example.org@evil.test/", "https://www.example.org/%0d%0a", NULL
  };
  for (size_t i = 0; i < sizeof bad_urls / sizeof *bad_urls; ++i) {
    fixture f = make_fixture(valid_wire, sizeof valid_wire - 1);
    rejected(p, store, &f, bad_urls[i]);
    assert(!f.connects);
  }
  {
    mulib_elvi_scrape_program_t other;
    const char source[] = "elvi fixture\nhost other.org\nhome https://other.org/\n"
      "search https://other.org/?q={query}\nanchor any\n";
    fixture f = make_fixture(valid_wire, sizeof valid_wire - 1);
    assert(mulib_elvi_scrape_compile(source, sizeof source - 1, &other, NULL));
    rejected(&other, store, &f, other.home);
    assert(!f.connects);
  }
  for (unsigned mode = CONNECT_FAIL; mode <= ABRUPT_EOF; ++mode) {
    fixture f = make_fixture(valid_wire, sizeof valid_wire - 1);
    f.mode = mode;
    rejected(p, store, &f, p->home);
    assert(f.connects == 1 && (f.written > 0) == (mode == ABRUPT_EOF));
  }
  {
    mulib_elvi_script_response_t response = {0};
    fixture f = make_fixture(valid_wire, sizeof valid_wire - 1);
    f.mode = TLS12_NO_ALPN;
    assert(fetch(p, store, &f, p->home, &response) && f.closes == 1);
    mulib_http_message_free(&response.message);
  }
  {
    const char *targets[] = {"/again", "https://www.example.org/app", "http://evil.test/",
                             "https://127.0.0.1/", "https://example.org/"};
    for (size_t i = 0; i < sizeof targets / sizeof *targets; ++i) {
      char wire[256];
      int n = snprintf(wire, sizeof wire, "HTTP/1.1 302 Found\r\nLocation: %s\r\n"
                       "Content-Length: 0\r\n\r\n", targets[i]);
      assert(n > 0 && (size_t)n < sizeof wire);
      fixture f = make_fixture(wire, (size_t)n);
      rejected(p, store, &f, p->home);
      assert(f.connects == 1);
    }
  }
}

static void response_limits(const mulib_elvi_scrape_program_t *p) {
  const char *bad[] = {
    "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Type: text/plain\r\n"
      "Content-Length: 0\r\n\r\n",
    "HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: 0\r\n\r\n",
    "HTTP/1.1 200 OK\r\nContent-Encoding: Identity\r\nContent-Encoding: Identity\r\n"
      "Content-Length: 0\r\n\r\n",
    "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Length: 0\r\n\r\n",
    "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Length: 0\r\n\r\n0\r\n\r\n",
    "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1\r\na\r\n",
    "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\nextra",
    "HTTP/1.1 304 Not Modified\r\n\r\n",
    "HTTP/1.1 101 Switching Protocols\r\n\r\n"
  };
  for (size_t i = 0; i < sizeof bad / sizeof *bad; ++i) {
    fixture f = make_fixture(bad[i], strlen(bad[i]));
    rejected(p, NULL, &f, p->home);
    assert(f.connects == 1);
  }
  {
    const char not_found[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    mulib_elvi_script_response_t response = {0};
    fixture f = make_fixture(not_found, sizeof not_found - 1);
    assert(fetch(p, NULL, &f, p->home, &response) && response.message.status == 404);
    mulib_http_message_free(&response.message);
  }
  {
    const char prefix[] = "HTTP/1.1 200 OK\r\n\r\n";
    const size_t cap = MULIB_ELVI_SCRIPT_MAX_BODY + 65536u + 1u;
    const size_t body_limit = sizeof prefix - 1u + MULIB_ELVI_SCRIPT_MAX_BODY;
    char *wire = malloc(cap);
    fixture f;
    assert(wire);
    memcpy(wire, prefix, sizeof prefix - 1u);
    memset(wire + sizeof prefix - 1u, 'x', cap - (sizeof prefix - 1u));
    for (size_t n = body_limit; n <= cap; n = n == body_limit ? n + 1u : cap) {
      f = make_fixture(wire, n);
      f.read_cap = 8192;
      if (n == body_limit) {
        mulib_elvi_script_response_t response = {0};
        assert(fetch(p, NULL, &f, p->home, &response));
        assert(response.message.close_delimited && response.message.body_length == MULIB_ELVI_SCRIPT_MAX_BODY);
        mulib_http_message_free(&response.message);
      } else rejected(p, NULL, &f, p->home);
      assert(f.connects == 1 && f.closes == 1);
      if (n == cap) break;
    }
    free(wire);
  }
}

static void cookie_json_limits(const mulib_elvi_scrape_program_t *p) {
  const char *bad[] = {
    "", "null", "{}", "[[]]", "[{}]", "[]x", "[,]", "[{} ,]", "[]\v",
    "[" COOKIE("a", "x", "example.org", "/", "true", "Strict", "") "]",
    "[" COOKIE("a", "x", ".www.example.org", "/", "true", "Strict", "") "]",
    "[" COOKIE("a", "x", ".org", "/", "true", "Strict", ",\"hostOnly\":false") "]",
    "[" COOKIE("a", "x", ".www.example.net", "/", "true", "Strict", ",\"hostOnly\":false") "]",
    "[" COOKIE("a", "x", "..example.org", "/", "true", "Strict", ",\"hostOnly\":false") "]",
    "[" COOKIE("a", "x", "www.example.org", "/app?bad", "true", "Strict", "") "]",
    "[" COOKIE("a=b", "x", "www.example.org", "/", "true", "Strict", "") "]",
    "[" COOKIE("a", "foo;bar", "www.example.org", "/", "true", "Strict", "") "]",
    "[" COOKIE("a", "x\\u0000", "www.example.org", "/", "true", "Strict", "") "]",
    "[" COOKIE("a", "x\\u00ff", "www.example.org", "/", "true", "Strict", "") "]",
    "[" COOKIE("a", "x\\u000d\\u000a", "www.example.org", "/", "true", "Strict", "") "]",
    "[" COOKIE("a", "x\\uzzzz", "www.example.org", "/", "true", "Strict", "") "]",
    "[" COOKIE("a", "x", "www.example.org", "/", "false", "None", "") "]",
    "[" COOKIE("a", "x", "www.example.org", "/", "true", "unknown", "") "]",
    "[" COOKIE("__Secure-test", "x", "www.example.org", "/", "false", "Strict", "") "]",
    "[" COOKIE("__Host-test", "x", "www.example.org", "/", "true", "Strict", ",\"hostOnly\":false") "]",
    "[" COOKIE("__Host-test", "x", "www.example.org", "/app", "true", "Strict", "") "]",
    "[" COOKIE("a", "x", "www.example.org", "/", "true", "Strict", ",\"s\\u0065cure\":false") "]",
    "[" COOKIE("a", "x", "www.example.org", "/", "true", "Strict", ",\"other\":null") "]",
    "[" COOKIE("a", "x", "www.example.org", "/", "true", "Strict", ",\"expires\":02000") "]",
    "[" COOKIE("a", "x", "www.example.org", "/", "true", "Strict", ",\"expires\":- 1") "]",
    "[" COOKIE("a", "x", "www.example.org", "/", "true", "Strict", ",\"expires\":2000.1") "]",
    "[" COOKIE("a", "x", "www.example.org", "/", "true", "Strict", ",\"expires\":1e9") "]",
    "[" COOKIE("a", "x", "www.example.org", "/", "true", "Strict", ",\"expires\":253402300800") "]",
    "[" COOKIE("a", "x", "www.example.org", "/", "true", "Strict", "") ","
        COOKIE("a", "y", "www.example.org", "/", "true", "Strict", "") "]"
  };
  for (size_t i = 0; i < sizeof bad / sizeof *bad; ++i)
    assert(!mulib_elvi_script_cookies_parse(p, bad[i], strlen(bad[i])));
  assert(!mulib_elvi_script_cookies_parse(p, "[]\0", 3));
  assert(!mulib_elvi_script_cookies_parse(p, "[]", MULIB_ELVI_SCRIPT_MAX_COOKIE_JSON + 1u));
  {
    mulib_elvi_script_cookies_t *empty = mulib_elvi_script_cookies_parse(p, " [ ]\n", 5);
    assert(empty);
    mulib_elvi_script_cookies_destroy(empty);
    mulib_elvi_script_cookies_destroy(NULL);
  }
  {
    char json[8192];
    size_t used = 1;
    json[0] = '[';
    for (unsigned k = 0; k <= MULIB_ELVI_SCRIPT_MAX_COOKIES; ++k) {
      int n = snprintf(json + used, sizeof json - used,
        (k ? "," COOKIE("a%u", "", "www.example.org", "/", "true", "Strict", "") :
              COOKIE("a%u", "", "www.example.org", "/", "true", "Strict", "")), k);
      assert(n > 0 && (size_t)n < sizeof json - used);
      used += (size_t)n;
      json[used] = ']';
      if (k == MULIB_ELVI_SCRIPT_MAX_COOKIES - 1u) {
        mulib_elvi_script_cookies_t *store = mulib_elvi_script_cookies_parse(p, json, used + 1u);
        assert(store);
        mulib_elvi_script_cookies_destroy(store);
      }
    }
    assert(!mulib_elvi_script_cookies_parse(p, json, used + 1u));
  }
  {
    char json[12288], value[2049];
    size_t used = 1;
    json[0] = '[';
    memset(value, 'x', sizeof value - 1);
    value[sizeof value - 1] = 0;
    for (unsigned k = 0; k < 4; ++k) {
      int n = snprintf(json + used, sizeof json - used,
        (k ? "," COOKIE("a%u", "%s", "www.example.org", "/", "true", "Strict", "") :
              COOKIE("a%u", "%s", "www.example.org", "/", "true", "Strict", "")), k, value);
      assert(n > 0 && (size_t)n < sizeof json - used);
      used += (size_t)n;
    }
    json[used] = ']';
    mulib_elvi_script_cookies_t *store = mulib_elvi_script_cookies_parse(p, json, used + 1u);
    fixture f = make_fixture(valid_wire, sizeof valid_wire - 1);
    assert(store);
    rejected(p, store, &f, p->home);
    assert(!f.connects);
    mulib_elvi_script_cookies_destroy(store);
  }
}

static void files_and_allocations(const mulib_elvi_scrape_program_t *p,
                                  const mulib_elvi_script_cookies_t *cookies) {
  char dir[] = "/tmp/mulib-elvi-cookies.XXXXXX";
  char path[256], other[256], alias[256], repeated[256];
  mulib_elvi_script_cookies_t *store;
  size_t allocations, baseline = allocation_live;
  int fd;
  assert(mkdtemp(dir));
  assert(snprintf(path, sizeof path, "%s/cookies.json", dir) > 0);
  assert(snprintf(other, sizeof other, "%s/other", dir) > 0);
  assert(snprintf(alias, sizeof alias, "%s/alias", dir) > 0);
  assert(snprintf(repeated, sizeof repeated, "%s/./cookies.json", dir) > 0);
  fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
  assert(fd >= 0 && write(fd, cookie_json, sizeof cookie_json - 1) == sizeof cookie_json - 1 && !close(fd));

  assert(!mulib_elvi_script_cookies_load(p, path, NULL));
  assert(!mulib_elvi_script_cookies_load(p, path, other));
  assert(!mulib_elvi_script_cookies_load(p, repeated, repeated));
  assert(!mulib_elvi_script_cookies_load(p, "cookies.json", "cookies.json"));
  assert(!symlink(path, alias) && !mulib_elvi_script_cookies_load(p, alias, alias) && !unlink(alias));
  assert(!link(path, other) && !mulib_elvi_script_cookies_load(p, path, path) && !unlink(other));
  assert(!mkfifo(other, 0600) && !mulib_elvi_script_cookies_load(p, other, other) && !unlink(other));
  assert(!chmod(path, 0640) && !mulib_elvi_script_cookies_load(p, path, path) && !chmod(path, 0600));
  assert(!chmod(dir, 0775) && !mulib_elvi_script_cookies_load(p, path, path) && !chmod(dir, 0700));
  assert(!symlink("/tmp", other));
  assert(snprintf(repeated, sizeof repeated, "%s/%s/cookies.json", other, dir + 5) > 0);
  assert(!mulib_elvi_script_cookies_load(p, repeated, repeated) && !unlink(other));

  allocation_calls = 0;
  store = mulib_elvi_script_cookies_parse(p, cookie_json, sizeof cookie_json - 1);
  assert(store && allocation_calls);
  allocations = allocation_calls;
  mulib_elvi_script_cookies_destroy(store);
  for (size_t i = 1; i <= allocations; ++i) {
    allocation_calls = 0; allocation_fail = i;
    assert(!mulib_elvi_script_cookies_parse(p, cookie_json, sizeof cookie_json - 1));
    assert(allocation_live == baseline);
  }
  allocation_calls = allocation_fail = 0;
  store = mulib_elvi_script_cookies_load(p, path, path);
  assert(store && allocation_calls);
  allocations = allocation_calls;
  mulib_elvi_script_cookies_destroy(store);
  for (size_t i = 1; i <= allocations; ++i) {
    allocation_calls = 0; allocation_fail = i;
    assert(!mulib_elvi_script_cookies_load(p, path, path) && allocation_live == baseline);
  }
  allocation_calls = allocation_fail = 0;
  {
    mulib_elvi_script_response_t response = {0};
    fixture f = make_fixture(valid_wire, sizeof valid_wire - 1);
    assert(fetch(p, cookies, &f, p->home, &response));
    mulib_http_message_free(&response.message);
  }
  allocations = allocation_calls;
  assert(allocations && allocation_live == baseline);
  for (size_t i = 1; i <= allocations; ++i) {
    fixture f = make_fixture(valid_wire, sizeof valid_wire - 1);
    allocation_calls = 0; allocation_fail = i;
    rejected(p, cookies, &f, p->home);
    assert(allocation_live == baseline);
  }
  allocation_calls = allocation_fail = 0;
  fd = open(path, O_WRONLY);
  assert(fd >= 0 && !ftruncate(fd, MULIB_ELVI_SCRIPT_MAX_COOKIE_JSON + 1u) && !close(fd));
  assert(!mulib_elvi_script_cookies_load(p, path, path));
  fd = open(path, O_WRONLY | O_TRUNC);
  assert(fd >= 0 && write(fd, "[]x", 3) == 3 && !close(fd));
  assert(!mulib_elvi_script_cookies_load(p, path, path));
  assert(!unlink(path) && !rmdir(dir));
}

static const char document_profile[] = "elvi fixture\nhost www.example.org\n"
  "home https://www.example.org/\nmode document\nartifacts capture\n";

static void document_rejected(const mulib_elvi_scrape_program_t *program,
    const mulib_elvi_script_capture_config_t *config,
    mulib_elvi_script_capture_result_t expected) {
  mulib_elvi_script_document_t document = {0};
  unsigned char snapshot[sizeof document];
  fixture *f = config->https->context;
  size_t baseline = allocation_live;
  strcpy(document.metadata.effective_url, "unchanged");
  memcpy(snapshot, &document, sizeof snapshot);
  assert(mulib_elvi_script_capture_https(program, config, &document) == expected);
  assert(!memcmp(snapshot, &document, sizeof snapshot));
  assert(f->connects == f->closes && allocation_live == baseline);
}

static void document_policy(const mulib_elvi_scrape_program_t *search) {
  static const char *const invalid_suffixes[] = {
    "xpath provide{mulib-xpath}\n", "logic lua{/tmp/logic.lua}\n", "forward ipc{/tmp/receiver}\n",
    "search https://www.example.org/?q={query}\n", "artifacts discard\n",
    "anchor any\n", "format html\n", "mode scrape\n"
  };
  static const char discard_profile[] = "elvi fixture\nhost www.example.org\n"
    "home https://www.example.org/\nmode scrape\nartifacts discard\n"
    "xpath provide{mulib-xpath}\nlogic lua{/tmp/logic.lua}\nforward ipc{/tmp/receiver}\n";
  static const char *const bad_urls[] = {
    "http://www.example.org/", "https://www.example.org:443/", "https://www.example.org/?q=#x",
    "https://www.example.org@evil.test/", "https://a.www.example.org/",
    "https://www.example.org.evil.test/", "https://www.example.org/%00",
    "https://www.example.org/%5cpath"
  };
  mulib_elvi_scrape_program_t program, discard, invalid;
  mulib_elvi_scrape_error_t error;
  mulib_elvi_script_document_t doc = {0};
  fixture f = make_fixture(valid_wire, sizeof valid_wire - 1);
  mulib_elvi_scrape_https_t https = {connect_tls, peer_tls, &f};
  mulib_elvi_script_capture_config_t config = {NULL, "www.example.org", &https, 3, NULL, NULL};
  char source[1024], oversize_url[MULIB_ELVI_DOCUMENT_MAX_URL + 1u];
  char rendered[128];
  assert(mulib_elvi_scrape_compile(document_profile, sizeof document_profile - 1, &program, &error));
  assert(program.mode == MULIB_ELVI_MODE_DOCUMENT &&
         program.artifacts.kind == MULIB_ELVI_STAGE_CAPTURE && program.artifacts.line == 5);
  rejected(&program, NULL, &f, program.home); /* No legacy transport bypass. */
  assert(!mulib_elvi_scrape_url(&program, "", 1, rendered, sizeof rendered));
  assert(mulib_elvi_scrape_compile(discard_profile, sizeof discard_profile - 1, &discard, NULL));
  document_rejected(search, &config, MULIB_ELVI_CAPTURE_INVALID);
  document_rejected(&discard, &config, MULIB_ELVI_CAPTURE_INVALID);
  invalid = program;
  invalid.artifacts.kind = MULIB_ELVI_STAGE_DISCARD;
  document_rejected(&invalid, &config, MULIB_ELVI_CAPTURE_INVALID);
  invalid = program;
  invalid.logic.kind = MULIB_ELVI_STAGE_LUA;
  document_rejected(&invalid, &config, MULIB_ELVI_CAPTURE_INVALID);
  for (size_t i = 0; i < sizeof invalid_suffixes / sizeof *invalid_suffixes; ++i) {
    int n = snprintf(source, sizeof source, "%s%s", document_profile, invalid_suffixes[i]);
    assert(n > 0 && (size_t)n < sizeof source);
    assert(!mulib_elvi_scrape_compile(source, (size_t)n, &invalid, &error) && error.line == 6);
  }
  {
    const char *capture = strstr(document_profile, "artifacts capture");
    assert(capture);
    assert(!mulib_elvi_scrape_compile(document_profile, (size_t)(capture - document_profile),
                                      &invalid, &error));
    assert(snprintf(source, sizeof source, "%.*sartifacts discard\n",
                    (int)(capture - document_profile), document_profile) > 0);
    assert(!mulib_elvi_scrape_compile(source, strlen(source), &invalid, &error));
    assert(snprintf(source, sizeof source, "%.*sartifacts capture\n",
                    (int)(strstr(discard_profile, "artifacts discard") - discard_profile),
                    discard_profile) > 0);
    assert(!mulib_elvi_scrape_compile(source, strlen(source), &invalid, &error));
  }
  config.approved_host = NULL;
  document_rejected(&program, &config, MULIB_ELVI_CAPTURE_DENIED);
  config.approved_host = "example.org";
  document_rejected(&program, &config, MULIB_ELVI_CAPTURE_DENIED);
  config.approved_host = "WWW.example.org";
  document_rejected(&program, &config, MULIB_ELVI_CAPTURE_DENIED);
  config.approved_host = "www.example.org";
  for (size_t i = 0; i < sizeof bad_urls / sizeof *bad_urls; ++i) {
    config.url = bad_urls[i];
    document_rejected(&program, &config, MULIB_ELVI_CAPTURE_DENIED);
  }
  strcpy(oversize_url, "https://www.example.org/");
  memset(oversize_url + strlen(oversize_url), 'a', sizeof oversize_url - strlen(oversize_url) - 1u);
  oversize_url[sizeof oversize_url - 1u] = '\0';
  config.url = oversize_url;
  document_rejected(&program, &config, MULIB_ELVI_CAPTURE_DENIED);
  config.url = NULL;
  config.max_body_bytes = 0;
  document_rejected(&program, &config, MULIB_ELVI_CAPTURE_INVALID);
  config.max_body_bytes = SIZE_MAX;
  document_rejected(&program, &config, MULIB_ELVI_CAPTURE_INVALID);
  config.max_body_bytes = 3;
  assert(!f.connects && !f.written);

  config.url = "https://WWW.EXAMPLE.ORG/capture?query=ok";
#ifdef ON1X_TEST_ELVI_CAPTURE_BRIDGE
  assert(on1x_elvi_capture_https(&program, &config, &doc) == MULIB_ELVI_CAPTURE_OK);
  {
    char hex[7] = "unused";
    assert(!on1x_elvi_document_copy_hex(&doc, hex, 6) && !strcmp(hex, "unused"));
    assert(on1x_elvi_document_copy_hex(&doc, hex, sizeof hex) && !strcmp(hex, "610062"));
  }
#else
  assert(mulib_elvi_script_capture_https(&program, &config, &doc) == MULIB_ELVI_CAPTURE_OK);
#endif
  assert(f.connects == 1 && f.closes == 1 && strstr(f.request, "GET /capture?query=ok HTTP/1.1\r\n"));
  assert(!strstr(f.request, "Cookie:") && doc.metadata.status == 200 &&
    doc.metadata.tls_version == 0x0304 && doc.metadata.certificate_verified == 1 &&
    doc.metadata.hostname_verified == 1 && doc.metadata.chunked && !doc.metadata.close_delimited);
  assert(!strcmp(doc.metadata.http_protocol, "HTTP/1.1") && !strcmp(doc.metadata.alpn, "http/1.1"));
  assert(!strcmp(doc.metadata.requested_url, config.url) && !strcmp(doc.metadata.effective_url, config.url));
  assert(!strcmp(doc.metadata.content_type, "text/html; charset=utf-8"));
  assert(doc.body_length == 3 && !memcmp(doc.body, "a\0b", 3));
  mulib_elvi_script_document_destroy(&doc);
  assert(!doc.body && !doc.body_length && !doc.metadata.status);
  mulib_elvi_script_document_destroy(&doc);
  mulib_elvi_script_document_destroy(NULL);
}

static void document_transport(void) {
  static const char redirect[] = "HTTP/1.1 302 Found\r\nLocation: /again\r\nContent-Length: 3\r\n\r\nabc";
  static const char not_found[] = "HTTP/1.1 404 Not Found\r\n\r\nno";
  static const char malformed[] = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nabc";
  static const char compressed[] = "HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\n\r\nabc";
  static const char duplicated[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n"
    "Content-Type: text/html\r\n\r\nabc";
  static const char nonascii[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html; x=\xff\r\n\r\n";
  const struct { const char *wire; mulib_elvi_script_capture_result_t result; } cases[] = {
    {redirect, MULIB_ELVI_CAPTURE_REDIRECT}, {malformed, MULIB_ELVI_CAPTURE_RESPONSE},
    {compressed, MULIB_ELVI_CAPTURE_RESPONSE}, {duplicated, MULIB_ELVI_CAPTURE_RESPONSE},
    {nonascii, MULIB_ELVI_CAPTURE_RESPONSE}
  };
  mulib_elvi_scrape_program_t program;
  mulib_elvi_script_document_t doc = {0};
  fixture f = {0};
  mulib_elvi_scrape_https_t https = {connect_tls, peer_tls, &f};
  mulib_elvi_script_capture_config_t config = {NULL, "www.example.org", &https, 32, NULL, NULL};
  size_t baseline = allocation_live, allocations;
  assert(mulib_elvi_scrape_compile(document_profile, sizeof document_profile - 1, &program, NULL));
  for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
    f = make_fixture(cases[i].wire, strlen(cases[i].wire));
    document_rejected(&program, &config, cases[i].result);
    assert(f.connects == 1 && f.written);
  }
  {
    char large_type[512];
    const char padded[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html; padding=%0256u\r\n\r\n";
    int n = snprintf(large_type, sizeof large_type, padded, 0u);
    assert(n > 0 && (size_t)n < sizeof large_type);
    f = make_fixture(large_type, (size_t)n);
    document_rejected(&program, &config, MULIB_ELVI_CAPTURE_LIMIT);
  }
  f = make_fixture(valid_wire, sizeof valid_wire - 1);
  config.max_body_bytes = 2;
  document_rejected(&program, &config, MULIB_ELVI_CAPTURE_LIMIT);
  config.max_body_bytes = 32;
  for (unsigned mode = CONNECT_FAIL; mode <= ABRUPT_EOF; ++mode) {
    f = make_fixture(valid_wire, sizeof valid_wire - 1);
    f.mode = mode;
    document_rejected(&program, &config, mode <= UNTERMINATED_ALPN ?
      MULIB_ELVI_CAPTURE_TLS : MULIB_ELVI_CAPTURE_TRANSPORT);
    assert(f.connects == 1 && (f.written > 0) == (mode == ABRUPT_EOF));
  }
  f = make_fixture(not_found, sizeof not_found - 1);
  f.mode = TLS12_NO_ALPN;
  assert(mulib_elvi_script_capture_https(&program, &config, &doc) == MULIB_ELVI_CAPTURE_OK);
  assert(f.connects == 1 && f.closes == 1 && doc.metadata.status == 404 &&
    doc.metadata.close_delimited && !doc.metadata.chunked && !doc.metadata.content_type[0] &&
    doc.metadata.tls_version == 0x0303 && !doc.metadata.alpn[0] &&
    doc.body_length == 2 && !memcmp(doc.body, "no", 2));
  mulib_elvi_script_document_destroy(&doc);

  allocation_calls = 0;
  f = make_fixture(valid_wire, sizeof valid_wire - 1);
  assert(mulib_elvi_script_capture_https(&program, &config, &doc) == MULIB_ELVI_CAPTURE_OK);
  allocations = allocation_calls;
  mulib_elvi_script_document_destroy(&doc);
  assert(allocations && allocation_live == baseline);
  for (size_t i = 1; i <= allocations; ++i) {
    allocation_calls = 0;
    allocation_fail = i;
    f = make_fixture(valid_wire, sizeof valid_wire - 1);
    document_rejected(&program, &config, MULIB_ELVI_CAPTURE_NOMEM);
  }
  allocation_calls = allocation_fail = 0;
}

static void document_cookie_authority(void) {
  mulib_elvi_scrape_program_t program;
  mulib_elvi_script_document_t doc = {0};
  char dir[] = "/tmp/mulib-elvi-capture.XXXXXX", path[256], source[1024];
  fixture f = make_fixture(valid_wire, sizeof valid_wire - 1);
  mulib_elvi_scrape_https_t https = {connect_tls, peer_tls, &f};
  mulib_elvi_script_capture_config_t config = {NULL, "www.example.org", &https, 3, NULL, NULL};
  int n, fd;
  n = snprintf(source, sizeof source, "%scookies provide{$HOME/cookies.json}\n", document_profile);
  assert(n > 0 && (size_t)n < sizeof source);
  assert(mulib_elvi_scrape_compile(source, (size_t)n, &program, NULL));
  document_rejected(&program, &config, MULIB_ELVI_CAPTURE_DENIED);
  assert(mkdtemp(dir) && snprintf(path, sizeof path, "%s/cookies.json", dir) > 0);
  config.approved_cookie_path = path;
  document_rejected(&program, &config, MULIB_ELVI_CAPTURE_DENIED); /* No approved HOME. */
  config.approved_home = dir;
  document_rejected(&program, &config, MULIB_ELVI_CAPTURE_CREDENTIALS); /* Missing file. */
  fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
  assert(fd >= 0 && write(fd, cookie_json, sizeof cookie_json - 1) == sizeof cookie_json - 1 && !close(fd));
  config.approved_host = "other.org";
  document_rejected(&program, &config, MULIB_ELVI_CAPTURE_DENIED);
  config.approved_host = "www.example.org";
  assert(!f.connects && !f.written);
  assert(mulib_elvi_script_capture_https(&program, &config, &doc) == MULIB_ELVI_CAPTURE_OK);
  assert(f.connects == 1 && f.closes == 1 && strstr(f.request, "Cookie: global=one;"));
  assert(!strstr(doc.metadata.content_type, "global="));
  mulib_elvi_script_document_destroy(&doc);
  assert(!unlink(path) && !rmdir(dir));
}

int main(void) {
  mulib_elvi_scrape_program_t program;
  mulib_elvi_script_cookies_t *cookies;
  assert(mulib_elvi_scrape_compile(dsl, sizeof dsl - 1, &program, NULL));
  cookies = mulib_elvi_script_cookies_parse(&program, cookie_json, sizeof cookie_json - 1);
  assert(cookies);
  cookie_requests(&program, cookies);
  policy(&program, cookies);
  response_limits(&program);
  cookie_json_limits(&program);
  files_and_allocations(&program, cookies);
  mulib_elvi_script_cookies_destroy(cookies);
  document_policy(&program);
  document_transport();
  document_cookie_authority();
  assert(!allocation_live);
  puts("elvi-script transport: OK");
  return 0;
}
