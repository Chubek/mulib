#include <stdio.h>
#include <string.h>
#include "mulib-secret-audit.h"
#include "mulib-secret-crypto-native.h"
#include "mulib-secret-lease.h"
#include "mulib-secret-policy.h"
#include "mulib-secret-provider.h"
#include "mulib-secret-rate.h"
#include "mulib-secret-spec.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static int is_byte(const void *p, size_t len, unsigned char byte)
{
    const unsigned char *b = (const unsigned char *)p;
    for (size_t i = 0; i < len; ++i)
        if (b[i] != byte)
            return 0;
    return 1;
}

static const mulib_sp_object object =
    {{42}, 0, MULIB_SP_GENERATE | MULIB_SP_SIGN | MULIB_SP_READ | MULIB_SP_EXPORT, 4, 0, 7};
static const mulib_sp_rule grant = {"alice", {42}, MULIB_SP_GENERATE, 0, MULIB_SP_GENERATE, 0};
static const mulib_sl_scope_v2 scope = {{1}, {2}, {3}, 5, 11, 12, "otp.test"};
static const unsigned char bytes[] = {0x71, 0};
static const unsigned char other[] = {0x71, 1};
static const mulib_sc_view request_bytes = {bytes, sizeof bytes};

static int policy_cases(void)
{
    mulib_sp_rule rules[2] = {grant, grant};
    mulib_sp_object obj = object;
    int confirm = -1;

    CHECK(mulib_sp_evaluate(NULL, 0, "alice", &obj, MULIB_SP_GENERATE, 1, &confirm)
          == MULIB_SC_DENIED && confirm == -1);
    CHECK(mulib_sp_evaluate(rules, 1, "alice", &obj, MULIB_SP_GENERATE, 1, &confirm)
          == MULIB_SC_OK && confirm == 1);
    CHECK(mulib_sp_check(rules, 1, "alice", &obj, MULIB_SP_GENERATE, 1, 2) == MULIB_SC_INVALID);
    CHECK(mulib_sp_check(rules, 1, "alice", &obj, MULIB_SP_GENERATE, 1, -1) == MULIB_SC_INVALID);
    CHECK(mulib_sp_check(rules, 1, "Alice", &obj, MULIB_SP_GENERATE, 1, 1) == MULIB_SC_DENIED);
    obj.id[15] = 1;
    CHECK(mulib_sp_check(rules, 1, "alice", &obj, MULIB_SP_GENERATE, 1, 1) == MULIB_SC_DENIED);
    obj = object;
    CHECK(mulib_sp_check(rules, 1, "alice", &obj, MULIB_SP_GENERATE | MULIB_SP_SIGN, 1, 1)
          == MULIB_SC_INVALID);
    rules[1].deny = MULIB_SP_GENERATE;
    rules[1].expires = 10;
    CHECK(mulib_sp_check(rules, 2, "alice", &obj, MULIB_SP_GENERATE, 9, 1) == MULIB_SC_DENIED);
    CHECK(mulib_sp_check(rules, 2, "alice", &obj, MULIB_SP_GENERATE, 10, 1) == MULIB_SC_OK);
    rules[1].deny = MULIB_SP_ALL + 1u;
    CHECK(mulib_sp_check(rules, 2, "alice", &obj, MULIB_SP_GENERATE, 10, 1) == MULIB_SC_INVALID);
    CHECK(mulib_sp_check(rules, MULIB_SP_MAX_RULES + 1u, "alice", &obj, MULIB_SP_GENERATE, 1, 1)
          == MULIB_SC_INVALID);

    rules[0].allow |= MULIB_SP_READ | MULIB_SP_EXPORT;
    rules[0].confirm = 0;
    obj.flags = MULIB_SP_RECONFIRM | MULIB_SP_NONEXPORT;
    CHECK(mulib_sp_check(rules, 1, "alice", &obj, MULIB_SP_READ, 1, 1) == MULIB_SC_DENIED);
    CHECK(mulib_sp_check(rules, 1, "alice", &obj, MULIB_SP_EXPORT, 1, 1) == MULIB_SC_DENIED);
    CHECK(mulib_sp_check(rules, 1, "alice", &obj, MULIB_SP_GENERATE, 1, 0) == MULIB_SC_DENIED);
    CHECK(mulib_sp_check(rules, 1, "alice", &obj, MULIB_SP_GENERATE, 1, 1) == MULIB_SC_OK);
    obj.flags = MULIB_SP_REVOKED;
    CHECK(mulib_sp_check(rules, 1, "alice", &obj, MULIB_SP_GENERATE, 1, 1) == MULIB_SC_DENIED);
    obj.flags = 32;
    CHECK(mulib_sp_check(rules, 1, "alice", &obj, MULIB_SP_GENERATE, 1, 1) == MULIB_SC_DENIED);
    obj.flags = 0;
    obj.expires = 1;
    CHECK(mulib_sp_check(rules, 1, "alice", &obj, MULIB_SP_GENERATE, 1, 1) == MULIB_SC_DENIED);
    obj.expires = 0;
    obj.capabilities |= MULIB_SP_ALL + 1u;
    CHECK(mulib_sp_check(rules, 1, "alice", &obj, MULIB_SP_GENERATE, 1, 1) == MULIB_SC_DENIED);
    return 0;
}

