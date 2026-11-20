/** @file mulib-secret-lease.h
 * @brief Broker-owned one-shot operation approvals with random tokens.
 * Define MULIB_IMPL_SECRET_LEASE once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_LEASE_H
#define MULIB_SECRET_LEASE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-crypto.h"
#include "mulib-secret-policy.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_SL_MAX_TTL 300u
#define MULIB_SL_MAX_CANONICAL UINT32_C(16777216)
#define MULIB_SL_MAX_PROVIDER 63u

typedef struct {
    unsigned char token[32], object[16], digest[32];
    uint64_t revision, expires;
    uint32_t operation;
    char subject[MULIB_SP_MAX_SUBJECT + 1u];
    int active;
} mulib_sl_approval;

/* v1 compatibility: does not bind vault, session, provider, policy generation
 * or destination. Do not use v1 as the authorization boundary of a service.
 * Zero-initialize, serialize and never copy an active lease; only token may
 * overlap its storage. Use trusted monotonic seconds and preserve clock epoch. */
int mulib_sl_issue(const mulib_scr_provider *p, mulib_sl_approval *a,
                   const char *subject, const mulib_sp_object *obj, unsigned op,
                   mulib_sc_view canonical, uint64_t now, uint32_t ttl);
int mulib_sl_consume(const mulib_scr_provider *p, mulib_sl_approval *a,
                     const unsigned char token[32], const char *subject,
                     const mulib_sp_object *obj, unsigned op,
                     mulib_sc_view canonical, uint64_t now);
void mulib_sl_cancel(mulib_sl_approval *a);

/* All scope fields are resolved by the trusted broker, not supplied as
 * authority by a client. IDs are nonzero, with a fresh session ID per login;
 * destination identifies the exact delivery endpoint/context, including for
 * local results. Advance policy_revision on grant or policy changes. */
typedef struct {
    unsigned char vault[16], session[16], destination[16];
    uint64_t policy_revision;
    uint32_t request_schema, result_schema;
    char provider[MULIB_SL_MAX_PROVIDER + 1u];
} mulib_sl_scope_v2;

typedef struct {
    unsigned char token[32], object[16], digest[32];
    uint64_t revision, issued, expires;
    uint32_t kind, operation;
    char subject[MULIB_SP_MAX_SUBJECT + 1u];
    mulib_sl_scope_v2 scope;
    int active;
} mulib_sl_approval_v2;

/* Zero-initialize, serialize and never copy an active approval. Only issue
 * after the trusted UI confirms the exact canonical bytes and scope. now is
 * trusted monotonic seconds from the same clock; TTL is in [1,300]. A matching
 * token burns the slot before request hashing, including on mismatch/error.
 * This is not a global replay ledger: the broker owns one canonical slot per
 * token, enforces unique lookup, and invalidates slots on session close/lock.
 * Treat tokens as secrets: never log them. A consumed token never authorizes
 * a retry after an uncertain effect.
 * Inputs must not overlap the approval, except token may point at its token. */
int mulib_sl_scope_valid_v2(const mulib_sl_scope_v2 *scope);
int mulib_sl_issue_v2(const mulib_scr_provider *p, mulib_sl_approval_v2 *a,
                      const mulib_sl_scope_v2 *scope, const char *subject,
                      const mulib_sp_object *obj, unsigned op,
                      mulib_sc_view canonical, uint64_t now, uint32_t ttl);
int mulib_sl_consume_v2(const mulib_scr_provider *p, mulib_sl_approval_v2 *a,
                        const unsigned char token[32], const mulib_sl_scope_v2 *scope,
                        const char *subject, const mulib_sp_object *obj,
                        unsigned op, mulib_sc_view canonical, uint64_t now);
void mulib_sl_cancel_v2(mulib_sl_approval_v2 *a);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_LEASE) && !defined(MULIB_SECRET_LEASE_IMPLEMENTED)
#define MULIB_SECRET_LEASE_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_sl_request_valid_(mulib_sc_view req)
{
    return req.len <= MULIB_SL_MAX_CANONICAL && mulib_sc_view_ok(req);
}

static int mulib_sl_binding_valid_(const char *subject, const mulib_sp_object *obj,
                                   unsigned op)
{
    return obj && obj->revision && obj->kind && mulib_sp_single_operation(op) &&
           mulib_sc_name(subject, MULIB_SP_MAX_SUBJECT);
}

