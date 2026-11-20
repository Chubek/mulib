/** @file mulib-secret-provider.h
 * @brief Versioned provider catalogue and policy-mediated operation dispatch.
 * Define MULIB_IMPL_SECRET_PROVIDER once. Dependencies need their own implementation switches.
 * Dispatch v2 requires SECRET_LEASE, SECRET_CRYPTO, SECRET_POLICY and SECRET_CORE.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_PROVIDER_H
#define MULIB_SECRET_PROVIDER_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-lease.h"
#include "mulib-secret-policy.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_SPRO_MAX 128u
#define MULIB_SPRO_MAX_SPAN MULIB_SL_MAX_CANONICAL
#define MULIB_SPRO_MAX_OPERATIONS 10u
#define MULIB_SPRO_ABI_V2 2u

typedef struct { unsigned op; uint64_t now; mulib_sc_view input; } mulib_spro_request;
typedef int (*mulib_spro_run)(void *ctx,const mulib_sp_object*,const mulib_spro_request*,mulib_sc_buf,size_t*);
typedef struct { uint32_t abi,kind,operations; const char *name; void *ctx; mulib_spro_run run; } mulib_spro_entry;
typedef struct { mulib_spro_entry entries[MULIB_SPRO_MAX]; size_t count; int frozen,busy; } mulib_spro_registry;

/* Trusted built-ins only. Zero-initialize and externally serialize the
 * registry; registrations borrow immutable names and long-lived context.
 * Buffers, metadata and written must not overlap. On any error, *written is
 * unchanged; on callback error/over-reporting the callback buffer is wiped. */
int mulib_spro_register(mulib_spro_registry *r, const mulib_spro_entry *e);
int mulib_spro_freeze(mulib_spro_registry *r);
/* v1 compatibility only: the consent bit must come from trusted broker code. */
int mulib_spro_call(mulib_spro_registry *r, uint32_t kind,
                    const mulib_sp_rule *rules, size_t n, const char *subject,
                    const mulib_sp_object *obj, const mulib_spro_request *req,
                    int consent, mulib_sc_buf out, size_t *written);

typedef int (*mulib_spro_validate_v2)(void *ctx, mulib_sc_view value);
typedef struct {
    unsigned operation;
    uint32_t request_schema, result_schema;
    size_t max_input, max_output;
    mulib_spro_validate_v2 validate_request, validate_result;
    mulib_spro_run run;
} mulib_spro_operation_v2;

typedef struct {
    uint32_t abi, size, kind; /* ABI_V2, sizeof(mulib_spro_entry_v2), nonzero kind */
    const char *name;
    void *ctx;
    const mulib_spro_operation_v2 *operations;
    size_t operation_count;
} mulib_spro_entry_v2;

typedef struct {
    mulib_spro_entry_v2 entries[MULIB_SPRO_MAX];
    size_t count;
    int frozen, busy;
} mulib_spro_registry_v2;

/* Same trust/ownership rules as v1; v2 also borrows the immutable operation
 * table. Schema IDs are provider-defined format/version identifiers. Each
 * operation needs side-effect-free validators, including for empty values. */
int mulib_spro_register_v2(mulib_spro_registry_v2 *r, const mulib_spro_entry_v2 *e);
int mulib_spro_freeze_v2(mulib_spro_registry_v2 *r);
/* Broker only. Hold the authenticated object, policy, request bytes, scope
 * and lease stable under service synchronization. req->input is exactly the
 * canonical byte string shown for approval; req->now is trusted monotonic
 * time in the lease and policy expiry domain. If policy requires consent,
 * approval and token are mandatory; an optional supplied pair is consumed
 * too. Size/schema/capacity preflight happens before lease consumption. The
 * private staging buffer must fit the configured maximum result size, and
 * must not be visible to clients before success. After successful lease
 * consumption, any error (including result validation after an effect) still
 * burns the lease. Once run reports success, over-reporting or result
 * rejection returns UNCERTAIN: the broker must reconcile before retrying.
 * Callbacks must also report uncertain effects themselves and must not
 * deliver results directly. The broker, not this registry, enforces identity,
 * destination, lifecycle, audit/durability and safe delivery. Callbacks are
 * NOT sandboxed. On run or result failure the slice passed to run is wiped. */
int mulib_spro_call_v2(mulib_spro_registry_v2 *r, const mulib_scr_provider *crypto,
                       const mulib_sp_rule *rules, size_t n, const char *subject,
                       const mulib_sp_object *obj, const mulib_spro_request *req,
                       const mulib_sl_scope_v2 *scope, mulib_sl_approval_v2 *approval,
                       const unsigned char token[32], mulib_sc_buf out, size_t *written);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_PROVIDER) && !defined(MULIB_SECRET_PROVIDER_IMPLEMENTED)