static int fail_random(void *ctx, unsigned char *out, size_t n)
{
    (void)ctx;
    memset(out, 0xff, n);
    return MULIB_SC_IO;
}

static int fail_hash(void *ctx, unsigned alg, mulib_sc_view in, unsigned char *out, size_t n)
{
    const mulib_sl_approval_v2 *approval = (const mulib_sl_approval_v2 *)ctx;
    (void)alg;
    (void)in;
    memset(out, 0xff, n);
    return approval->active ? MULIB_SC_STATE : MULIB_SC_IO;
}

static int lease_cases(const mulib_scr_provider *p)
{
    mulib_sl_approval_v2 approval;
    const mulib_sc_view oversized = {bytes, MULIB_SL_MAX_CANONICAL + 1u};
    const mulib_sc_view malformed = {NULL, 1};
    mulib_scr_provider failed = *p;
    unsigned char wrong_token[32];
    memset(&approval, 0, sizeof approval);
    CHECK(mulib_sl_issue_v2(p, &approval, &scope, "alice", &object, MULIB_SP_GENERATE,
                            request_bytes, UINT64_MAX, 1) == MULIB_SC_INVALID);
    CHECK(mulib_sl_issue_v2(p, &approval, &scope, "alice", &object, MULIB_SP_GENERATE,
                            oversized, 10, 5) == MULIB_SC_INVALID);
    CHECK(mulib_sl_issue_v2(p, &approval, &scope, "alice", &object, MULIB_SP_GENERATE,
                            malformed, 10, 5) == MULIB_SC_INVALID);
    failed.random = fail_random;
    CHECK(mulib_sl_issue_v2(&failed, &approval, &scope, "alice", &object, MULIB_SP_GENERATE,
                            request_bytes, 10, 5) == MULIB_SC_IO);
    CHECK(is_byte(&approval, sizeof approval, 0));

    CHECK(mulib_sl_issue_v2(p, &approval, &scope, "alice", &object, MULIB_SP_GENERATE,
                            request_bytes, 10, 5) == MULIB_SC_OK);
    memcpy(wrong_token, approval.token, sizeof wrong_token);
    wrong_token[0] ^= 1;
    CHECK(mulib_sl_consume_v2(p, &approval, wrong_token, &scope, "alice", &object,
                              MULIB_SP_GENERATE, request_bytes, 14) == MULIB_SC_DENIED);
    CHECK(approval.active == 1);
    CHECK(mulib_sl_consume_v2(p, &approval, approval.token, &scope, "alice", &object,
                              MULIB_SP_GENERATE, request_bytes, 14) == MULIB_SC_OK);
    CHECK(is_byte(&approval, sizeof approval, 0));
    CHECK(mulib_sl_consume_v2(p, &approval, wrong_token, &scope, "alice", &object,
                              MULIB_SP_GENERATE, request_bytes, 14) == MULIB_SC_DENIED);

    CHECK(mulib_sl_issue_v2(p, &approval, &scope, "alice", &object, MULIB_SP_GENERATE,
                            request_bytes, 10, 5) == MULIB_SC_OK);
    CHECK(mulib_sl_consume_v2(p, &approval, approval.token, &approval.scope, "alice", &object,
                              MULIB_SP_GENERATE, request_bytes, 11) == MULIB_SC_DENIED);
    CHECK(is_byte(&approval, sizeof approval, 0));

    for (unsigned i = 0; i < 15; ++i) {
        mulib_sl_scope_v2 changed = scope;
        mulib_sp_object obj = object;
        unsigned op = MULIB_SP_GENERATE;
        uint64_t time = 11;
        const char *subject = "alice";
        mulib_sc_view canonical = request_bytes;

        switch (i) {
        case 0: changed.vault[15] = 1; break;
        case 1: changed.session[15] = 1; break;
        case 2: changed.destination[15] = 1; break;
        case 3: ++changed.policy_revision; break;
        case 4: ++changed.request_schema; break;
        case 5: ++changed.result_schema; break;
        case 6: changed.provider[0] = 'x'; break;
        case 7: obj.id[15] = 1; break;
        case 8: ++obj.revision; break;
        case 9: ++obj.kind; break;
        case 10: op = MULIB_SP_SIGN; break;
        case 11: time = 9; break;
        case 12: time = 15; break;
        case 13: subject = "bob"; break;
        case 14: canonical.ptr = other; break;
        }
        CHECK(mulib_sl_issue_v2(p, &approval, &scope, "alice", &object, MULIB_SP_GENERATE,
                                request_bytes, 10, 5) == MULIB_SC_OK);
        CHECK(mulib_sl_consume_v2(p, &approval, approval.token, &changed, subject,
                                  &obj, op, canonical, time) == MULIB_SC_DENIED);
        CHECK(is_byte(&approval, sizeof approval, 0));
    }
    failed = *p;
    failed.ctx = &approval;
    failed.hash = fail_hash;
    CHECK(mulib_sl_issue_v2(p, &approval, &scope, "alice", &object, MULIB_SP_GENERATE,
                            request_bytes, 10, 5) == MULIB_SC_OK);
    CHECK(mulib_sl_consume_v2(&failed, &approval, approval.token, &scope, "alice", &object,
                              MULIB_SP_GENERATE, request_bytes, 11) == MULIB_SC_IO);
    CHECK(is_byte(&approval, sizeof approval, 0));
    return 0;
}

