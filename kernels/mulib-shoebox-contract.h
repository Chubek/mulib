/** @file mulib-shoebox-contract.h
 * @brief Versioned Shoebox multicall applet contract, dispatch and bounded I/O.
 * Define MULIB_IMPL_SHOEBOX_CONTRACT once. No applets or process-global state.
 */
#ifndef MULIB_SHOEBOX_CONTRACT_H
#define MULIB_SHOEBOX_CONTRACT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_SHOEBOX_ABI_VERSION 1u
enum { MULIB_SHOEBOX_OK = 0, MULIB_SHOEBOX_INVALID = -1, MULIB_SHOEBOX_ABI = -2,
       MULIB_SHOEBOX_DUPLICATE = -3, MULIB_SHOEBOX_NOT_FOUND = -4,
       MULIB_SHOEBOX_IO = -5, MULIB_SHOEBOX_CANCELLED = -6 };
enum { MULIB_SHOEBOX_STDIN = 0, MULIB_SHOEBOX_STDOUT = 1, MULIB_SHOEBOX_STDERR = 2 };
/** Callbacks return zero or a host error; count reports progress <= requested.
 * A zero-byte successful read is EOF. A zero-byte write is an I/O error.
 * Streams are logical identifiers, not necessarily OS file descriptors. */
typedef struct {
    void *context;
    int (*read)(void *, unsigned, void *, size_t, size_t *);
    int (*write)(void *, unsigned, const void *, size_t, size_t *);
    int (*cancelled)(void *);
} mulib_shoebox_io_t;
/** Context and argv/environment strings are borrowed for the duration of main.
 * env_count is explicit (no sentinel required). Applets return 0..255 and must
 * clean up their own resources; they must not call exit or retain this context.
 * Hosts decide privileges, signals, isolation, locale and concurrency policy. */
typedef struct {
    uint32_t abi_version; size_t struct_size;
    mulib_shoebox_io_t io;
    const char *const *environment; size_t env_count;
    void *host;
} mulib_shoebox_context_t;
typedef int (*mulib_shoebox_main_fn)(const mulib_shoebox_context_t *, int, const char *const *);
typedef struct {
    uint32_t abi_version; size_t struct_size;
    const char *name, *summary, *usage;
    mulib_shoebox_main_fn main;
} mulib_shoebox_applet_t;
/** @brief Check version, size, callbacks, canonical ASCII names and duplicates.
 * Empty registry is valid. Version 1 descriptors must have exactly this size. */
int mulib_shoebox_validate(const mulib_shoebox_applet_t *applets, size_t count);
/** @brief Look up exact applet name; returns borrowed pointer, or NULL. */
const mulib_shoebox_applet_t *mulib_shoebox_find(const mulib_shoebox_applet_t *applets, size_t count, const char *name);
/** @brief Dispatch basename(argv[0]), or argv[1] when basename is launcher_name.
 * Applet sees its own argv[0]; argv is never modified. Contract result is
 * separate from applet exit_status. Unknown applet sets exit_status=127;
 * other pre-dispatch errors set 125. No help/list utility is built in. */
int mulib_shoebox_dispatch(const mulib_shoebox_applet_t *applets, size_t count,
    const char *launcher_name, const mulib_shoebox_context_t *context,
    int argc, const char *const *argv, int *exit_status);
/** @brief Complete partial writes; stop on error, cancellation or zero progress.
 * written reports completed bytes, including progress on an error. */
int mulib_shoebox_write_all(const mulib_shoebox_context_t *context, unsigned stream,
                           const void *data, size_t bytes, size_t *written);
