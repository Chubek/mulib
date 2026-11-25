/** @file mulib-elvi-script.h
 * Synchronous ElviScript scrape coordinator. Search mode uses ElviScrape.
 * A compiled profile selects providers; only the host supplies capabilities.
 */
#ifndef MULIB_ELVI_SCRIPT_H
#define MULIB_ELVI_SCRIPT_H
#include "mulib-elvi-scrape.h"
#include "mulib-elvi-script-ipc.h"
#include "mulib-elvi-script-logic.h"
#include "mulib-elvi-script-transport.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_elvi_script_run_config {
  const char *approved_home; /* No environment lookup. NULL for literal paths. */
  const char *approved_logic_path, *approved_forward_path;
  const char *logic_source; /* Borrowed bytes explicitly authorized for that path. */
  size_t logic_length;
  int forward_fd; /* Borrowed, exclusive connected Unix stream for that path. */
  uint64_t ipc_timeout_ms;
  mulib_elvi_script_fetch_fn fetch; /* Injected callback, e.g. offline transport. */
  void *fetch_user;
} mulib_elvi_script_run_config_t;

/* Borrow an unmodified compiled profile, independently authorized paths,
 * source bytes and descriptor for one invocation. A logical path is matched
 * exactly after expansion and never opened here. No cookies are read by this
 * entry point; the supplied transport is responsible for any credentials.
 * Return zero only after sending COMPLETE. On failure close/discard the
 * stream (even if DIAGNOSTIC/FAILURE could not be sent). Callbacks are
 * synchronous and application-owned; IPC deadlines cannot interrupt them. */
int mulib_elvi_script_run(const mulib_elvi_scrape_program_t *program,
                          const mulib_elvi_script_run_config_t *config);

/* Explicit opt-in to HTTPS with host-owned verified TLS callbacks and finite
 * TLS I/O limits. If the profile declares cookies, its resolved path must
 * exactly match approved_cookie_path before the authorized loader opens it.
 * config->fetch and config->fetch_user must both be NULL. No redirect/retry. */
int mulib_elvi_script_run_https(const mulib_elvi_scrape_program_t *program,
    const mulib_elvi_script_run_config_t *config, const mulib_elvi_scrape_https_t *https,
    const char *approved_cookie_path);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_ELVI_SCRIPT) && !defined(MULIB_ELVI_SCRIPT_IMPLEMENTED)
#define MULIB_ELVI_SCRIPT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <errno.h>
#include <poll.h>
#endif

typedef struct mulib_elvi_script_run_state_ {
  const mulib_elvi_scrape_program_t *program;
  mulib_elvi_script_request_t requests[MULIB_ELVI_SCRIPT_MAX_REQUESTS];
  mulib_elvi_script_result_t results[MULIB_ELVI_SCRIPT_MAX_RESULTS];
  unsigned head, tail, count;
  int rejected;
} mulib_elvi_script_run_state_;

static int mulib_elvi_script_enqueue_(void *user, const mulib_elvi_script_request_t *req)
{
  mulib_elvi_script_run_state_ *s = (mulib_elvi_script_run_state_ *)user;
  if (!req || s->rejected || s->tail >= MULIB_ELVI_SCRIPT_MAX_REQUESTS ||
      !memchr(req->url, '\0', sizeof req->url) ||
      !mulib_elvi_script_url_allowed(s->program, req->url)) goto reject;
  for (unsigned i = 0; i < s->tail; ++i)
    if (!strcmp(s->requests[i].url, req->url)) goto reject;
  s->requests[s->tail++] = *req;
  return 1;
reject:
  s->rejected = 1;
  return 0;
}

static int mulib_elvi_script_emit_(void *user, const mulib_elvi_script_result_t *r)
{
  mulib_elvi_script_run_state_ *s = (mulib_elvi_script_run_state_ *)user;
  if (s->rejected || s->count >= s->program->max_results ||
      !mulib_elvi_ipc_result_valid(r) ||
      !mulib_elvi_script_url_allowed(s->program, r->url)) {
    s->rejected = 1;
    return 0;
  }
  s->results[s->count++] = *r;
  return 1;
}

static int mulib_elvi_script_match_path_(const mulib_elvi_stage_t *stage,
    const char *home, const char *authorized, char *resolved, size_t cap)
{
  return authorized && authorized[0] == '/' &&
         memchr(stage->argument, '\0', sizeof stage->argument) &&
         mulib_elvi_script_resolve(stage->argument, home, resolved, cap) &&
         !strcmp(resolved, authorized);
}