typedef struct {
    int requests, results, effects, mode;
    const mulib_sl_approval_v2 *approval;
    mulib_spro_registry_v2 *registry;
} dispatch_test;

static int validate_request(void *v, mulib_sc_view in)
{
    dispatch_test *t = (dispatch_test *)v;
    ++t->requests;
    return in.len == 2 && in.ptr[0] == 0x71 && !in.ptr[1] ? MULIB_SC_OK : MULIB_SC_INVALID;
}

static int validate_result(void *v, mulib_sc_view in)
{
    dispatch_test *t = (dispatch_test *)v;
    ++t->results;
    return in.len == 1 && *in.ptr == 0x72 ? MULIB_SC_OK : MULIB_SC_INVALID;
}

static int run_operation(void *v, const mulib_sp_object *obj, const mulib_spro_request *q,
                         mulib_sc_buf out, size_t *written)
{
    dispatch_test *t = (dispatch_test *)v;
    if (obj->kind != 4 || q->op != MULIB_SP_GENERATE || out.cap != 4 ||
        t->approval->active || !t->registry->busy)
        return MULIB_SC_STATE;
    if (mulib_spro_call_v2(t->registry, NULL, &grant, 1, "alice", obj, q,
                           &scope, NULL, NULL, out, written) != MULIB_SC_INVALID)
        return MULIB_SC_STATE;
    ++t->effects;
    memset(out.ptr, 0xff, out.cap);
    *written = t->mode == 1 ? out.cap + 1u : 1u;
    if (t->mode != 2)
        out.ptr[0] = 0x72;
    if (t->mode == 3)
        return MULIB_SC_UNCERTAIN;
    return t->mode == 4 ? 1 : MULIB_SC_OK;
}