#define MULIB_SECRET_PROVIDER_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_spro_range_args_(const mulib_spro_request *q, mulib_sc_buf out,
                                  const size_t *written)
{
    return q && written && (out.ptr || !out.cap) && out.cap <= MULIB_SPRO_MAX_SPAN &&
           q->input.len <= MULIB_SPRO_MAX_SPAN && mulib_sc_view_ok(q->input) &&
           !mulib_sc_overlap(q, sizeof *q, out.ptr, out.cap) &&
           !mulib_sc_overlap(q, sizeof *q, written, sizeof *written) &&
           !mulib_sc_overlap(q->input.ptr, q->input.len, out.ptr, out.cap) &&
           !mulib_sc_overlap(written, sizeof *written, out.ptr, out.cap) &&
           !mulib_sc_overlap(written, sizeof *written, q->input.ptr, q->input.len);
}

static int mulib_spro_callback_status_(int rc)
{
    return rc > 0 ? MULIB_SC_STATE : rc;
}

static int mulib_spro_entry_valid_(const mulib_spro_entry *e)
{
    return e->abi == 1 && e->kind && e->run && e->operations &&
           !(e->operations & ~MULIB_SP_ALL) && mulib_sc_name(e->name, MULIB_SL_MAX_PROVIDER);
}

int mulib_spro_register(mulib_spro_registry *r, const mulib_spro_entry *e)
{
    if (!r || !e || r->frozen || r->busy || r->count >= MULIB_SPRO_MAX ||
        !mulib_spro_entry_valid_(e))
        return MULIB_SC_INVALID;
    for (size_t i = 0; i < r->count; ++i) {
        if (!mulib_spro_entry_valid_(&r->entries[i]))
            return MULIB_SC_STATE;
        if (r->entries[i].kind == e->kind || !strcmp(r->entries[i].name, e->name))
            return MULIB_SC_CONFLICT;
    }
    r->entries[r->count++] = *e;
    return MULIB_SC_OK;
}

int mulib_spro_freeze(mulib_spro_registry *r)
{
    if (!r || r->busy || !r->count || r->count > MULIB_SPRO_MAX)
        return MULIB_SC_STATE;
    for (size_t i = 0; i < r->count; ++i)
        if (!mulib_spro_entry_valid_(&r->entries[i]))
            return MULIB_SC_STATE;
    r->frozen = 1;
    return MULIB_SC_OK;
}

int mulib_spro_call(mulib_spro_registry *r, uint32_t kind,
                    const mulib_sp_rule *rules, size_t n, const char *subject,
                    const mulib_sp_object *obj, const mulib_spro_request *q,
                    int consent, mulib_sc_buf out, size_t *written)
{
    int rc;
    if (!r || r->frozen != 1 || r->busy || !r->count || r->count > MULIB_SPRO_MAX ||
        !obj || kind != obj->kind || !out.ptr || !mulib_spro_range_args_(q, out, written))
        return MULIB_SC_INVALID;
    rc = mulib_sp_check(rules, n, subject, obj, q->op, q->now, consent);
    if (rc)
        return rc;

    for (size_t i = 0; i < r->count; ++i) {
        const mulib_spro_entry *e = &r->entries[i];
        size_t w = 0;
        if (e->kind != kind)
            continue;
        if (!mulib_spro_entry_valid_(e))
            return MULIB_SC_STATE;
        if (!(e->operations & q->op))
            return MULIB_SC_UNSUPPORTED;
        r->busy = 1;
        rc = mulib_spro_callback_status_(e->run(e->ctx, obj, q, out, &w));
        r->busy = 0;
        if (rc || w > out.cap) {
            mulib_sc_wipe(out.ptr, out.cap);
            return rc ? rc : MULIB_SC_LIMIT;
        }
        *written = w;
        return MULIB_SC_OK;
    }
    return MULIB_SC_NOTFOUND;
}

static int mulib_spro_entry_valid_v2_(const mulib_spro_entry_v2 *e)
{
    unsigned seen = 0;
    if (e->abi != MULIB_SPRO_ABI_V2 || e->size != sizeof *e || !e->kind ||
        !mulib_sc_name(e->name, MULIB_SL_MAX_PROVIDER) || !e->operations ||
        !e->operation_count || e->operation_count > MULIB_SPRO_MAX_OPERATIONS)
        return 0;
    for (size_t i = 0; i < e->operation_count; ++i) {
        const mulib_spro_operation_v2 *op = &e->operations[i];
        if (!mulib_sp_single_operation(op->operation) || (seen & op->operation) ||
            !op->request_schema || !op->result_schema || !op->validate_request ||
            !op->validate_result || !op->run || op->max_input > MULIB_SPRO_MAX_SPAN ||
            op->max_output > MULIB_SPRO_MAX_SPAN)
            return 0;
        seen |= op->operation;
    }
    return 1;
}