static int mulib_elvi_script_response_ok_(const mulib_elvi_scrape_program_t *p,
    const mulib_elvi_script_request_t *req, const mulib_elvi_script_response_t *response)
{
  const mulib_http_message_t *m = &response->message;
  const char *type, *encoding;
  const char html[] = "text/html", identity[] = "identity";
  size_t cursor = 0;
  if (!memchr(response->effective_url, '\0', sizeof response->effective_url) ||
      !mulib_elvi_script_url_allowed(p, response->effective_url) ||
      strcmp(req->url, response->effective_url) ||
      !m->is_response || m->status != 200 || !m->body ||
      !m->body_length || m->body_length > MULIB_ELVI_SCRIPT_MAX_BODY ||
      m->header_count > 128 || (m->header_count && !m->headers)) return 0;
  type = mulib_http_header_next(m, "Content-Type", &cursor);
  if (!type || mulib_http_header_next(m, "Content-Type", &cursor)) return 0;
  for (size_t i = 0; i < sizeof html - 1; ++i) {
    unsigned char c = (unsigned char)*type++;
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + ('a' - 'A'));
    if (c != (unsigned char)html[i]) return 0;
  }
  while (*type == ' ' || *type == '\t') ++type;
  if (*type && *type != ';') return 0;
  cursor = 0;
  encoding = mulib_http_header_next(m, "Content-Encoding", &cursor);
  if (encoding && mulib_http_header_next(m, "Content-Encoding", &cursor)) return 0;
  for (size_t i = 0; encoding && i < sizeof identity; ++i) {
    unsigned char c = (unsigned char)*encoding++;
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + ('a' - 'A'));
    if (c != (unsigned char)identity[i]) return 0;
  }
  return 1;
}

/* Do not start another callback on an already-disconnected stream. A peer
 * could still disconnect during a callback; the next check/send detects it. */
static int mulib_elvi_script_forward_alive_(const mulib_elvi_ipc_t *ipc)
{
  int64_t now = mulib_ipc_now_ms();
  if (now < 0 || now >= ipc->deadline) return now < 0 ? MULIB_SB_SYSTEM : MULIB_SB_TIMEOUT;
#if defined(__unix__) || defined(__APPLE__)
  struct pollfd poller = {ipc->stream.fd, 0, 0};
  int n = poll(&poller, 1, 0);
  if (n < 0 && errno != EINTR) return MULIB_SB_SYSTEM;
  if (n > 0 && (poller.revents & (POLLERR | POLLHUP | POLLNVAL))) return MULIB_SB_EOF;
  return 0;
#else
  return MULIB_SB_UNSUPPORTED;
#endif
}

static int mulib_elvi_script_report_(mulib_elvi_ipc_t *ipc, unsigned code, int status)
{
  mulib_elvi_ipc_event_t e = {0};
  int rc;
  e.value = code;
  e.kind = MULIB_ELVI_IPC_DIAGNOSTIC;
  rc = mulib_elvi_ipc_send(ipc, &e);
  if (!rc) {
    e.kind = MULIB_ELVI_IPC_FAILURE;
    rc = mulib_elvi_ipc_send(ipc, &e);
  }
  return rc ? rc : status;
}