static int mulib_sl_binding_matches_(const unsigned char id[16], uint64_t revision,
                                     unsigned operation, const char *saved_subject,
                                     const char *subject, const mulib_sp_object *obj,
                                     unsigned op)
{
    return mulib_sl_binding_valid_(subject, obj, op) &&
           mulib_sc_name(saved_subject, MULIB_SP_MAX_SUBJECT) &&
           revision == obj->revision && operation == op && !memcmp(id, obj->id, 16) &&
           !strcmp(saved_subject, subject);
}

static int mulib_sl_issue_valid_(const char *subject, const mulib_sp_object *obj,
                                 unsigned op, mulib_sc_view req, uint64_t now, uint32_t ttl)
{
    return mulib_sl_binding_valid_(subject, obj, op) && mulib_sl_request_valid_(req) &&
           ttl && ttl <= MULIB_SL_MAX_TTL && now <= UINT64_MAX - ttl;
}

static int mulib_sl_generate_(const mulib_scr_provider *p, mulib_sc_view req,
                               unsigned char token[32], unsigned char digest[32])
{
    int rc = mulib_scr_random(p, token, 32);
    return rc ? rc : mulib_scr_hash(p, MULIB_SCR_SHA256, req, digest, 32);
}

static int mulib_sl_verify_digest_(const mulib_scr_provider *p, mulib_sc_view req,
                                   const unsigned char expected[32])
{
    unsigned char digest[32];
    int rc = mulib_scr_hash(p, MULIB_SCR_SHA256, req, digest, sizeof digest);
    if (!rc && !mulib_sc_equal(digest, expected, sizeof digest))
        rc = MULIB_SC_DENIED;
    mulib_sc_wipe(digest, sizeof digest);
    return rc;
}

void mulib_sl_cancel(mulib_sl_approval *a)
{
    if (a)
        mulib_sc_wipe(a, sizeof *a);
}

int mulib_sl_issue(const mulib_scr_provider *p, mulib_sl_approval *a,
                   const char *subject, const mulib_sp_object *obj, unsigned op,
                   mulib_sc_view req, uint64_t now, uint32_t ttl)
{
    mulib_sl_approval tmp;
    int rc;
    if (!a || a->active || !mulib_sl_issue_valid_(subject, obj, op, req, now, ttl) ||
        mulib_sc_overlap(a, sizeof *a, req.ptr, req.len))
        return MULIB_SC_INVALID;

    memset(&tmp, 0, sizeof tmp);
    rc = mulib_sl_generate_(p, req, tmp.token, tmp.digest);
    if (!rc) {
        memcpy(tmp.object, obj->id, sizeof tmp.object);
        memcpy(tmp.subject, subject, strlen(subject) + 1u);
        tmp.revision = obj->revision;
        tmp.operation = op;
        tmp.expires = now + ttl;
        tmp.active = 1;
        *a = tmp;
    }
    mulib_sl_cancel(&tmp);
    return rc;
}

int mulib_sl_consume(const mulib_scr_provider *p, mulib_sl_approval *a,
                     const unsigned char token[32], const char *subject,
                     const mulib_sp_object *obj, unsigned op,
                     mulib_sc_view req, uint64_t now)
{
    unsigned char expected[32];
    int rc;
    if (!a || a->active != 1)
        return MULIB_SC_DENIED;
    if (now >= a->expires) {
        mulib_sl_cancel(a);
        return MULIB_SC_DENIED;
    }
    if (!mulib_sc_equal(a->token, token, sizeof a->token))
        return MULIB_SC_DENIED;
    if (!mulib_sl_binding_matches_(a->object, a->revision, a->operation, a->subject,
                                    subject, obj, op) || !mulib_sl_request_valid_(req) ||
        mulib_sc_overlap(a, sizeof *a, req.ptr, req.len)) {
        mulib_sl_cancel(a);
        return MULIB_SC_DENIED;
    }
    memcpy(expected, a->digest, sizeof expected);
    mulib_sl_cancel(a);
    rc = mulib_sl_verify_digest_(p, req, expected);
    mulib_sc_wipe(expected, sizeof expected);
    return rc;
}