static int provider_cases(const mulib_scr_provider *p)
{
    mulib_spro_registry_v2 registry;
    mulib_sl_approval_v2 approval;
    dispatch_test stats = {0, 0, 0, 0, &approval, &registry};
    const mulib_spro_operation_v2 generate =
        {MULIB_SP_GENERATE, 11, 12, 2, 4, validate_request, validate_result, run_operation};
    const mulib_spro_operation_v2 duplicate[2] = {generate, generate};
    mulib_spro_entry_v2 entry = {MULIB_SPRO_ABI_V2, sizeof entry, 4, "otp.test", &stats, &generate, 1};
    const mulib_spro_request request = {MULIB_SP_GENERATE, 11, request_bytes};
    const mulib_spro_request malformed = {MULIB_SP_GENERATE, 11, {other, sizeof other}};
    const mulib_spro_request unsupported = {MULIB_SP_SIGN, 11, request_bytes};
    unsigned char buffer[8], token[32];
    const mulib_spro_request alias = {MULIB_SP_GENERATE, 11, {buffer, sizeof bytes}};
    const mulib_sc_buf out = {buffer, sizeof buffer}, short_out = {buffer, 3};
    size_t written = 99;
    mulib_sp_rule sign_grant = grant;
    mulib_sl_scope_v2 bad_scope = scope;
    memset(&registry, 0, sizeof registry);
    memset(&approval, 0, sizeof approval);

    CHECK(mulib_spro_freeze_v2(&registry) == MULIB_SC_STATE);
    --entry.size;
    CHECK(mulib_spro_register_v2(&registry, &entry) == MULIB_SC_INVALID);
    ++entry.size;
    entry.operation_count = 2;
    entry.operations = duplicate;
    CHECK(mulib_spro_register_v2(&registry, &entry) == MULIB_SC_INVALID);
    entry.operation_count = 1;
    entry.operations = &generate;
    CHECK(mulib_spro_register_v2(&registry, &entry) == MULIB_SC_OK);
    CHECK(mulib_spro_register_v2(&registry, &entry) == MULIB_SC_CONFLICT);
    CHECK(mulib_spro_freeze_v2(&registry) == MULIB_SC_OK);
    CHECK(mulib_spro_register_v2(&registry, &entry) == MULIB_SC_INVALID);

    CHECK(mulib_spro_call_v2(&registry, p, &grant, 1, "alice", &object, &request,
                             &scope, NULL, NULL, out, &written) == MULIB_SC_DENIED);
    sign_grant.allow = MULIB_SP_SIGN;
    CHECK(mulib_spro_call_v2(&registry, p, &sign_grant, 1, "alice", &object, &unsupported,
                             &scope, NULL, NULL, out, &written) == MULIB_SC_UNSUPPORTED);
    CHECK(mulib_sl_issue_v2(p, &approval, &scope, "alice", &object, MULIB_SP_GENERATE,
                            request_bytes, 10, 5) == MULIB_SC_OK);
    memcpy(token, approval.token, sizeof token);
    ++bad_scope.result_schema;
    CHECK(mulib_spro_call_v2(&registry, p, &grant, 1, "alice", &object, &request,
                             &bad_scope, &approval, token, out, &written) == MULIB_SC_DENIED);
    CHECK(mulib_spro_call_v2(&registry, p, &grant, 1, "alice", &object, &request,
                             &scope, &approval, token, short_out, &written) == MULIB_SC_LIMIT);
    CHECK(mulib_spro_call_v2(&registry, p, &grant, 1, "alice", &object, &alias,
                             &scope, &approval, token, out, &written) == MULIB_SC_INVALID);
    CHECK(written == 99 && !stats.requests && !stats.effects && approval.active);
    memset(buffer, 0xa5, sizeof buffer);
    CHECK(mulib_spro_call_v2(&registry, p, &grant, 1, "alice", &object, &request,
                             &scope, &approval, token, out, &written) == MULIB_SC_OK);
    CHECK(written == 1 && buffer[0] == 0x72 && is_byte(buffer + 1, 3, 0) &&
          is_byte(buffer + 4, 4, 0xa5) && stats.requests == 1 && stats.results == 1 &&
          stats.effects == 1 && !approval.active && !registry.busy);
    CHECK(mulib_spro_call_v2(&registry, p, &grant, 1, "alice", &object, &request,
                             &scope, &approval, token, out, &written) == MULIB_SC_DENIED);

    CHECK(mulib_sl_issue_v2(p, &approval, &scope, "alice", &object, MULIB_SP_GENERATE,
                            request_bytes, 10, 5) == MULIB_SC_OK);
    CHECK(mulib_spro_call_v2(&registry, p, &grant, 1, "alice", &object, &malformed,
                             &scope, &approval, approval.token, out, &written) == MULIB_SC_DENIED);
    CHECK(!approval.active && stats.requests == 1);
    CHECK(mulib_sl_issue_v2(p, &approval, &scope, "alice", &object, MULIB_SP_GENERATE,
                            malformed.input, 10, 5) == MULIB_SC_OK);
    CHECK(mulib_spro_call_v2(&registry, p, &grant, 1, "alice", &object, &malformed,
                             &scope, &approval, approval.token, out, &written) == MULIB_SC_INVALID);
    CHECK(!approval.active && stats.requests == 2 && stats.effects == 1);

    for (unsigned i = 0; i < 4; ++i) {
        stats.mode = (int)i + 1;
        written = 77;
        memset(buffer, 0xa5, sizeof buffer);
        CHECK(mulib_sl_issue_v2(p, &approval, &scope, "alice", &object, MULIB_SP_GENERATE,
                                request_bytes, 10, 5) == MULIB_SC_OK);
        CHECK(mulib_spro_call_v2(&registry, p, &grant, 1, "alice", &object, &request,
                                 &scope, &approval, approval.token, out, &written)
              == MULIB_SC_UNCERTAIN);
        CHECK(!approval.active && !registry.busy && written == 77 &&
              is_byte(buffer, 4, 0) && is_byte(buffer + 4, 4, 0xa5));
    }
    CHECK(stats.effects == 5 && stats.results == 2);
    return 0;
}

