#define _POSIX_C_SOURCE 200809L
#if defined(__unix__) || defined(__APPLE__)
#define MULIB_IMPL_MUNE
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_GLOB
#define MULIB_IMPL_XML
#define MULIB_IMPL_HTML
#define MULIB_IMPL_HTML_TEXTSTREAM
#define MULIB_IMPL_XPATH
#define MULIB_IMPL_IPC_FRAMED
#define MULIB_IMPL_ELVI_SCRIPT_IPC
#define MULIB_IMPL_ELVI_SCRIPT_LOGIC
#define MULIB_IMPL_ELVI_SCRIPT_TRANSPORT
#include "fixtures/elvi-scrape-setup.h"
#include "mulib-elvi-script-ipc.h"
#include "mulib-elvi-script-logic.h"
#include "mulib-elvi-script-transport.h"
#include <stdlib.h>

/* Inject failure at the coordinator's own queue/results allocation. The
 * dependency implementations above retain their normal allocation paths. */
static int fail_coordinator_alloc;
static void *fixture_coordinator_calloc(size_t count, size_t element)
{
  if (fail_coordinator_alloc) return NULL;
  return calloc(count, element);
}
#define calloc fixture_coordinator_calloc
#define MULIB_IMPL_ELVI_SCRIPT
#include "mulib-elvi-script.h"
#undef calloc
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct fixture {
  const char *root, *home;
  unsigned calls, behavior;
} fixture;

static char *read_fixture(const char *root, const char *relative, size_t *length)
{
  char path[4096];
  int n = snprintf(path, sizeof path, "%s/%s", root, relative);
  char *buf = malloc(65537);
  FILE *f;
  assert(n > 0 && (size_t)n < sizeof path && buf);
  f = fopen(path, "rb");
  assert(f);
  *length = fread(buf, 1, 65537, f);
  assert(*length <= 65536 && feof(f) && !ferror(f));
  assert(fclose(f) == 0);
  buf[*length] = '\0';
  return buf;
}

static void fixture_message(const char *root, const char *relative,
                            mulib_elvi_script_response_t *out)
{
  size_t length, consumed;
  char *body = read_fixture(root, relative, &length);
  char *wire = malloc(length + 256);
  int n;
  assert(wire);
  n = snprintf(wire, length + 256, "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
      "Content-Length: %zu\r\n\r\n%s", length, body);
  assert(n > 0 && (size_t)n < length + 256);
  assert(mulib_http_parse(wire, (size_t)n, 1, NULL, &out->message, &consumed) == MULIB_HTTP_OK);
  assert(consumed == (size_t)n);
  free(wire);
  free(body);
}

static int fetch_fixture(void *user, const mulib_elvi_script_request_t *req,
                         mulib_elvi_script_response_t *out)
{
  fixture *f = user;
  unsigned call = f->calls++;
  const char *relative;
  if (call == 0) {
    assert(!strcmp(req->url, f->home));
    relative = "tests/fixtures/elvi-script/booking-home.html";
  } else {
    assert(call == 1 && !strcmp(req->url, "https://booking.com/fixture/page-2"));
    relative = "tests/fixtures/elvi-script/booking-page-2.html";
  }
  if (call == 1 && f->behavior == 1) return 0;
  fixture_message(f->root, relative, out);
  if (f->behavior == 2) strcpy(out->effective_url, "https://wrong.example/");
  else if (f->behavior == 3) strcpy(out->effective_url, "https://booking.com/other");
  else strcpy(out->effective_url, req->url);
  return 1;
}

static void expect_failure(mulib_elvi_ipc_t *rx, unsigned code)
{
  mulib_elvi_ipc_event_t e;
  assert(mulib_elvi_ipc_receive(rx, &e) == 0 && e.kind == MULIB_ELVI_IPC_DIAGNOSTIC);
  assert(e.value == code);
  assert(mulib_elvi_ipc_receive(rx, &e) == 0 && e.kind == MULIB_ELVI_IPC_FAILURE);
  assert(e.value == code);
  assert(mulib_elvi_ipc_receive(rx, &e) == MULIB_SB_STATE);
}

static int never_connect(void *user, const mulib_uri_t *endpoint, mulib_tsl_t *session)
{
  (void)user; (void)endpoint; (void)session;
  assert(!"an unauthorized configuration must not connect");
  return 0;
}

