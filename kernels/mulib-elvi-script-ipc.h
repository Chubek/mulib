/** @file mulib-elvi-script-ipc.h
 * ElviScript v1 event protocol over mulib-ipc-framed. The caller explicitly
 * supplies an authorized, exclusive, connected Unix stream (e.g. a supervised
 * socketpair). No pathname lookup, listener, reconnect, or peer discovery.
 */
#ifndef MULIB_ELVI_SCRIPT_IPC_H
#define MULIB_ELVI_SCRIPT_IPC_H
#include "detail/mulib-elvi-script-types.h"
#include "mulib-ipc-framed.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MULIB_ELVI_IPC_VERSION 1
#define MULIB_ELVI_IPC_MAX_FRAME (2u + 8u + 511u + 2047u + 127u + 255u)
#define MULIB_ELVI_IPC_MAX_TIMEOUT_MS 30000u

typedef enum mulib_elvi_ipc_kind {
  MULIB_ELVI_IPC_RESULT = 1, MULIB_ELVI_IPC_DIAGNOSTIC,
  MULIB_ELVI_IPC_COMPLETE, MULIB_ELVI_IPC_FAILURE
} mulib_elvi_ipc_kind_t;
typedef enum mulib_elvi_ipc_code {
  MULIB_ELVI_IPC_CONFIG = 1, MULIB_ELVI_IPC_RESOURCE, MULIB_ELVI_IPC_FETCH,
  MULIB_ELVI_IPC_RESPONSE, MULIB_ELVI_IPC_LOGIC, MULIB_ELVI_IPC_POLICY
} mulib_elvi_ipc_code_t;
typedef struct mulib_elvi_ipc_event {
  mulib_elvi_ipc_kind_t kind;
  unsigned value; /* COMPLETE: result count; DIAGNOSTIC/FAILURE: safe code. */
  mulib_elvi_script_result_t result; /* RESULT only */
} mulib_elvi_ipc_event_t;
typedef struct mulib_elvi_ipc {
  mulib_ipc_framed_t stream;
  int64_t deadline;
  unsigned results, diagnostic, terminal;
} mulib_elvi_ipc_t;

/* Borrow fd; never closes it. Use one direction and one run per stream.
 * The timeout is absolute from initialization, not renewed per frame. */
int mulib_elvi_ipc_init(mulib_elvi_ipc_t *ipc, int fd, uint64_t timeout_ms);
/* Return 1 if the record fits the v1 UTF-8 wire schema; no I/O. */
int mulib_elvi_ipc_result_valid(const mulib_elvi_script_result_t *result);
int mulib_elvi_ipc_send(mulib_elvi_ipc_t *ipc, const mulib_elvi_ipc_event_t *event);
/* Success is zero, error is MULIB_SB_*; event unchanged on error.
 * Results are tentative until COMPLETE; FAILURE, EOF, and malformed input
 * discard them. An I/O/protocol failure poisons the stream; caller closes fd. */
int mulib_elvi_ipc_receive(mulib_elvi_ipc_t *ipc, mulib_elvi_ipc_event_t *event);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_ELVI_SCRIPT_IPC) && !defined(MULIB_ELVI_SCRIPT_IPC_IMPLEMENTED)
#define MULIB_ELVI_SCRIPT_IPC_IMPLEMENTED
#include "mulib-utf8.h"
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/socket.h>
#endif

int mulib_elvi_ipc_init(mulib_elvi_ipc_t *ipc, int fd, uint64_t timeout_ms)
{
  if (!ipc || fd < 0 || !timeout_ms || timeout_ms > MULIB_ELVI_IPC_MAX_TIMEOUT_MS)
    return MULIB_SB_INVALID;
#if defined(__unix__) || defined(__APPLE__)
  struct sockaddr_storage addr;
  socklen_t n = sizeof addr;
  mulib_elvi_ipc_t tmp = {0};
  int rc;
  if (getpeername(fd, (struct sockaddr *)&addr, &n) || addr.ss_family != AF_UNIX)
    return MULIB_SB_INVALID;
  rc = mulib_ipc_framed_init(&tmp.stream, fd, MULIB_ELVI_IPC_MAX_FRAME);
  if (!rc) rc = mulib_ipc_deadline(timeout_ms, &tmp.deadline);
  if (!rc) *ipc = tmp;
  return rc;
#else
  return MULIB_SB_UNSUPPORTED;
#endif
}

static int mulib_elvi_ipc_text_(const char *s, size_t n)
{
  for (size_t i = 0; i < n; ++i)
    if ((unsigned char)s[i] < 0x20 || (unsigned char)s[i] == 0x7f) return 0;
  return mulib_utf8_validate((const unsigned char *)s, n);
}

int mulib_elvi_ipc_result_valid(const mulib_elvi_script_result_t *result)
{
  if (!result) return 0;
  const char *fields[] = {result->name, result->url, result->price, result->location};
  const size_t caps[] = {sizeof result->name, sizeof result->url,
                         sizeof result->price, sizeof result->location};
  for (size_t i = 0; i < 4; ++i) {
    const char *end = (const char *)memchr(fields[i], '\0', caps[i]);
    if (!end || (end == fields[i] && i < 2) ||
        !mulib_elvi_ipc_text_(fields[i], (size_t)(end - fields[i]))) return 0;
  }
  return 1;
}

