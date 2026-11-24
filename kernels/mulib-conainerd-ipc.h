/** @file mulib-conainerd-ipc.h
 * @brief Container daemon command/status records over the common IPC envelope.
 * The historical conainerd spelling is intentional. Define MULIB_IMPL_CONAINERD_IPC. */
#ifndef MULIB_CONAINERD_IPC_H
#define MULIB_CONAINERD_IPC_H
#include "mulib-container-ipc.h"
#include "mulib-container.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_conainerd_opcode
{
  MULIB_CONAINERD_START = 1, MULIB_CONAINERD_STOP, MULIB_CONAINERD_DELETE,
  MULIB_CONAINERD_STATUS, MULIB_CONAINERD_PAUSE, MULIB_CONAINERD_RESUME,
  MULIB_CONAINERD_STEP
} mulib_conainerd_opcode;
typedef struct mulib_conainerd_command
{
  mulib_conainerd_opcode opcode;
  uint64_t request_id, expected_generation, now, value;
  char id[MULIB_CT_ID_CAP];
} mulib_conainerd_command;
typedef struct mulib_conainerd_status
{
  mulib_container_state state;
  int exit_code;
  uint64_t generation, starts, started_at, exited_at, polls;
} mulib_conainerd_status;
/** @brief Encode a fixed 96-byte command payload. Generation zero disables CAS.
 * STOP value is signal 1..64; STEP value is a positive work budget; other values zero. */
int mulib_conainerd_ipc_command (const mulib_conainerd_command *command, mulib_ct_blob *out);
/** @brief Parse a decoded request with canonical zero padding and value checks. */
int mulib_conainerd_ipc_parse (const mulib_container_ipc_message *message,
                              mulib_conainerd_command *command);
/** @brief Encode a 48-byte status on success, empty payload on negative CT error. */
int mulib_conainerd_ipc_reply (const mulib_conainerd_command *command, int result,
                              const mulib_conainerd_status *status, mulib_ct_blob *out);
/** @brief Validate response correlation and decode status. Remote errors are
 * returned directly; output remains unchanged for failed responses. */
int mulib_conainerd_ipc_status (const mulib_container_ipc_message *message,
                               const mulib_conainerd_command *command,
                               mulib_conainerd_status *status);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONAINERD_IPC) && !defined(MULIB_CONAINERD_IPC_IMPLEMENTED)
#define MULIB_CONAINERD_IPC_IMPLEMENTED
static int mulib_conainerd_valid_ (const mulib_conainerd_command *c)
{
  if (!c || !mulib_ct_id_valid (c->id) || c->opcode < MULIB_CONAINERD_START
      || c->opcode > MULIB_CONAINERD_STEP) return 0;
  if (c->opcode == MULIB_CONAINERD_STOP) return c->value >= 1 && c->value <= 64;
  if (c->opcode == MULIB_CONAINERD_STEP) return c->value && c->value <= SIZE_MAX;
  return c->value == 0;
}
int
mulib_conainerd_ipc_command (const mulib_conainerd_command *c, mulib_ct_blob *out)
{
  unsigned char p[96] = { 0 };
  mulib_container_ipc_message m;
  if (!mulib_conainerd_valid_ (c) || !out) return MULIB_CT_EINVAL;
  memcpy (p, c->id, strlen (c->id));
  mulib_ct_store64 (p + 64, c->expected_generation);
  mulib_ct_store64 (p + 72, c->now); mulib_ct_store64 (p + 80, c->value);
  memset (&m, 0, sizeof (m)); m.opcode = (uint16_t)c->opcode;
  m.request_id = c->request_id; m.data = p; m.length = sizeof (p);
  return mulib_container_ipc_encode (&m, out, sizeof (p));
}
int
mulib_conainerd_ipc_parse (const mulib_container_ipc_message *m, mulib_conainerd_command *out)
{
  mulib_conainerd_command c;
  size_t i, n;
  if (!m || !out) return MULIB_CT_EINVAL;
  if (m->flags || m->status || m->length != 96 || !m->data) return MULIB_CT_EFORMAT;
  memset (&c, 0, sizeof (c)); memcpy (c.id, m->data, 64);
  if (mulib_ct_text_length (c.id, 63, &n)) return MULIB_CT_EFORMAT;
  for (i = n; i < 64; ++i) if (m->data[i]) return MULIB_CT_EFORMAT;
  for (i = 88; i < 96; ++i) if (m->data[i]) return MULIB_CT_EFORMAT;
  c.opcode = (mulib_conainerd_opcode)m->opcode; c.request_id = m->request_id;
  c.expected_generation = mulib_ct_load64 (m->data + 64);
  c.now = mulib_ct_load64 (m->data + 72); c.value = mulib_ct_load64 (m->data + 80);
  if (!mulib_conainerd_valid_ (&c)) return MULIB_CT_EFORMAT;
  *out = c;
  return 0;
}
int
mulib_conainerd_ipc_reply (const mulib_conainerd_command *c, int result,
                          const mulib_conainerd_status *s, mulib_ct_blob *out)
{
  unsigned char p[48] = { 0 };
  mulib_container_ipc_message m;
  if (!c || !out || (!result && !s) || result > 0 || result < MULIB_CT_ECHECKSUM)
    return MULIB_CT_EINVAL;
  memset (&m, 0, sizeof (m)); m.opcode = (uint16_t)c->opcode;
  m.request_id = c->request_id; m.flags = MULIB_CONTAINER_IPC_RESPONSE; m.status = result;
  if (!result)
    {
      mulib_ct_store32 (p, (uint32_t)s->state); mulib_ct_store32 (p + 4, (uint32_t)s->exit_code);
      mulib_ct_store64 (p + 8, s->generation); mulib_ct_store64 (p + 16, s->starts);
      mulib_ct_store64 (p + 24, s->started_at); mulib_ct_store64 (p + 32, s->exited_at);
      mulib_ct_store64 (p + 40, s->polls); m.data = p; m.length = sizeof (p);
    }
  return mulib_container_ipc_encode (&m, out, sizeof (p));
}
int
mulib_conainerd_ipc_status (const mulib_container_ipc_message *m,
                           const mulib_conainerd_command *c, mulib_conainerd_status *out)
{
  mulib_conainerd_status s;
  uint32_t code, state;
  if (!m || !c || !out) return MULIB_CT_EINVAL;
  if (m->flags != MULIB_CONTAINER_IPC_RESPONSE || m->opcode != c->opcode
      || m->request_id != c->request_id || m->status > 0
      || m->status < MULIB_CT_ECHECKSUM) return MULIB_CT_EFORMAT;
  if (m->status) return m->length ? MULIB_CT_EFORMAT : m->status;
  if (m->length != 48 || !m->data) return MULIB_CT_EFORMAT;
  state = mulib_ct_load32 (m->data); code = mulib_ct_load32 (m->data + 4);
  if (state < MULIB_CONTAINER_CREATED || state > MULIB_CONTAINER_DELETED) return MULIB_CT_EFORMAT;
  s.state = (mulib_container_state)state;
  s.exit_code = code <= INT32_MAX ? (int)code : -1 - (int)(UINT32_MAX - code);
  s.generation = mulib_ct_load64 (m->data + 8); s.starts = mulib_ct_load64 (m->data + 16);
  s.started_at = mulib_ct_load64 (m->data + 24); s.exited_at = mulib_ct_load64 (m->data + 32);
  s.polls = mulib_ct_load64 (m->data + 40); *out = s;
  return 0;
}
#endif