static int never_peer(void *user, const mulib_tsl_t *session, mulib_elvi_scrape_peer_t *peer)
{
  (void)user; (void)session; (void)peer;
  assert(!"an unauthorized configuration must not inspect a session");
  return 0;
}

typedef struct fake_tls {
  char response[2][2048], request[4096];
  size_t offset, written;
  unsigned connects, closes;
} fake_tls;

static int fake_handshake(void *user, int *verified)
{
  (void)user;
  *verified = 1;
  return MULIB_TSL_OK;
}

static int fake_read(void *user, void *out, size_t capacity, size_t *length)
{
  fake_tls *f = user;
  const char *message = f->response[f->connects - 1];
  size_t n = strlen(message) - f->offset;
  if (n > capacity) n = capacity;
  if (n > 17) n = 17;
  memcpy(out, message + f->offset, n);
  f->offset += n;
  *length = n;
  return n ? MULIB_TSL_OK : MULIB_TSL_EOF;
}

static int fake_write(void *user, const void *data, size_t length, size_t *written)
{
  fake_tls *f = user;
  if (length > 19) length = 19;
  assert(length < sizeof f->request - f->written);
  memcpy(f->request + f->written, data, length);
  f->written += length;
  f->request[f->written] = '\0';
  *written = length;
  return MULIB_TSL_OK;
}

static void fake_close(void *user)
{
  fake_tls *f = user;
  const char *line = f->connects == 1 ? "GET / HTTP/1.1\r\n" :
                                      "GET /fixture/page-2 HTTP/1.1\r\n";
  ++f->closes;
  assert(!strncmp(f->request, line, strlen(line)));
  assert(strstr(f->request, "\r\nCookie: fixture_session=offline-not-a-credential\r\n"));
}

static int fake_connect(void *user, const mulib_uri_t *endpoint, mulib_tsl_t *session)
{
  fake_tls *f = user;
  mulib_tsl_ops_t ops = {fake_handshake, fake_read, fake_write, fake_close};
  assert(endpoint->host.length == 11 && !memcmp(endpoint->host.data, "booking.com", 11));
  assert(f->connects < 2 && f->closes == f->connects);
  ++f->connects;
  f->offset = f->written = 0;
  return mulib_tsl_init(session, &ops, f);
}

static int fake_peer(void *user, const mulib_tsl_t *session, mulib_elvi_scrape_peer_t *peer)
{
  (void)user;
  assert(session->state == 2);
  strcpy(peer->hostname, "booking.com");
  peer->certificate_verified = peer->hostname_verified = 1;
  peer->tls_version = 0x0304;
  strcpy(peer->alpn, "http/1.1");
  return 1;
}

/* Exercise production authorization, cookie scope, and transport composition
 * over in-memory TLS callbacks; there is no network connector in this test. */
static void authorized_run(const char *root, const mulib_elvi_scrape_program_t *p,
    mulib_elvi_script_run_config_t c)
{
  fake_tls fake = {0};
  mulib_elvi_scrape_https_t tls = {fake_connect, fake_peer, &fake};
  const char *pages[] = {"tests/fixtures/elvi-script/booking-home.html",
                         "tests/fixtures/elvi-script/booking-page-2.html"};
#if defined(__APPLE__)
  char home[] = "/private/tmp/mulib-elvi-fixture-XXXXXX";
#else
  char home[] = "/tmp/mulib-elvi-fixture-XXXXXX";
#endif
  char dot[128], elvi[128], jar[128];
  char path[192], logic_path[192];
  mulib_elvi_ipc_t rx;
  mulib_elvi_ipc_event_t e;
  int pair[2], fd;
  size_t n;
  char *json;
  assert(mkdtemp(home));
  assert(snprintf(dot, sizeof dot, "%s/.mulib", home) > 0 && mkdir(dot, 0700) == 0);
  assert(snprintf(elvi, sizeof elvi, "%s/elvi", dot) > 0 && mkdir(elvi, 0700) == 0);
  assert(snprintf(jar, sizeof jar, "%s/cookies", elvi) > 0 && mkdir(jar, 0700) == 0);
  assert(snprintf(path, sizeof path, "%s/BookingCookies.json", jar) > 0);
  assert(snprintf(logic_path, sizeof logic_path, "%s/logic/BookingDotCom.lua", elvi) > 0);
  json = read_fixture(root, "tests/fixtures/elvi-script/booking-cookies.json", &n);
  fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
  assert(fd >= 0 && write(fd, json, n) == (ssize_t)n && close(fd) == 0);
  free(json);
  for (size_t i = 0; i < sizeof pages / sizeof pages[0]; ++i) {
    char *body = read_fixture(root, pages[i], &n);
    int result = snprintf(fake.response[i], sizeof fake.response[i],
        "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %zu\r\n\r\n%s", n, body);
    assert(result > 0 && (size_t)result < sizeof fake.response[i]);
    free(body);
  }
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
  c.fetch = NULL;
  c.fetch_user = NULL;
  c.approved_home = home;
  c.approved_logic_path = logic_path;
  c.forward_fd = pair[0];
  assert(mulib_elvi_ipc_init(&rx, pair[1], c.ipc_timeout_ms) == 0);
  assert(mulib_elvi_script_run_https(p, &c, &tls, path) == 0);
  assert(fake.connects == 2 && fake.closes == 2);
  assert(mulib_elvi_ipc_receive(&rx, &e) == 0 && e.kind == MULIB_ELVI_IPC_RESULT);
  assert(!strcmp(e.result.name, "North Canal House"));
  assert(mulib_elvi_ipc_receive(&rx, &e) == 0 && e.kind == MULIB_ELVI_IPC_RESULT);
  assert(!strcmp(e.result.name, "Orchard Inn"));
  assert(mulib_elvi_ipc_receive(&rx, &e) == 0 && e.kind == MULIB_ELVI_IPC_COMPLETE);
  close(pair[0]);
  close(pair[1]);
  assert(unlink(path) == 0 && rmdir(jar) == 0 && rmdir(elvi) == 0 &&
         rmdir(dot) == 0 && rmdir(home) == 0);
}