int mulib_sl_scope_valid_v2(const mulib_sl_scope_v2 *scope)
{
    const unsigned char zero[16] = {0};
    return scope && scope->policy_revision && scope->request_schema &&
           scope->result_schema && mulib_sc_name(scope->provider, MULIB_SL_MAX_PROVIDER) &&
           memcmp(scope->vault, zero, sizeof zero) &&
           memcmp(scope->session, zero, sizeof zero) &&
           memcmp(scope->destination, zero, sizeof zero);
}

static int mulib_sl_scope_matches_(const mulib_sl_scope_v2 *a, const mulib_sl_scope_v2 *b)
{
    return mulib_sl_scope_valid_v2(a) && mulib_sl_scope_valid_v2(b) &&
           a->policy_revision == b->policy_revision &&
           a->request_schema == b->request_schema && a->result_schema == b->result_schema &&
           !memcmp(a->vault, b->vault, sizeof a->vault) &&
           !memcmp(a->session, b->session, sizeof a->session) &&
           !memcmp(a->destination, b->destination, sizeof a->destination) &&
           !strcmp(a->provider, b->provider);
}

void mulib_sl_cancel_v2(mulib_sl_approval_v2 *a)
{
    if (a)
        mulib_sc_wipe(a, sizeof *a);
}

int mulib_sl_issue_v2(const mulib_scr_provider *p, mulib_sl_approval_v2 *a,
                      const mulib_sl_scope_v2 *scope, const char *subject,
                      const mulib_sp_object *obj, unsigned op,
                      mulib_sc_view req, uint64_t now, uint32_t ttl)
{
    mulib_sl_approval_v2 tmp;
    int rc;
    if (!a || a->active || !mulib_sl_issue_valid_(subject, obj, op, req, now, ttl) ||
        mulib_sc_overlap(a, sizeof *a, scope, sizeof *scope) ||
        !mulib_sl_scope_valid_v2(scope) || mulib_sc_overlap(a, sizeof *a, req.ptr, req.len))
        return MULIB_SC_INVALID;
    memset(&tmp, 0, sizeof tmp);
    rc = mulib_sl_generate_(p, req, tmp.token, tmp.digest);
    if (!rc) {
        tmp.scope = *scope;
        memcpy(tmp.object, obj->id, sizeof tmp.object);
        memcpy(tmp.subject, subject, strlen(subject) + 1u);
        tmp.revision = obj->revision;
        tmp.kind = obj->kind;
        tmp.operation = op;
        tmp.issued = now;
        tmp.expires = now + ttl;
        tmp.active = 1;
        *a = tmp;
    }
    mulib_sl_cancel_v2(&tmp);
    return rc;
}

int mulib_sl_consume_v2(const mulib_scr_provider *p, mulib_sl_approval_v2 *a,
                        const unsigned char token[32], const mulib_sl_scope_v2 *scope,
                        const char *subject, const mulib_sp_object *obj,
                        unsigned op, mulib_sc_view req, uint64_t now)
{
    unsigned char expected[32];
    int rc;
    if (!a || a->active != 1)
        return MULIB_SC_DENIED;
    if (now < a->issued || now >= a->expires) {
        mulib_sl_cancel_v2(a);
        return MULIB_SC_DENIED;
    }
    if (!mulib_sc_equal(a->token, token, sizeof a->token))
        return MULIB_SC_DENIED;
    if (mulib_sc_overlap(a, sizeof *a, scope, sizeof *scope) ||
        !mulib_sl_scope_matches_(&a->scope, scope) ||
        !mulib_sl_binding_matches_(a->object, a->revision, a->operation, a->subject,
                                    subject, obj, op) ||
        a->kind != obj->kind || !mulib_sl_request_valid_(req) ||
        mulib_sc_overlap(a, sizeof *a, req.ptr, req.len)) {
        mulib_sl_cancel_v2(a);
        return MULIB_SC_DENIED;
    }
    memcpy(expected, a->digest, sizeof expected);
    mulib_sl_cancel_v2(a);
    rc = mulib_sl_verify_digest_(p, req, expected);
    mulib_sc_wipe(expected, sizeof expected);
    return rc;
}
#ifdef __cplusplus
}
#endif
#endif
