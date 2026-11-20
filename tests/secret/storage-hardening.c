/* P0 storage/ownership regressions. OpenSSL is a real optional test backend.
 * On Linux, define MULIB_TEST_STORE_FAULTS and add
 * -Wl,--wrap=fsync,--wrap=write,--wrap=read,--wrap=mlock for fault injection. */
#include "mulib-secret-catalog.h"
#include "mulib-secret-crypto-openssl.h"
#include "mulib-secret-memory.h"
#include "mulib-secret-state.h"
#include "mulib-secret-store.h"
#include "mulib-secret-vault.h"

#define MULIB_IMPL_SECRET_CORE
#define MULIB_IMPL_SECRET_CRYPTO
#define MULIB_IMPL_SECRET_CRYPTO_OPENSSL
#define MULIB_IMPL_SECRET_CATALOG
#define MULIB_IMPL_SECRET_MEMORY
#define MULIB_IMPL_SECRET_RECORD
#define MULIB_IMPL_SECRET_STATE
#define MULIB_IMPL_SECRET_STORE
#define MULIB_IMPL_SECRET_VAULT
#include "mulib-secret-core.h"
#include "mulib-secret-crypto.h"
#include "mulib-secret-crypto-openssl.h"
#include "mulib-secret-catalog.h"
#include "mulib-secret-memory.h"
#include "mulib-secret-record.h"
#include "mulib-secret-state.h"
#include "mulib-secret-store.h"
#include "mulib-secret-vault.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)

#ifdef MULIB_TEST_STORE_FAULTS
#ifdef __cplusplus
extern "C" {
#endif
int __real_fsync(int);
int __real_mlock(const void *, size_t);
ssize_t __real_read(int, void *, size_t);
ssize_t __real_write(int, const void *, size_t);
static int fsync_fault, read_fault, write_fault, lock_fault;

int __wrap_fsync(int fd)
{
    if (fsync_fault == 3) {
        fsync_fault = 0;
        errno = EINTR;
        return -1;
    }
    if (fsync_fault > 0 && --fsync_fault == 0) {
        errno = EIO;
        return -1;
    }
    return __real_fsync(fd);
}

int __wrap_mlock(const void *p, size_t n)
{
    if (lock_fault) {
        errno = EPERM;
        return -1;
    }
    return __real_mlock(p, n);
}

ssize_t __wrap_read(int fd, void *p, size_t n)
{
    if (read_fault == 2) {
        --read_fault;
        return __real_read(fd, p, n ? 1 : 0);
    }
    if (read_fault == 1) {
        --read_fault;
        errno = EIO;
        return -1;
    }
    return __real_read(fd, p, n);
}

ssize_t __wrap_write(int fd, const void *p, size_t n)
{
    if (write_fault == 2) {
        --write_fault;
        return __real_write(fd, p, n ? 1 : 0);
    }
    if (write_fault == 1) {
        --write_fault;
        errno = ENOSPC;
        return -1;
    }
    return __real_write(fd, p, n);
}
#ifdef __cplusplus
}
#endif
#endif

static int filled(const unsigned char *p, size_t n, unsigned char value)
{
    for (size_t i = 0; i < n; ++i)
        if (p[i] != value)
            return 0;
    return 1;
}

#if defined(__linux__)
static void check_map_permissions(const void *p, const char *expected)
{
    FILE *maps = fopen("/proc/self/maps", "r");
    CHECK(maps);
    char line[1024], perms[5];
    unsigned long long start, end, target = (unsigned long long)(uintptr_t)p;
    int found = 0;
    while (fgets(line, sizeof line, maps)) {
        if (sscanf(line, "%llx-%llx %4s", &start, &end, perms) == 3 &&
            start <= target && target < end) {
            found = strcmp(perms, expected) == 0;
            break;
        }
    }
    CHECK(fclose(maps) == 0 && found);
}
#endif

