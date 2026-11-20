/** @file mulib-secret-policy.h
 * @brief Fail-closed authorization, exact subjects, operation masks, expiry and consent.
 * Define MULIB_IMPL_SECRET_POLICY once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_POLICY_H
#define MULIB_SECRET_POLICY_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-core.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {
    MULIB_SP_READ = 1u, MULIB_SP_GENERATE = 2u, MULIB_SP_SIGN = 4u,
    MULIB_SP_DERIVE = 8u, MULIB_SP_EXPORT = 16u, MULIB_SP_ROTATE = 32u,
    MULIB_SP_DELETE = 64u, MULIB_SP_DECRYPT = 128u, MULIB_SP_REFRESH = 256u,
    MULIB_SP_ENCRYPT = 512u
};
#define MULIB_SP_ALL UINT32_C(1023)
#define MULIB_SP_NONEXPORT UINT32_C(1)
#define MULIB_SP_RECONFIRM UINT32_C(2)
#define MULIB_SP_REVOKED UINT32_C(4)
#define MULIB_SP_MAX_RULES 4096u
#define MULIB_SP_MAX_SUBJECT 127u

typedef struct {
    unsigned char id[16];
    uint32_t flags, capabilities, kind;
    uint64_t expires, revision;
} mulib_sp_object;
typedef struct {
    const char *subject;
    unsigned char id[16];
    uint32_t allow, deny, confirm;
    uint64_t expires;
} mulib_sp_rule;

/* Broker-only: rules and object are an immutable, authenticated snapshot;
 * subject is derived from the authenticated client, never a request field.
 * now is trusted and in the same time domain as both expiry fields. Exact
 * subject/ID only; active denies override all grants. */
int mulib_sp_single_operation(unsigned op);
/* On success, *needs_confirmation is exactly 0 or 1. Success with 1 is a
 * conditional grant, not authority to execute. Output is unchanged on error.
 * NONEXPORT denies READ and EXPORT; it does not constrain trusted callbacks. */
int mulib_sp_evaluate(const mulib_sp_rule *rules, size_t n, const char *subject,
                      const mulib_sp_object *obj, unsigned op, uint64_t now,
                      int *needs_confirmation);
/* v1 compatibility: confirmed must be exactly 0 or 1, broker supplied, and
 * based on consent already bound to this request and consumed by the broker. */
int mulib_sp_check(const mulib_sp_rule *rules, size_t n, const char *subject,
                   const mulib_sp_object *obj, unsigned op, uint64_t now,
                   int confirmed);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_POLICY) && !defined(MULIB_SECRET_POLICY_IMPLEMENTED)
#define MULIB_SECRET_POLICY_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_sp_single_operation(unsigned op)
{
    return op && !(op & (op - 1u)) && !(op & ~MULIB_SP_ALL);
}

int mulib_sp_evaluate(const mulib_sp_rule *rules, size_t n, const char *subject,
                      const mulib_sp_object *obj, unsigned op, uint64_t now,
                      int *needs_confirmation)
{
    unsigned allow = 0, deny = 0, confirm = 0;
    const uint32_t known_flags = MULIB_SP_NONEXPORT | MULIB_SP_RECONFIRM |
                                 MULIB_SP_REVOKED;

    if ((!rules && n) || n > MULIB_SP_MAX_RULES || !obj || !needs_confirmation ||
        !mulib_sc_name(subject, MULIB_SP_MAX_SUBJECT) || !obj->revision ||
        !obj->kind || !mulib_sp_single_operation(op))
        return MULIB_SC_INVALID;
    if ((obj->flags & ~known_flags) || (obj->capabilities & ~MULIB_SP_ALL) ||
        (obj->flags & MULIB_SP_REVOKED) || (obj->expires && now >= obj->expires) ||
        !(obj->capabilities & op))
        return MULIB_SC_DENIED;
    if ((obj->flags & MULIB_SP_NONEXPORT) && (op & (MULIB_SP_EXPORT | MULIB_SP_READ)))
        return MULIB_SC_DENIED;

    for (size_t i = 0; i < n; ++i) {
        const mulib_sp_rule *rule = &rules[i];
        if (!mulib_sc_name(rule->subject, MULIB_SP_MAX_SUBJECT) ||
            ((rule->allow | rule->deny | rule->confirm) & ~MULIB_SP_ALL))
            return MULIB_SC_INVALID;
        if (strcmp(rule->subject, subject) ||
            memcmp(rule->id, obj->id, sizeof obj->id) ||
            (rule->expires && now >= rule->expires))
            continue;
        allow |= rule->allow;
        deny |= rule->deny;
        confirm |= rule->confirm;
    }

    if ((deny & op) || !(allow & op))
        return MULIB_SC_DENIED;
    *needs_confirmation = (confirm & op) != 0 || (obj->flags & MULIB_SP_RECONFIRM) != 0;
    return MULIB_SC_OK;
}

int mulib_sp_check(const mulib_sp_rule *rules, size_t n, const char *subject,
                   const mulib_sp_object *obj, unsigned op, uint64_t now,
                   int confirmed)
{
    int needs_confirmation, rc;
    if (confirmed != 0 && confirmed != 1)
        return MULIB_SC_INVALID;
    rc = mulib_sp_evaluate(rules, n, subject, obj, op, now, &needs_confirmation);
    if (rc)
        return rc;
    return needs_confirmation && !confirmed ? MULIB_SC_DENIED : MULIB_SC_OK;
}
#ifdef __cplusplus
}
#endif
#endif
