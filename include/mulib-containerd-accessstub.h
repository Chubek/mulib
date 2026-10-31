/** @file mulib-containerd-accessstub.h
 * @brief Explicit principal/command authorization and bounded audit counters.
 * Define MULIB_IMPL_CONTAINERD_ACCESSSTUB plus CONAINERD_IPC dependencies. */
#ifndef MULIB_CONTAINERD_ACCESSSTUB_H
#define MULIB_CONTAINERD_ACCESSSTUB_H
#include "mulib-conainerd-ipc.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { MULIB_CONTAINERD_ACCESS_ALL = 0x7f };
/** @brief Identity is supplied by an authenticated transport, never by a command body. */
typedef struct mulib_containerd_principal
{
  uint64_t subject;
  uint32_t uid, gid;
  int authenticated;
} mulib_containerd_principal;
/** @brief First matching rule wins. Empty container_id matches every container.
 * any_subject bypasses subject equality, not authentication. */
typedef struct mulib_containerd_access_rule
{
  uint64_t subject;
  char container_id[MULIB_CT_ID_CAP];
  uint32_t operations;
  int any_subject, allow;
} mulib_containerd_access_rule;
typedef struct mulib_containerd_accessstub
{
  mulib_containerd_access_rule *rules;
  size_t count, capacity;
  uint64_t allowed, denied;
  int default_allow;
} mulib_containerd_accessstub;
/** @brief Initialize bounded policy with explicit fallback. */
int mulib_containerd_accessstub_init (mulib_containerd_accessstub *policy,
                                      size_t capacity, int default_allow);
/** @brief Release rule storage. */
void mulib_containerd_accessstub_free (mulib_containerd_accessstub *policy);
/** @brief Append copied rule; operation bit N-1 corresponds to opcode N. */
int mulib_containerd_accessstub_add (mulib_containerd_accessstub *policy,
                                     const mulib_containerd_access_rule *rule);
/** @brief Remove one rule by index; preserves ordering of remaining rules. */
int mulib_containerd_accessstub_remove (mulib_containerd_accessstub *policy, size_t index);
/** @brief Check one command. Unauthenticated identities always return EACCESS.
 * matched is SIZE_MAX for fallback/invalid/unauthenticated requests. */
int mulib_containerd_accessstub_check (mulib_containerd_accessstub *policy,
    const mulib_containerd_principal *principal, const mulib_conainerd_command *command,
    size_t *matched);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINERD_ACCESSSTUB) && !defined(MULIB_CONTAINERD_ACCESSSTUB_IMPLEMENTED)
#define MULIB_CONTAINERD_ACCESSSTUB_IMPLEMENTED
int mulib_containerd_accessstub_init (mulib_containerd_accessstub *p, size_t capacity, int allow)
{
  if (!p || capacity > SIZE_MAX / sizeof (*p->rules) || (allow != 0 && allow != 1))
    return MULIB_CT_EINVAL;
  memset (p, 0, sizeof (*p)); p->capacity = capacity; p->default_allow = allow;
  return 0;
}
void mulib_containerd_accessstub_free (mulib_containerd_accessstub *p)
{
  if (p) { MULIB_CT_FREE (p->rules); memset (p, 0, sizeof (*p)); }
}
int
mulib_containerd_accessstub_add (mulib_containerd_accessstub *p,
                                const mulib_containerd_access_rule *rule)
{
  mulib_containerd_access_rule copy, *next;
  if (!p || !rule || (rule->container_id[0] && !mulib_ct_id_valid (rule->container_id))
      || !rule->operations || (rule->operations & ~MULIB_CONTAINERD_ACCESS_ALL)
      || (rule->allow != 0 && rule->allow != 1)
      || (rule->any_subject != 0 && rule->any_subject != 1)) return MULIB_CT_EINVAL;
  if (p->count >= p->capacity) return MULIB_CT_ELIMIT;
  copy = *rule;
  next = (mulib_containerd_access_rule *)MULIB_CT_REALLOC (p->rules, (p->count + 1) * sizeof (*next));
  if (!next) return MULIB_CT_ENOMEM;
  p->rules = next; next[p->count++] = copy;
  return 0;
}
int mulib_containerd_accessstub_remove (mulib_containerd_accessstub *p, size_t i)
{
  if (!p) return MULIB_CT_EINVAL;
  if (i >= p->count) return MULIB_CT_ENOENT;
  memmove (p->rules + i, p->rules + i + 1, (--p->count - i) * sizeof (*p->rules));
  return 0;
}
int
mulib_containerd_accessstub_check (mulib_containerd_accessstub *p,
    const mulib_containerd_principal *principal, const mulib_conainerd_command *c,
    size_t *matched)
{
  size_t i;
  int allow;
  if (matched) *matched = SIZE_MAX;
  if (!p || !principal || !c || !mulib_ct_id_valid (c->id)
      || c->opcode < 1 || c->opcode > MULIB_CONAINERD_STEP) return MULIB_CT_EINVAL;
  allow = principal->authenticated ? p->default_allow : 0;
  if (principal->authenticated)
    for (i = 0; i < p->count; ++i)
      {
        const mulib_containerd_access_rule *r = p->rules + i;
        if ((!r->any_subject && r->subject != principal->subject)
            || !(r->operations & (1U << (c->opcode - 1)))
            || (r->container_id[0] && strcmp (r->container_id, c->id))) continue;
        allow = r->allow; if (matched) *matched = i; break;
      }
  if (allow) { p->allowed = mulib_ct_counter_add (p->allowed, 1); return 0; }
  p->denied = mulib_ct_counter_add (p->denied, 1);
  return MULIB_CT_EACCESS;
}
#endif