static void receiver_run(const mulib_elvi_scrape_program_t *p,
    mulib_elvi_script_run_config_t c, const char *receiver, int complete)
{
  static const char expected[] =
    "North Canal House\thttps://booking.com/hotel/fixture/north-canal\tUSD 120\tCanal District\n"
    "Orchard Inn\thttps://booking.com/hotel/fixture/orchard\t\tHill & Grove\n";
  int pair[2], out[2], status;
  size_t n = 0;
  pid_t child;
  char buf[sizeof expected + 32], fdarg[32];
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0 && pipe(out) == 0);
  child = fork();
  assert(child >= 0);
  if (!child) {
    close(pair[0]);
    close(out[0]);
    if (dup2(out[1], STDOUT_FILENO) < 0) _exit(125);
    close(out[1]);
    if (snprintf(fdarg, sizeof fdarg, "%d", pair[1]) < 1) _exit(125);
    execl(receiver, receiver, fdarg, (char *)NULL);
    _exit(126);
  }
  close(pair[1]);
  close(out[1]);
  c.forward_fd = pair[0];
  if (complete) assert(mulib_elvi_script_run(p, &c) == 0);
  else {
    mulib_elvi_ipc_t tx;
    mulib_elvi_ipc_event_t e = {0};
    assert(mulib_elvi_ipc_init(&tx, pair[0], c.ipc_timeout_ms) == 0);
    e.kind = MULIB_ELVI_IPC_RESULT;
    strcpy(e.result.name, "Tentative result");
    strcpy(e.result.url, "https://booking.com/fixture/not-committed");
    assert(mulib_elvi_ipc_send(&tx, &e) == 0);
  }
  close(pair[0]);
  while (n < sizeof buf) {
    ssize_t r = read(out[0], buf + n, sizeof buf - n);
    assert(r >= 0);
    if (!r) break;
    n += (size_t)r;
  }
  close(out[0]);
  assert(waitpid(child, &status, 0) == child && WIFEXITED(status));
  if (complete) {
    assert(WEXITSTATUS(status) == 0);
    assert(n == sizeof expected - 1 && !memcmp(expected, buf, n));
    assert(fwrite(buf, 1, n, stdout) == n); /* Show the verified offline demo. */
  } else assert(WEXITSTATUS(status) == 1 && n == 0);
}