static void test_memory(void)
{
    mulib_sm_buffer b;
    memset(&b, 0, sizeof b);
    CHECK(mulib_sm_alloc(&b, 0, 1) == MULIB_SC_INVALID && !b.ptr);
    CHECK(mulib_sm_alloc(&b, SIZE_MAX, 0) == MULIB_SC_LIMIT && !b.ptr);
#ifdef MULIB_TEST_STORE_FAULTS
    lock_fault = 1;
    CHECK(mulib_sm_alloc(&b, 4, 1) == MULIB_SC_DENIED);
    CHECK(!b.ptr && !b.cap && !b.mapping && !b.hardening);
    CHECK(mulib_sm_alloc(&b, 4, 0) == MULIB_SC_OK && !b.locked);
    mulib_sm_clear(&b);
    lock_fault = 0;
#endif
    CHECK(mulib_sm_alloc(&b, 4, 0) == MULIB_SC_OK && b.cap >= 4 && b.len == 0);
    CHECK(b.hardening & MULIB_SM_GUARDED);
    CHECK(filled(b.ptr, b.cap, 0));
#if defined(__linux__)
    check_map_permissions(b.mapping, "---p");
    check_map_permissions(b.ptr, "rw-p");
    check_map_permissions(b.ptr + b.cap, "---p");
#endif
    memset(b.ptr, 0x73, b.cap);
    mulib_sc_view own_slice = {b.ptr + 2, 2};
    CHECK(mulib_sm_assign(&b, own_slice) == MULIB_SC_OK && b.len == 2);
    CHECK(filled(b.ptr, 2, 0x73) && filled(b.ptr + 2, b.cap - 2, 0));
    if (b.hardening & MULIB_SM_WIPEONFORK) {
        pid_t pid = fork();
        CHECK(pid >= 0);
        if (pid == 0)
            _exit(filled(b.ptr, b.cap, 0) ? 0 : 1);
        int status;
        CHECK(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
        CHECK(filled(b.ptr, 2, 0x73));
    }
    mulib_sm_clear(&b);
    CHECK(!b.ptr && !b.len && !b.cap && !b.mapping && !b.hardening);
    mulib_sm_clear(&b);
}

static int kdf_calls;
static int kdf_fail(void *ctx, mulib_scr_kdf_params q, mulib_sc_view password,
                    mulib_sc_view salt, unsigned char out[32])
{
    (void)ctx; (void)q; (void)password; (void)salt;
    ++kdf_calls;
    memset(out, 0x73, 32);
    return MULIB_SC_IO;
}

static void fixed_hex(const char *hex, unsigned char *out, size_t len)
{
    CHECK(strlen(hex) == 2 * len);
    for (size_t i = 0; i < len; ++i) {
        unsigned value;
        CHECK(sscanf(hex + 2 * i, "%2x", &value) == 1);
        out[i] = (unsigned char)value;
    }
}

static void test_wire_compatibility(const mulib_scr_provider *p)
{
    /* Fixed MSV1/MSR1 vectors assembled with Python hashlib PBKDF2 and
     * cryptography HKDF/AESGCM. Shared salt/nonce here are test fixtures only. */
    static const char vault[] =
        "4d53563101010100000927c000000000202122232425262728292a2b2c2d2e2f"
        "303132333435363738393a3b3c3d3e3f404142434445464748494a4b00000000"
        "00000000000000000000000000000000beaf0f0860f428eb2af324796fd8dfa3"
        "12d86f93cd81c17616815f78ecd6d593f68bb3d6a5dbef259859c80d9742f631";
    static const char record[] =
        "4d53523101010000101112131415161718191a1b1c1d1e1f0000000000000002"
        "0000000300000004202122232425262728292a2b2c2d2e2f3031323334353637"
        "38393a3b3c3d3e3f404142434445464748494a4b000000000000000000000000"
        "5eab08316b4f744d10bac13b9f629e6fce0c4dfe";
    static const unsigned char pass[] = "synthetic password", payload[] = {1, 2, 3, 4};
    unsigned char header[MULIB_SV_HEADER], wire[MULIB_SR_OVERHEAD + sizeof payload], out[4];
    fixed_hex(vault, header, sizeof header);
    fixed_hex(record, wire, sizeof wire);
    mulib_sv_profile profile = {MULIB_SCR_AES128_GCM, {MULIB_SCR_PBKDF2_SHA256, 600000, 0, 0}};
    mulib_sv_session v;
    memset(&v, 0, sizeof v);
    mulib_sc_view password = {pass, sizeof pass - 1}, sealed = {wire, sizeof wire};
    CHECK(mulib_sv_unlock_pinned(p, profile, password, header, &v, 0) == MULIB_SC_OK);
    for (size_t i = 0; i < 32; ++i)
        CHECK(v.root.ptr[i] == (unsigned char)i);
    mulib_sr_meta m;
    for (size_t i = 0; i < 16; ++i)
        m.id[i] = (unsigned char)(16 + i);
    m.revision = 2;
    m.kind = 3;
    mulib_sc_buf plaintext = {out, sizeof out};
    size_t n = 0;
    CHECK(mulib_sr_open_pinned(p, profile.algorithm, v.root.ptr, &m, sealed, plaintext, &n) ==
          MULIB_SC_OK);
    CHECK(n == sizeof payload && memcmp(out, payload, n) == 0);
    mulib_sv_lock(&v);
}

static void test_vault_record(const mulib_scr_provider *p, unsigned alg)
{
    static const unsigned char pass[] = "synthetic password";
    static const unsigned char next[] = "synthetic password rewrap";
    static const unsigned char payload[] = {1, 2, 3, 4};
    mulib_sc_view pw = {pass, sizeof pass - 1}, np = {next, sizeof next - 1};
    mulib_scr_kdf_params q = {MULIB_SCR_PBKDF2_SHA256, 600000, 0, 0};
    mulib_sv_profile profile = {alg, q};
    mulib_sv_session v, u;
    memset(&v, 0, sizeof v);
    memset(&u, 0, sizeof u);
    unsigned char hdr[MULIB_SV_HEADER];
    memset(hdr, 0xc5, sizeof hdr);
    mulib_scr_provider failing = *p;
    failing.kdf = kdf_fail;
    kdf_calls = 0;
#ifdef MULIB_TEST_STORE_FAULTS
    lock_fault = 1;
    CHECK(mulib_sv_create(&failing, alg, q, pw, hdr, &v, 1) == MULIB_SC_DENIED);
    CHECK(kdf_calls == 0 && !v.root.ptr && filled(hdr, sizeof hdr, 0xc5));
    lock_fault = 0;
#endif
    CHECK(mulib_sv_create(&failing, alg, q, pw, hdr, &v, 0) == MULIB_SC_IO);
    CHECK(kdf_calls == 1 && !v.root.ptr && !v.algorithm && filled(hdr, sizeof hdr, 0xc5));
    mulib_sc_view alias_pw = {hdr + 1, 8};
    CHECK(mulib_sv_create(p, alg, q, alias_pw, hdr, &v, 0) == MULIB_SC_INVALID);
    CHECK(mulib_sv_create(p, alg, q, pw, hdr, &v, 0) == MULIB_SC_OK);
    CHECK(memcmp(hdr, "MSV1", 4) == 0 && hdr[5] == alg && v.root.len == 32);
    mulib_sv_profile different = profile;
    ++different.kdf.iterations;
    CHECK(mulib_sv_unlock_pinned(&failing, different, pw, hdr, &u, 0) == MULIB_SC_DENIED);
    different = profile;
    different.algorithm = alg == MULIB_SCR_AES128_GCM ? MULIB_SCR_AES256_GCM : MULIB_SCR_AES128_GCM;
    CHECK(mulib_sv_unlock_pinned(&failing, different, pw, hdr, &u, 0) == MULIB_SC_DENIED);
    hdr[79] = 1;
    CHECK(mulib_sv_unlock_pinned(&failing, profile, pw, hdr, &u, 0) == MULIB_SC_INVALID);
    CHECK(kdf_calls == 1 && !u.root.ptr);
    hdr[79] = 0;
    CHECK(mulib_sv_unlock_pinned(p, profile, np, hdr, &u, 0) == MULIB_SC_AUTH);
    CHECK(!u.root.ptr && !u.algorithm);
    CHECK(mulib_sv_unlock_pinned(p, profile, pw, hdr, &u, 0) == MULIB_SC_OK);
    CHECK(mulib_sc_equal(v.root.ptr, u.root.ptr, 32));
    mulib_sv_lock(&u);
    CHECK(mulib_sv_rewrap(p, &v, q, np, v.root.ptr) == MULIB_SC_INVALID);
    CHECK(mulib_sv_rewrap(p, &v, q, np, hdr) == MULIB_SC_OK);
    CHECK(mulib_sv_unlock_pinned(p, profile, np, hdr, &u, 0) == MULIB_SC_OK);
    CHECK(mulib_sc_equal(v.root.ptr, u.root.ptr, 32));
    mulib_sv_lock(&u);

    unsigned char wire[MULIB_SR_OVERHEAD + 32], plain[32];
    mulib_sr_meta m;
    memset(&m, 0, sizeof m);
    m.id[0] = 1;
    m.revision = 2;
    m.kind = 3;
    mulib_sc_buf enc = {wire, sizeof wire}, out = {plain, sizeof plain};
    mulib_sc_view src = {payload, sizeof payload}, aliased_src = {wire + 1, sizeof payload};
    size_t n = 42, len = 42;
    memset(wire, 0xc5, sizeof wire);
    CHECK(mulib_sr_seal(p, alg, v.root.ptr, &m, aliased_src, enc, &n) == MULIB_SC_INVALID);
    CHECK(n == 42 && filled(wire, sizeof wire, 0xc5));
    CHECK(mulib_sr_seal(p, alg, v.root.ptr, &m, src, enc, &n) == MULIB_SC_OK);
    CHECK(n == MULIB_SR_OVERHEAD + sizeof payload && wire[n] == 0xc5);
    mulib_sc_view sealed = {wire, n};
    memset(plain, 0xc5, sizeof plain);
    CHECK(mulib_sr_open_pinned(p, different.algorithm, v.root.ptr, &m, sealed, out, &len) ==
          MULIB_SC_DENIED);
    CHECK(mulib_sr_open_pinned(p, alg, v.root.ptr, &m, sealed, enc, &len) == MULIB_SC_INVALID);
    CHECK(len == 42 && filled(plain, sizeof plain, 0xc5));
    wire[n - 1] ^= 1;
    CHECK(mulib_sr_open_pinned(p, alg, v.root.ptr, &m, sealed, out, &len) == MULIB_SC_AUTH);
    CHECK(len == 42 && filled(plain, sizeof payload, 0) &&
          filled(plain + sizeof payload, sizeof plain - sizeof payload, 0xc5));
    wire[n - 1] ^= 1;
    CHECK(mulib_sr_open_pinned(p, alg, v.root.ptr, &m, sealed, out, &len) == MULIB_SC_OK);
    CHECK(len == sizeof payload && memcmp(plain, payload, len) == 0);
    mulib_sc_view empty = {NULL, 0};
    mulib_sc_buf empty_out = {NULL, 0};
    CHECK(mulib_sr_seal(p, alg, v.root.ptr, &m, empty, enc, &n) == MULIB_SC_OK);
    sealed.len = n;
    CHECK(mulib_sr_open_pinned(p, alg, v.root.ptr, &m, sealed, empty_out, &len) == MULIB_SC_OK);
    CHECK(n == MULIB_SR_OVERHEAD && len == 0);
    mulib_sv_lock(&v);
}

static void test_catalog(void)
{
    mulib_scat_entry e[2], out[2];
    memset(e, 0, sizeof e);
    for (size_t i = 0; i < 2; ++i) {
        e[i].kind = e[i].policy.kind = 1;
        e[i].policy.id[0] = (unsigned char)i;
        e[i].policy.revision = 1;
        e[i].policy.flags = MULIB_SP_NONEXPORT;
    }
    unsigned char wire[8 + 2 * MULIB_SCAT_ENTRY_SIZE];
    mulib_sc_buf dst = {wire, sizeof wire};
    size_t n = 42, count = 42, idx = 42;
    CHECK(mulib_scat_encode(e, 2, dst, &n) == MULIB_SC_OK && n == sizeof wire);
    mulib_sc_view src = {wire, n};
    memset(out, 0xc5, sizeof out);
    wire[n - 1] = 1;
    CHECK(mulib_scat_decode(src, out, 2, &count) == MULIB_SC_INVALID);
    CHECK(count == 42 && filled((const unsigned char *)out, sizeof out, 0xc5));
    wire[n - 1] = 0;
    CHECK(mulib_scat_decode(src, out, 2, &count) == MULIB_SC_OK && count == 2);
    mulib_sc_buf alias = {(unsigned char *)out, sizeof out};
    CHECK(mulib_scat_encode(out, 1, alias, &n) == MULIB_SC_INVALID && n == sizeof wire);
    CHECK(mulib_scat_find(out, count, e[1].policy.id, &idx) == MULIB_SC_OK && idx == 1);
    CHECK(mulib_scat_update(out + 1, 1, 0, 0, 0) == MULIB_SC_DENIED);
    CHECK(mulib_scat_update(out + 1, 1, MULIB_SP_NONEXPORT, 0, 0) == MULIB_SC_OK);
    CHECK(out[1].policy.revision == 2);
}

typedef struct {
    mulib_ss_counter durable;
    int calls, result;
} test_cas_ctx;

static int test_cas(void *ctx, const mulib_ss_counter *old, const mulib_ss_counter *next)
{
    test_cas_ctx *c = (test_cas_ctx *)ctx;
    ++c->calls;
    if (c->durable.revision != old->revision || c->durable.counter != old->counter ||
        memcmp(c->durable.id, old->id, sizeof old->id))
        return MULIB_SC_CONFLICT;
    if (c->result == MULIB_SC_OK || c->result == MULIB_SC_UNCERTAIN)
        c->durable = *next;
    return c->result;
}

static void test_state(void)
{
    test_cas_ctx c;
    memset(&c, 0, sizeof c);
    c.durable.revision = 1;
    c.durable.counter = 8;
    c.durable.id[0] = 91;
    mulib_ss_counter s = c.durable, stale = c.durable;
    uint64_t value = 42;
    CHECK(mulib_ss_reserve(&s, test_cas, &c, &s.counter) == MULIB_SC_INVALID && c.calls == 0);
    c.result = MULIB_SC_IO;
    CHECK(mulib_ss_reserve(&s, test_cas, &c, &value) == MULIB_SC_IO);
    CHECK(s.revision == 1 && s.counter == 8 && value == 42 && c.calls == 1);
    c.result = MULIB_SC_UNCERTAIN;
    CHECK(mulib_ss_reserve(&s, test_cas, &c, &value) == MULIB_SC_UNCERTAIN);
    CHECK(s.revision == 0 && value == 42 && c.calls == 2);
    CHECK(mulib_ss_accept(&s, 99, test_cas, &c) == MULIB_SC_STATE);
    CHECK(mulib_ss_reserve(&s, test_cas, &c, &value) == MULIB_SC_STATE && c.calls == 2);
    c.result = MULIB_SC_OK;
    CHECK(mulib_ss_reserve(&stale, test_cas, &c, &value) == MULIB_SC_CONFLICT);
    CHECK(stale.revision == 0 && value == 42);
    s = c.durable;
    CHECK(mulib_ss_reserve(&s, test_cas, &c, &value) == MULIB_SC_OK);
    CHECK(s.counter == 10 && s.revision == 3 && value == 9);
    CHECK(mulib_ss_accept(&s, 9, test_cas, &c) == MULIB_SC_AUTH);
    c.result = MULIB_SC_UNCERTAIN;
    CHECK(mulib_ss_accept(&s, 10, test_cas, &c) == MULIB_SC_UNCERTAIN);
    CHECK(s.revision == 0 && c.durable.counter == 11);
}

static void test_store(const mulib_scr_provider *p)
{
    const char *base = getenv("TMPDIR");
    char dir[1024];
    int n = snprintf(dir, sizeof dir, "%s/mulib-storage-p0-XXXXXX", base ? base : "/tmp");
    CHECK(n > 0 && (size_t)n < sizeof dir && mkdtemp(dir));
    int d = open(dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    CHECK(d >= 0);
    unsigned char bytes[16], out[32];
    CHECK(mulib_scr_random(p, bytes, sizeof bytes) == MULIB_SC_OK);
    mulib_sc_view first = {bytes, 4}, second = {bytes + 4, 8};
    mulib_sc_buf dst = {out, sizeof out};
    size_t len = 42;
    mode_t old_umask = umask(0777);
    CHECK(mulib_sstore_write(p, d, "blob", first) == MULIB_SC_OK);
    (void)umask(old_umask);
    CHECK(mulib_sstore_read(d, "blob", dst, &len) == MULIB_SC_OK &&
          len == first.len && memcmp(out, first.ptr, len) == 0);
    CHECK(mulib_sstore_read(d, "../blob", dst, &len) == MULIB_SC_INVALID);
    CHECK(symlinkat("blob", d, "link") == 0);
    CHECK(mulib_sstore_read(d, "link", dst, &len) == MULIB_SC_DENIED &&
          mulib_sstore_write(p, d, "link", first) == MULIB_SC_DENIED &&
          mulib_sstore_remove(d, "link") == MULIB_SC_DENIED);
    CHECK(unlinkat(d, "link", 0) == 0);
    CHECK(linkat(d, "blob", d, "hard", 0) == 0);
    CHECK(mulib_sstore_read(d, "blob", dst, &len) == MULIB_SC_DENIED &&
          mulib_sstore_write(p, d, "blob", first) == MULIB_SC_DENIED &&
          mulib_sstore_remove(d, "blob") == MULIB_SC_DENIED);
    CHECK(unlinkat(d, "hard", 0) == 0);
    CHECK(fchmodat(d, "blob", 0644, 0) == 0);
    CHECK(mulib_sstore_read(d, "blob", dst, &len) == MULIB_SC_DENIED &&
          mulib_sstore_remove(d, "blob") == MULIB_SC_DENIED);
    CHECK(fchmodat(d, "blob", 0600, 0) == 0);

#ifdef MULIB_TEST_STORE_FAULTS
    memset(out, 0xc5, sizeof out);
    len = 42;
    read_fault = 2;
    CHECK(mulib_sstore_read(d, "blob", dst, &len) == MULIB_SC_IO);
    CHECK(len == 42 && out[0] == 0 && filled(out + 1, sizeof out - 1, 0xc5));
    write_fault = 2;
    CHECK(mulib_sstore_write(p, d, "blob", second) == MULIB_SC_IO);
    fsync_fault = 1;
    CHECK(mulib_sstore_write(p, d, "blob", second) == MULIB_SC_IO);
    CHECK(mulib_sstore_read(d, "blob", dst, &len) == MULIB_SC_OK &&
          len == first.len && memcmp(out, first.ptr, len) == 0);
    fsync_fault = 3;
    CHECK(mulib_sstore_write(p, d, "blob", second) == MULIB_SC_OK); /* EINTR retried. */
    fsync_fault = 2;
    CHECK(mulib_sstore_write(p, d, "blob", first) == MULIB_SC_UNCERTAIN);
    CHECK(mulib_sstore_read(d, "blob", dst, &len) == MULIB_SC_OK &&
          len == first.len && memcmp(out, first.ptr, len) == 0);
    fsync_fault = 1;
    CHECK(mulib_sstore_remove(d, "blob") == MULIB_SC_UNCERTAIN);
#else
    CHECK(mulib_sstore_write(p, d, "blob", second) == MULIB_SC_OK);
    CHECK(mulib_sstore_remove(d, "blob") == MULIB_SC_OK);
#endif
    CHECK(mulib_sstore_read(d, "blob", dst, &len) == MULIB_SC_NOTFOUND);
    CHECK(close(d) == 0 && rmdir(dir) == 0);
}

int main(void)
{
    mulib_scr_provider provider;
    mulib_scr_openssl(&provider);
    test_memory();
    test_wire_compatibility(&provider);
    test_vault_record(&provider, MULIB_SCR_AES128_GCM);
    test_vault_record(&provider, MULIB_SCR_AES256_GCM);
    test_catalog();
    test_state();
    test_store(&provider);
    puts("secret storage hardening: OK");
    return 0;
}