static int fail_hmac(void *ctx, unsigned alg, mulib_sc_view key, mulib_sc_view in,
                     unsigned char *out, size_t n)
{
    (void)ctx;
    (void)alg;
    (void)key;
    (void)in;
    memset(out, 0xff, n);
    return MULIB_SC_IO;
}

static int audit_cases(const mulib_scr_provider *p)
{
    /* HMAC-SHA256 using Python hmac on the fixed 88-byte body. */
    const unsigned char known[32] = {
        0x2e,0xae,0x01,0x08,0x5d,0x4d,0xb4,0x26,0xcb,0x80,0x05,0x42,0x9c,0x10,0x25,0x08,
        0x0c,0xc8,0x37,0x8e,0x4b,0x31,0xf9,0x87,0xcd,0xa2,0xbf,0x92,0x49,0x95,0xc6,0x1b
    };
    unsigned char key_bytes[32], wire[MULIB_SAUD_SIZE], second[MULIB_SAUD_SIZE];
    const mulib_sc_view key = {key_bytes, sizeof key_bytes};
    const mulib_saud_event event = {42, {0x42}, {0x99}, MULIB_SP_SIGN, MULIB_SC_DENIED};
    const mulib_saud_state genesis = {0, {0}};
    mulib_saud_state state = genesis, anchor, first;
    mulib_scr_provider failed = *p;
    const mulib_sc_view short_wire = {wire, sizeof wire - 1u};

    for (unsigned i = 0; i < sizeof key_bytes; ++i)
        key_bytes[i] = (unsigned char)i;
    failed.hmac = fail_hmac;
    memset(wire, 0x12, sizeof wire);
    CHECK(mulib_saud_append(&failed, key, &state, &event, wire, &state) == MULIB_SC_IO);
    CHECK(is_byte(wire, sizeof wire, 0x12) && !mulib_saud_check_anchor(&state, &genesis));
    CHECK(mulib_saud_append(p, key, &state, &event, wire, &first) == MULIB_SC_OK);
    CHECK(first.sequence == 1 && !memcmp(wire + 88, known, sizeof known));
    CHECK(mulib_saud_append(p, key, &first, &event, second, &anchor) == MULIB_SC_OK);
    CHECK(mulib_saud_verify_record(p, key, &state, short_wire) == MULIB_SC_INVALID);
    CHECK(mulib_saud_verify(p, key, &state, second) == MULIB_SC_AUTH);
    CHECK(mulib_saud_verify(&failed, key, &state, wire) == MULIB_SC_IO);
    CHECK(mulib_saud_check_anchor(&state, &genesis) == MULIB_SC_OK);
    CHECK(mulib_saud_verify(p, key, &state, wire) == MULIB_SC_OK);
    CHECK(mulib_saud_verify(p, key, &state, wire) == MULIB_SC_AUTH);
    CHECK(mulib_saud_check_anchor(&state, &anchor) == MULIB_SC_AUTH);
    second[119] ^= 1;
    CHECK(mulib_saud_verify(p, key, &state, second) == MULIB_SC_AUTH);
    CHECK(mulib_saud_check_anchor(&state, &first) == MULIB_SC_OK);
    second[119] ^= 1;
    CHECK(mulib_saud_verify(p, key, &state, second) == MULIB_SC_OK);
    CHECK(mulib_saud_check_anchor(&state, &anchor) == MULIB_SC_OK);
    anchor.head[31] ^= 1;
    CHECK(mulib_saud_check_anchor(&state, &anchor) == MULIB_SC_AUTH);
    state.sequence = 0;
    CHECK(mulib_saud_check_anchor(&state, &genesis) == MULIB_SC_INVALID);
    state.sequence = UINT64_MAX;
    CHECK(mulib_saud_append(p, key, &state, &event, wire, &anchor) == MULIB_SC_LIMIT);
    CHECK(mulib_saud_verify(p, key, &state, wire) == MULIB_SC_LIMIT);
    return 0;
}