static int mulib_elvi_script_run_(const mulib_elvi_scrape_program_t *p,
    const mulib_elvi_script_run_config_t *config, int live,
    const mulib_elvi_scrape_https_t *https, const char *approved_cookie_path)
{
  mulib_elvi_script_run_state_ *s = NULL;
  mulib_elvi_script_logic_t *logic = NULL;
  mulib_elvi_script_cookies_t *cookies = NULL;
  mulib_elvi_script_transport_t transport;
  mulib_elvi_script_fetch_fn fetch;
  void *fetch_user;
  mulib_elvi_ipc_t ipc;
  mulib_elvi_ipc_event_t event = {0};
  char path[4096];
  int rc;
  unsigned code = 0;
  if (!p || !config || p->mode != MULIB_ELVI_MODE_SCRAPE ||
      p->forward.kind != MULIB_ELVI_STAGE_IPC ||
      !mulib_elvi_script_match_path_(&p->forward, config->approved_home,
                                     config->approved_forward_path, path, sizeof path))
    return MULIB_SB_INVALID;
  rc = mulib_elvi_ipc_init(&ipc, config->forward_fd, config->ipc_timeout_ms);
  if (rc) return rc;
  rc = mulib_elvi_script_forward_alive_(&ipc);
  if (rc) return rc;
  if (p->xpath.kind != MULIB_ELVI_STAGE_XPATH ||
      p->artifacts.kind != MULIB_ELVI_STAGE_DISCARD ||
      p->logic.kind != MULIB_ELVI_STAGE_LUA || !p->max_results ||
      p->max_results > MULIB_ELVI_SCRIPT_MAX_RESULTS ||
      !config->logic_source || !config->logic_length ||
      !mulib_elvi_script_match_path_(&p->logic, config->approved_home,
                                     config->approved_logic_path, path, sizeof path) ||
      (p->cookies.kind != MULIB_ELVI_STAGE_NONE &&
       p->cookies.kind != MULIB_ELVI_STAGE_COOKIE_FILE) ||
      (!live && !config->fetch) ||
      (live && (!https || !https->connect || !https->peer || config->fetch ||
                config->fetch_user))) {
    code = MULIB_ELVI_IPC_CONFIG;
    rc = MULIB_SB_INVALID;
    goto done;
  }
  fetch = config->fetch;
  fetch_user = config->fetch_user;
  if (live) {
    if (p->cookies.kind == MULIB_ELVI_STAGE_COOKIE_FILE) {
      if (!mulib_elvi_script_match_path_(&p->cookies, config->approved_home,
                                        approved_cookie_path, path, sizeof path) ||
          !(cookies = mulib_elvi_script_cookies_load(p, path, approved_cookie_path))) {
        code = MULIB_ELVI_IPC_CONFIG;
        rc = MULIB_SB_DENIED;
        goto done;
      }
    }
    transport.program = p;
    transport.https = https;
    transport.cookies = cookies;
    fetch = mulib_elvi_script_fetch_https;
    fetch_user = &transport;
  }
  s = (mulib_elvi_script_run_state_ *)calloc(1, sizeof *s);
  if (!s) {
    code = MULIB_ELVI_IPC_RESOURCE;
    rc = MULIB_SB_NOMEM;
    goto done;
  }
  s->program = p;
  {
    mulib_elvi_script_request_t first = {{0}};
    if (!memchr(p->home, '\0', sizeof p->home) ||
        !mulib_elvi_script_url_allowed(p, p->home)) {
      code = MULIB_ELVI_IPC_POLICY;
      rc = MULIB_SB_DENIED;
      goto done;
    }
    strcpy(first.url, p->home);
    if (!mulib_elvi_script_enqueue_(s, &first)) {
      code = MULIB_ELVI_IPC_POLICY;
      rc = MULIB_SB_DENIED;
      goto done;
    }
  }
  logic = mulib_elvi_script_logic_create(config->logic_source, config->logic_length,
                                         mulib_elvi_script_enqueue_, mulib_elvi_script_emit_, s);
  if (!logic || s->rejected) {
    code = s->rejected ? MULIB_ELVI_IPC_POLICY : MULIB_ELVI_IPC_LOGIC;
    rc = s->rejected ? MULIB_SB_DENIED : MULIB_SB_PROTOCOL;
    goto done;
  }
  while (s->head < s->tail) {
    const mulib_elvi_script_request_t *req = &s->requests[s->head++];
    mulib_elvi_script_response_t response = {0};
    rc = mulib_elvi_script_forward_alive_(&ipc);
    if (rc) goto done;
    if (!fetch(fetch_user, req, &response)) code = MULIB_ELVI_IPC_FETCH;
    else if ((rc = mulib_elvi_script_forward_alive_(&ipc)) != 0) {
      mulib_http_message_free(&response.message);
      goto done;
    }
    else if (!mulib_elvi_script_response_ok_(p, req, &response)) code = MULIB_ELVI_IPC_RESPONSE;
    else if (!mulib_elvi_script_logic_response(logic, &response) || s->rejected)
      code = s->rejected ? MULIB_ELVI_IPC_POLICY : MULIB_ELVI_IPC_LOGIC;
    /* artifact discard: no body or headers are written to disk or IPC. */
    mulib_http_message_free(&response.message);
    if (code) {
      rc = code == MULIB_ELVI_IPC_POLICY ? MULIB_SB_DENIED : MULIB_SB_PROTOCOL;
      goto done;
    }
  }
  if (!mulib_elvi_script_logic_finish(logic) || s->rejected) {
    code = s->rejected ? MULIB_ELVI_IPC_POLICY : MULIB_ELVI_IPC_LOGIC;
    rc = s->rejected ? MULIB_SB_DENIED : MULIB_SB_PROTOCOL;
    goto done;
  }
  for (unsigned i = 0; i < s->count; ++i) {
    event.kind = MULIB_ELVI_IPC_RESULT;
    event.result = s->results[i];
    rc = mulib_elvi_ipc_send(&ipc, &event);
    if (rc) goto done;
  }
  event.kind = MULIB_ELVI_IPC_COMPLETE;
  event.value = s->count;
  rc = mulib_elvi_ipc_send(&ipc, &event);
done:
  mulib_elvi_script_logic_destroy(logic);
  mulib_elvi_script_cookies_destroy(cookies);
  free(s);
  return code ? mulib_elvi_script_report_(&ipc, code, rc) : rc;
}

int mulib_elvi_script_run(const mulib_elvi_scrape_program_t *program,
                          const mulib_elvi_script_run_config_t *config)
{
  return mulib_elvi_script_run_(program, config, 0, NULL, NULL);
}

int mulib_elvi_script_run_https(const mulib_elvi_scrape_program_t *program,
    const mulib_elvi_script_run_config_t *config, const mulib_elvi_scrape_https_t *https,
    const char *approved_cookie_path)
{
  return mulib_elvi_script_run_(program, config, 1, https, approved_cookie_path);
}
#endif