static void pipeline(const char *root, const char *receiver)
{
  const char fixture_home[] = "/nonexistent/elvi-test-home";
  const char logic_path[] = "/nonexistent/elvi-test-home/.mulib/elvi/logic/BookingDotCom.lua";
  const char ipc_path[] = "/dev/shm/elv-scrape/BookingDotCom";
  char path[4096];
  static const char denied[] = "function on_response(d, u) enqueue('https://wrong.example/') end";
  static const char duplicate[] = "function on_response(d, u) enqueue(u) end";
  static const char overflow[] =
    "function on_response(d, u) for i = 1, 8 do enqueue('https://booking.com/fixture/' .. i) end end";
  static const char finish_fail[] =
    "function on_response(d, u) emit('draft', 'https://booking.com/fixture/result') end\n"
    "function on_finish() enqueue('https://booking.com/fixture/not-permitted') end";
  static const char control_result[] =
    "function on_response(d, u)\n"
    "emit('draft', 'https://booking.com/fixture/result')\n"
    "emit('invalid\\tname', 'https://booking.com/fixture/result') end";
  mulib_elvi_scrape_program_t p;
  mulib_elvi_script_run_config_t c = {0};
  size_t length;
  char *source, *cookie_json;
  mulib_elvi_script_cookies_t *cookies;
  fixture f = {root, p.home, 0, 0};

  int n = snprintf(path, sizeof path, "%s/library/elvis/BookingDotCom.elvs", root);
  assert(n > 0 && (size_t)n < sizeof path && mulib_elvi_scrape_load(path, &p, NULL));
  assert(p.mode == MULIB_ELVI_MODE_SCRAPE && !strcmp(p.host, "booking.com"));

  cookie_json = read_fixture(root, "tests/fixtures/elvi-script/booking-cookies.json", &length);
  cookies = mulib_elvi_script_cookies_parse(&p, cookie_json, length);
  assert(cookies);
  mulib_elvi_script_cookies_destroy(cookies);
  free(cookie_json);

  source = read_fixture(root, "library/elvis/logic/BookingDotCom.lua", &length);
  c.approved_home = fixture_home;
  c.approved_logic_path = logic_path;
  c.approved_forward_path = ipc_path;
  c.logic_source = source;
  c.logic_length = length;
  c.ipc_timeout_ms = 5000;
  c.fetch = fetch_fixture;
  c.fetch_user = &f;

  for (unsigned i = 0; i < 12; ++i) {
    mulib_elvi_script_run_config_t cfg = c;
    mulib_elvi_ipc_t rx;
    int pair[2], rc;
    f.calls = 0;
    f.behavior = i < 4 ? i : 0;
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    cfg.forward_fd = pair[0];
    assert(mulib_elvi_ipc_init(&rx, pair[1], cfg.ipc_timeout_ms) == 0);
    if (i == 4) { cfg.logic_source = denied; cfg.logic_length = sizeof denied - 1; }
    if (i == 5) { cfg.logic_source = duplicate; cfg.logic_length = sizeof duplicate - 1; }
    if (i == 6) { cfg.logic_source = overflow; cfg.logic_length = sizeof overflow - 1; }
    if (i == 7) { cfg.logic_source = finish_fail; cfg.logic_length = sizeof finish_fail - 1; }
    if (i == 8) cfg.approved_logic_path = "/different/logic.lua";
    if (i == 10) { cfg.logic_source = control_result; cfg.logic_length = sizeof control_result - 1; }
    if (i == 9) {
      mulib_elvi_scrape_https_t tls = {never_connect, never_peer, NULL};
      cfg.fetch = NULL;
      cfg.fetch_user = NULL;
      rc = mulib_elvi_script_run_https(&p, &cfg, &tls, "/different/credentials.json");
    } else {
      fail_coordinator_alloc = i == 11;
      rc = mulib_elvi_script_run(&p, &cfg);
      fail_coordinator_alloc = 0;
    }
    if (i == 0) {
      mulib_elvi_ipc_event_t e;
      assert(rc == 0 && f.calls == 2);
      assert(!mulib_elvi_ipc_receive(&rx, &e) && e.kind == MULIB_ELVI_IPC_RESULT);
      assert(!strcmp(e.result.name, "North Canal House") && !strcmp(e.result.price, "USD 120"));
      assert(!mulib_elvi_ipc_receive(&rx, &e) && e.kind == MULIB_ELVI_IPC_RESULT);
      assert(!strcmp(e.result.name, "Orchard Inn") && !e.result.price[0]);
      assert(!mulib_elvi_ipc_receive(&rx, &e) && e.kind == MULIB_ELVI_IPC_COMPLETE && e.value == 2);
    } else {
      assert(rc && f.calls == (i == 1 ? 2u : i == 8 || i == 9 || i == 11 ? 0u : 1u));
      if (i == 11) assert(rc == MULIB_SB_NOMEM);
      expect_failure(&rx, i == 1 ? MULIB_ELVI_IPC_FETCH :
          i <= 3 ? MULIB_ELVI_IPC_RESPONSE : i <= 6 || i == 10 ? MULIB_ELVI_IPC_POLICY :
          i == 7 ? MULIB_ELVI_IPC_LOGIC :
          i == 11 ? MULIB_ELVI_IPC_RESOURCE : MULIB_ELVI_IPC_CONFIG);
    }
    close(pair[0]);
    close(pair[1]);
  }
  {
    int pair[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    c.forward_fd = pair[0];
    f.calls = 0;
    close(pair[1]);
    assert(mulib_elvi_script_run(&p, &c) == MULIB_SB_EOF && f.calls == 0);
    close(pair[0]);
  }
  if (receiver) {
    f.calls = f.behavior = 0;
    receiver_run(&p, c, receiver, 1);
    assert(f.calls == 2);
    receiver_run(&p, c, receiver, 0);
  }
  authorized_run(root, &p, c);
  free(source);
}

static int fetch_agoda_fixture(void *user, const mulib_elvi_script_request_t *req,
                               mulib_elvi_script_response_t *out)
{
  fixture *f = user;
  assert(f->calls++ == 0 && !strcmp(req->url, f->home));
  fixture_message(f->root, "tests/fixtures/elvi-script/agoda-listings.html", out);
  strcpy(out->effective_url, req->url);
  return 1;
}

static void agoda(const char *root)
{
  const char home[] = "https://www.agoda.com/city/singapore-sg.html";
  fixture f = {root, home, 0, 0};
  char path[4096];
  mulib_elvi_scrape_program_t p;
  mulib_elvi_script_run_config_t c = {0};
  mulib_elvi_ipc_t rx;
  mulib_elvi_ipc_event_t e;
  int pair[2];
  int n = snprintf(path, sizeof path, "%s/library/elvis/Agoda.elvs", root);
  assert(n > 0 && (size_t)n < sizeof path && mulib_elvi_scrape_load(path, &p, NULL));
  assert(p.mode == MULIB_ELVI_MODE_SCRAPE && p.cookies.kind == MULIB_ELVI_STAGE_NONE);
  char *source = read_fixture(root, "library/elvis/logic/Agoda.lua", &c.logic_length);
  c.logic_source = source;
  c.approved_home = "/nonexistent/elvi-test-home";
  c.approved_logic_path = "/nonexistent/elvi-test-home/.mulib/elvi/logic/Agoda.lua";
  c.approved_forward_path = "/dev/shm/elv-scrape/Agoda";
  c.fetch = fetch_agoda_fixture;
  c.fetch_user = &f;
  c.ipc_timeout_ms = 5000;
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
  c.forward_fd = pair[0];
  assert(!mulib_elvi_ipc_init(&rx, pair[1], c.ipc_timeout_ms));
  assert(!mulib_elvi_script_run(&p, &c) && f.calls == 1);
  assert(!mulib_elvi_ipc_receive(&rx, &e) && e.kind == MULIB_ELVI_IPC_RESULT);
  assert(!strcmp(e.result.name, "Canal & Garden House"));
  assert(!strcmp(e.result.url, "https://www.agoda.com/canal-house/hotel/singapore-sg.html"));
  assert(!strcmp(e.result.price, "SGD 140") && !strcmp(e.result.location, "Singapore River"));
  assert(!mulib_elvi_ipc_receive(&rx, &e) && e.kind == MULIB_ELVI_IPC_RESULT);
  assert(!strcmp(e.result.name, "Hill Inn") &&
         !strcmp(e.result.url, "https://www.agoda.com/hill-inn/hotel/singapore-sg.html"));
  assert(!e.result.price[0] && !e.result.location[0]);
  assert(!mulib_elvi_ipc_receive(&rx, &e) && e.kind == MULIB_ELVI_IPC_COMPLETE && e.value == 2);
  close(pair[0]);
  close(pair[1]);
  free(source);
}

static void malformed_frames(void)
{
  static const unsigned char bad[][16] = {
    {0, 3, 0},                  /* Unsupported version. */
    {1, 255, 0},                /* Unknown frame kind. */
    {1, 3, 1},                  /* Incorrect completion count. */
    {1, 4, 1},                  /* Failure without diagnostic. */
    {1, 2, 255},                /* Unknown diagnostic. */
    {1, 1, 0xff, 0xff},         /* Field length > field capacity. */
    {1, 1, 0, 2, 0xc0, 0xaf},   /* Invalid UTF-8. */
    {1, 1, 0, 2, 'a', 0},       /* Embedded NUL. */
    {1, 1, 0, 1, 'A', 0, 1, 'B', 0, 0, 0, 0, 42}, /* Extra byte. */
    {1, 3, 0, 1},               /* Extra status payload. */
  };
  static const size_t lengths[] = {3, 3, 3, 3, 3, 4, 6, 6, 13, 4};
  for (size_t i = 0; i < sizeof lengths / sizeof lengths[0]; ++i) {
    mulib_ipc_framed_t raw;
    mulib_elvi_ipc_t rx;
    mulib_elvi_ipc_event_t e, before;
    int pair[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    assert(mulib_elvi_ipc_init(&rx, pair[1], 2000) == 0);
    assert(mulib_ipc_framed_init(&raw, pair[0], MULIB_ELVI_IPC_MAX_FRAME) == 0);
    assert(mulib_ipc_send(&raw, bad[i], lengths[i], rx.deadline) == 0);
    memset(&e, 0xa5, sizeof e);
    memcpy(&before, &e, sizeof e);
    assert(mulib_elvi_ipc_receive(&rx, &e) == MULIB_SB_PROTOCOL);
    assert(!memcmp(&before, &e, sizeof e));
    assert(mulib_elvi_ipc_receive(&rx, &e) == MULIB_SB_STATE);
    close(pair[0]);
    close(pair[1]);
  }
}

static void ipc_limits(void)
{
  mulib_elvi_ipc_t tx, rx;
  mulib_elvi_ipc_event_t e = {0};
  int pair[2], sndbuf = 4096, rc = 0;
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
  assert(mulib_elvi_ipc_init(&tx, pair[0], 0) == MULIB_SB_INVALID);
  assert(mulib_elvi_ipc_init(&tx, pair[0], MULIB_ELVI_IPC_MAX_TIMEOUT_MS + 1u) == MULIB_SB_INVALID);
  assert(mulib_elvi_ipc_init(&tx, pair[0], 1000) == 0);
  assert(mulib_elvi_ipc_init(&rx, pair[1], 1000) == 0);
  e.kind = MULIB_ELVI_IPC_COMPLETE;
  close(pair[1]);
  assert(mulib_elvi_ipc_send(&tx, &e) != 0); /* No SIGPIPE. */
  assert(mulib_elvi_ipc_send(&tx, &e) == MULIB_SB_STATE);
  close(pair[0]);

  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
  assert(mulib_elvi_ipc_init(&rx, pair[1], 1000) == 0);
  {
    const unsigned char partial[] = {0, 0, 0, 10, 1, 1};
    assert(write(pair[0], partial, sizeof partial) == (ssize_t)sizeof partial);
  }
  close(pair[0]);
  assert(mulib_elvi_ipc_receive(&rx, &e) == MULIB_SB_PROTOCOL);
  close(pair[1]);

  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
  assert(mulib_elvi_ipc_init(&rx, pair[1], 30) == 0);
  assert(mulib_elvi_ipc_receive(&rx, &e) == MULIB_SB_TIMEOUT);
  assert(mulib_elvi_ipc_receive(&rx, &e) == MULIB_SB_STATE);
  close(pair[0]);
  close(pair[1]);

  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
  assert(setsockopt(pair[0], SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof sndbuf) == 0);
  assert(mulib_elvi_ipc_init(&tx, pair[0], 40) == 0);
  e.kind = MULIB_ELVI_IPC_RESULT;
  strcpy(e.result.name, "valid record");
  memset(e.result.url, 'x', sizeof e.result.url - 1);
  for (unsigned i = 0; i < MULIB_ELVI_SCRIPT_MAX_RESULTS && !rc; ++i)
    rc = mulib_elvi_ipc_send(&tx, &e);
  assert(rc == MULIB_SB_TIMEOUT && tx.stream.poisoned);
  assert(mulib_elvi_ipc_send(&tx, &e) == MULIB_SB_STATE);
  close(pair[0]);
  close(pair[1]);
}

int main(int argc, char **argv)
{
  const char *root = argc > 1 ? argv[1] : ".";
  malformed_frames();
  ipc_limits();
  pipeline(root, argc > 2 ? argv[2] : NULL);
  agoda(root);
  puts("ElviScript offline pipeline and IPC: OK");
  return 0;
}
#else
int main(void) { return 0; }
#endif