static int rate_and_spec_cases(void)
{
    mulib_srate_state rate = {0, 0, 0};
    uint64_t wait = UINT64_MAX;

    CHECK(mulib_srate_check(&rate, 0, &wait) == MULIB_SC_OK && !wait);
    CHECK(mulib_srate_failure(&rate, 10, 2, 5) == MULIB_SC_OK && rate.next == 12);
    CHECK(mulib_srate_check(&rate, 11, &wait) == MULIB_SC_DENIED && wait == 1);
    CHECK(mulib_srate_check(&rate, 9, &wait) == MULIB_SC_STATE && wait == 1);
    CHECK(mulib_srate_success(&rate, 9) == MULIB_SC_STATE);
    CHECK(mulib_srate_success(&rate, 11) == MULIB_SC_DENIED);
    CHECK(mulib_srate_failure(&rate, 9, 2, 5) == MULIB_SC_STATE && rate.next == 12);
    CHECK(mulib_srate_failure(&rate, 12, 2, 5) == MULIB_SC_OK && rate.next == 16);
    CHECK(mulib_srate_failure(&rate, 16, 2, 5) == MULIB_SC_OK && rate.next == 21);
    CHECK(mulib_srate_failure(&rate, 17, 1, 1) == MULIB_SC_OK && rate.next == 21);
    CHECK(mulib_srate_check(&rate, 21, &rate.next) == MULIB_SC_INVALID && rate.next == 21);
    rate.failures = UINT32_MAX;
    CHECK(mulib_srate_failure(&rate, 21, 1, UINT32_MAX) == MULIB_SC_OK &&
          rate.failures == UINT32_MAX && rate.next == UINT64_C(21) + UINT32_MAX);
    CHECK(mulib_srate_success(&rate, rate.next) == MULIB_SC_OK && !rate.failures);
    CHECK(mulib_srate_failure(&rate, UINT64_MAX - 1, 2, 5) == MULIB_SC_OK &&
          rate.next == UINT64_MAX);
    CHECK(mulib_srate_check(&rate, UINT64_MAX, &wait) == MULIB_SC_DENIED && wait == 1);
    CHECK(mulib_srate_success(&rate, UINT64_MAX) == MULIB_SC_DENIED);
    rate.next = 0;
    CHECK(mulib_srate_check(&rate, UINT64_MAX, &wait) == MULIB_SC_STATE && wait == 1);

    CHECK(mulib_sspec_count() >= 40 && !mulib_sspec_at(mulib_sspec_count()));
    CHECK(!mulib_sspec_find("ToTp") && !mulib_sspec_find_kind(0) && !mulib_sspec_find_kind(UINT32_MAX));
    for (size_t i = 0; i < mulib_sspec_count(); ++i) {
        const mulib_sspec *spec = mulib_sspec_at(i);
        CHECK(spec && mulib_sspec_find(spec->name) == spec && mulib_sspec_find_kind(spec->kind) == spec);
        CHECK(spec->operations && !(spec->operations & ~MULIB_SP_ALL));
    }
    return 0;
}

int main(void)
{
    mulib_scr_provider p;
    mulib_scr_native(&p);
    if (policy_cases() || lease_cases(&p) || provider_cases(&p) ||
        audit_cases(&p) || rate_and_spec_cases())
        return 1;
    puts("Secret broker kernel negative tests passed");
    return 0;
}