/** @brief Borrow first exact NAME= value, or NULL; no global environment. */
const char *mulib_shoebox_getenv(const mulib_shoebox_context_t *context, const char *name);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SHOEBOX_CONTRACT) && !defined(MULIB_SHOEBOX_CONTRACT_IMPLEMENTED)
#define MULIB_SHOEBOX_CONTRACT_IMPLEMENTED
#include <string.h>
static int mulib_shoebox_name_(const char *name) {
    const unsigned char *p = (const unsigned char *)name;
    if (!p || !*p || *p == '-' || !strcmp(name, ".") || !strcmp(name, "..")) return 0;
    for (; *p; ++p) if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
        (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' || *p == '.' || *p == '[' || *p == ']')) return 0;
    return 1;
}
static int mulib_shoebox_context_(const mulib_shoebox_context_t *c) {
    size_t i;
    if (!c || c->abi_version != MULIB_SHOEBOX_ABI_VERSION || c->struct_size < sizeof(*c)) return MULIB_SHOEBOX_ABI;
    if (!c->environment && c->env_count) return MULIB_SHOEBOX_INVALID;
    for (i = 0; i < c->env_count; ++i) if (!c->environment[i]) return MULIB_SHOEBOX_INVALID;
    return 0;
}
int mulib_shoebox_validate(const mulib_shoebox_applet_t *a, size_t count) {
    size_t i, j;
    if ((!a && count) || count > SIZE_MAX / sizeof(*a)) return MULIB_SHOEBOX_INVALID;
    for (i = 0; i < count; ++i) {
        if (a[i].abi_version != MULIB_SHOEBOX_ABI_VERSION || a[i].struct_size != sizeof(*a)) return MULIB_SHOEBOX_ABI;
        if (!mulib_shoebox_name_(a[i].name) || !a[i].main) return MULIB_SHOEBOX_INVALID;
        for (j = 0; j < i; ++j) if (!strcmp(a[i].name, a[j].name)) return MULIB_SHOEBOX_DUPLICATE;
    }
    return 0;
}
const mulib_shoebox_applet_t *mulib_shoebox_find(const mulib_shoebox_applet_t *a, size_t count, const char *name) {
    size_t i;
    if (!name || mulib_shoebox_validate(a, count)) return NULL;
    for (i = 0; i < count; ++i) if (!strcmp(a[i].name, name)) return a + i;
    return NULL;
}
int mulib_shoebox_dispatch(const mulib_shoebox_applet_t *a, size_t count,
    const char *launcher, const mulib_shoebox_context_t *c, int argc, const char *const *argv, int *status) {
    const mulib_shoebox_applet_t *applet; const char *name, *p; int i, rc;
    if (!status) return MULIB_SHOEBOX_INVALID;
    *status = 125;
    rc = mulib_shoebox_validate(a, count); if (rc) return rc;
    rc = mulib_shoebox_context_(c); if (rc) return rc;
    if (!mulib_shoebox_name_(launcher) || argc < 1 || !argv) return MULIB_SHOEBOX_INVALID;
    for (i = 0; i < argc; ++i) if (!argv[i]) return MULIB_SHOEBOX_INVALID;
    name = argv[0]; for (p = name; *p; ++p) if (*p == '/') name = p + 1;
    if (!strcmp(name, launcher)) {
        if (argc < 2) return MULIB_SHOEBOX_INVALID;
        --argc; ++argv; name = argv[0];
    }
    applet = mulib_shoebox_find(a, count, name);
    if (!applet) { *status = 127; return MULIB_SHOEBOX_NOT_FOUND; }
    if (c->io.cancelled && c->io.cancelled(c->io.context)) return MULIB_SHOEBOX_CANCELLED;
    rc = applet->main(c, argc, argv);
    if (rc < 0 || rc > 255) return MULIB_SHOEBOX_INVALID;
    *status = rc; return 0;
}
int mulib_shoebox_write_all(const mulib_shoebox_context_t *c, unsigned stream,
                           const void *data, size_t bytes, size_t *written) {
    size_t n; int rc; const uint8_t *p = (const uint8_t *)data;
    if (!written) return MULIB_SHOEBOX_INVALID;
    *written = 0; rc = mulib_shoebox_context_(c); if (rc) return rc;
    if ((!p && bytes) || (stream != MULIB_SHOEBOX_STDOUT && stream != MULIB_SHOEBOX_STDERR) || !c->io.write)
        return MULIB_SHOEBOX_INVALID;
    while (*written < bytes) {
        if (c->io.cancelled && c->io.cancelled(c->io.context)) return MULIB_SHOEBOX_CANCELLED;
        n = 0; rc = c->io.write(c->io.context, stream, p + *written, bytes - *written, &n);
        if (n > bytes - *written) return MULIB_SHOEBOX_IO;
        *written += n;
        if (rc || !n) return MULIB_SHOEBOX_IO;
    }
    return 0;
}
const char *mulib_shoebox_getenv(const mulib_shoebox_context_t *c, const char *name) {
    size_t i, n;
    if (mulib_shoebox_context_(c) || !name || !*name || strchr(name, '=')) return NULL;
    n = strlen(name);
    for (i = 0; i < c->env_count; ++i)
        if (!strncmp(c->environment[i], name, n) && c->environment[i][n] == '=') return c->environment[i] + n + 1;
    return NULL;
}
#endif