int mulib_spro_register_v2(mulib_spro_registry_v2 *r, const mulib_spro_entry_v2 *e)
{
    if (!r || !e || r->frozen || r->busy || r->count >= MULIB_SPRO_MAX ||
        !mulib_spro_entry_valid_v2_(e))
        return MULIB_SC_INVALID;
    for (size_t i = 0; i < r->count; ++i) {
        if (!mulib_spro_entry_valid_v2_(&r->entries[i]))
            return MULIB_SC_STATE;
        if (r->entries[i].kind == e->kind || !strcmp(r->entries[i].name, e->name))
            return MULIB_SC_CONFLICT;
    }
    r->entries[r->count++] = *e;
    return MULIB_SC_OK;
}

int mulib_spro_freeze_v2(mulib_spro_registry_v2 *r)
{
    if (!r || r->busy || !r->count || r->count > MULIB_SPRO_MAX)
        return MULIB_SC_STATE;
    for (size_t i = 0; i < r->count; ++i)
        if (!mulib_spro_entry_valid_v2_(&r->entries[i]))
            return MULIB_SC_STATE;
    r->frozen = 1;
    return MULIB_SC_OK;
}

int mulib_spro_call_v2(mulib_spro_registry_v2 *r, const mulib_scr_provider *crypto,
                       const mulib_sp_rule *rules, size_t n, const char *subject,
                       const mulib_sp_object *obj, const mulib_spro_request *q,
                       const mulib_sl_scope_v2 *scope, mulib_sl_approval_v2 *approval,
                       const unsigned char token[32], mulib_sc_buf out, size_t *written)
{
    const mulib_spro_entry_v2 *e = NULL;
    const mulib_spro_operation_v2 *op = NULL;
    mulib_sc_view result;
    size_t w = 0;
    int rc, needs_confirmation;
    if (!r || r->frozen != 1 || r->busy || !r->count || r->count > MULIB_SPRO_MAX ||
        !obj || !mulib_spro_range_args_(q, out, written) || !mulib_sl_scope_valid_v2(scope) ||
        mulib_sc_overlap(approval, sizeof *approval, q->input.ptr, q->input.len) ||
        mulib_sc_overlap(approval, sizeof *approval, out.ptr, out.cap) ||
        mulib_sc_overlap(approval, sizeof *approval, written, sizeof *written) ||
        ((approval != NULL) != (token != NULL)))
        return MULIB_SC_INVALID;
    rc = mulib_sp_evaluate(rules, n, subject, obj, q->op, q->now, &needs_confirmation);
    if (rc)
        return rc;
    if (needs_confirmation && !approval)
        return MULIB_SC_DENIED;

    for (size_t i = 0; i < r->count; ++i) {
        if (r->entries[i].kind == obj->kind) {
            e = &r->entries[i];
            break;
        }
    }
    if (!e)
        return MULIB_SC_NOTFOUND;
    if (!mulib_spro_entry_valid_v2_(e))
        return MULIB_SC_STATE;
    for (size_t i = 0; i < e->operation_count; ++i) {
        if (e->operations[i].operation == q->op) {
            op = &e->operations[i];
            break;
        }
    }
    if (!op)
        return MULIB_SC_UNSUPPORTED;
    if (strcmp(e->name, scope->provider) || op->request_schema != scope->request_schema ||
        op->result_schema != scope->result_schema)
        return MULIB_SC_DENIED;
    if (q->input.len > op->max_input || out.cap < op->max_output)
        return MULIB_SC_LIMIT;
    out.cap = op->max_output;

    r->busy = 1;
    if (approval) {
        rc = mulib_sl_consume_v2(crypto, approval, token, scope, subject, obj,
                                 q->op, q->input, q->now);
        if (rc)
            goto done;
    }
    rc = mulib_spro_callback_status_(op->validate_request(e->ctx, q->input));
    if (rc)
        goto done;
    rc = op->run(e->ctx, obj, q, out, &w);
    if (rc > 0)
        rc = MULIB_SC_UNCERTAIN;
    if (!rc) {
        if (w > out.cap) {
            rc = MULIB_SC_UNCERTAIN;
        } else {
            result.ptr = out.ptr;
            result.len = w;
            if (op->validate_result(e->ctx, result))
                rc = MULIB_SC_UNCERTAIN;
        }
    }
    if (rc) {
        mulib_sc_wipe(out.ptr, out.cap);
    } else {
        if (out.cap > w)
            mulib_sc_wipe(out.ptr + w, out.cap - w);
        *written = w;
    }
done:
    r->busy = 0;
    return rc;
}
#ifdef __cplusplus
}
#endif
#endif