static int mulib_elvi_ipc_step_(mulib_elvi_ipc_t *ipc, const mulib_elvi_ipc_event_t *e)
{
  switch (e->kind) {
    case MULIB_ELVI_IPC_RESULT:
      if (ipc->diagnostic || ipc->results >= MULIB_ELVI_SCRIPT_MAX_RESULTS)
        return MULIB_SB_PROTOCOL;
      ++ipc->results;
      break;
    case MULIB_ELVI_IPC_DIAGNOSTIC:
      if (ipc->diagnostic || e->value < MULIB_ELVI_IPC_CONFIG ||
          e->value > MULIB_ELVI_IPC_POLICY) return MULIB_SB_PROTOCOL;
      ipc->diagnostic = e->value;
      break;
    case MULIB_ELVI_IPC_COMPLETE:
      if (ipc->diagnostic || e->value != ipc->results) return MULIB_SB_PROTOCOL;
      ipc->terminal = 1;
      break;
    case MULIB_ELVI_IPC_FAILURE:
      if (!ipc->diagnostic || e->value != ipc->diagnostic) return MULIB_SB_PROTOCOL;
      ipc->terminal = 1;
      break;
    default: return MULIB_SB_PROTOCOL;
  }
  return 0;
}

int mulib_elvi_ipc_send(mulib_elvi_ipc_t *ipc, const mulib_elvi_ipc_event_t *event)
{
  unsigned char wire[MULIB_ELVI_IPC_MAX_FRAME];
  size_t pos = 2;
  mulib_elvi_ipc_t next;
  int rc;
  if (!ipc || !event) return MULIB_SB_INVALID;
  if (ipc->terminal || ipc->stream.poisoned) return MULIB_SB_STATE;
  next = *ipc;
  rc = mulib_elvi_ipc_step_(&next, event);
  if (rc) return rc;
  wire[0] = MULIB_ELVI_IPC_VERSION;
  wire[1] = (unsigned char)event->kind;
  if (event->kind == MULIB_ELVI_IPC_RESULT) {
    const char *f[] = {event->result.name, event->result.url,
                       event->result.price, event->result.location};
    if (!mulib_elvi_ipc_result_valid(&event->result)) return MULIB_SB_INVALID;
    for (size_t i = 0; i < 4; ++i) {
      size_t n = strlen(f[i]);
      wire[pos++] = (unsigned char)(n >> 8);
      wire[pos++] = (unsigned char)n;
      memcpy(wire + pos, f[i], n);
      pos += n;
    }
  } else {
    wire[pos++] = (unsigned char)event->value;
  }
  rc = mulib_ipc_send(&ipc->stream, wire, pos, ipc->deadline);
  if (!rc) *ipc = next;
  return rc;
}

int mulib_elvi_ipc_receive(mulib_elvi_ipc_t *ipc, mulib_elvi_ipc_event_t *event)
{
  unsigned char wire[MULIB_ELVI_IPC_MAX_FRAME];
  mulib_elvi_ipc_event_t tmp = {0};
  size_t size, pos = 2;
  int rc;
  if (!ipc || !event) return MULIB_SB_INVALID;
  if (ipc->terminal || ipc->stream.poisoned) return MULIB_SB_STATE;
  rc = mulib_ipc_receive(&ipc->stream, wire, sizeof wire, &size, ipc->deadline);
  if (rc) return rc;
  if (size < 3 || wire[0] != MULIB_ELVI_IPC_VERSION) goto invalid;
  tmp.kind = (mulib_elvi_ipc_kind_t)wire[1];
  if (tmp.kind == MULIB_ELVI_IPC_RESULT) {
    char *f[] = {tmp.result.name, tmp.result.url, tmp.result.price, tmp.result.location};
    const size_t caps[] = {sizeof tmp.result.name, sizeof tmp.result.url,
                           sizeof tmp.result.price, sizeof tmp.result.location};
    for (size_t i = 0; i < 4; ++i) {
      size_t n;
      if (size - pos < 2) goto invalid;
      n = (size_t)wire[pos] * 256u + wire[pos + 1];
      pos += 2;
      if (n >= caps[i] || n > size - pos || (!n && i < 2) ||
          !mulib_elvi_ipc_text_((const char *)wire + pos, n)) goto invalid;
      memcpy(f[i], wire + pos, n);
      pos += n;
    }
    if (pos != size) goto invalid;
  } else {
    if (size != 3) goto invalid;
    tmp.value = wire[2];
  }
  if (mulib_elvi_ipc_step_(ipc, &tmp)) goto invalid;
  *event = tmp;
  return 0;
invalid:
  ipc->stream.poisoned = 1;
  return MULIB_SB_PROTOCOL;
}
#endif
