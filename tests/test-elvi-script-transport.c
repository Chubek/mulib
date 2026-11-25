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
  assert(uri->host.length == 15 && !memcmp(uri->host.data, "www.example.org", 15));
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
  assert(!allocation_live);
  puts("elvi-script transport: OK");
  return 0;
}
