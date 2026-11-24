/**
 * @file mulib-uniproc.h
 * @brief Strict UTF conversion, Unicode normalization and case mapping via ICU.
 * Define MULIB_IMPL_UNIPROC, MULIB_IMPL_UTF8 and MULIB_IMPL_DYNLOAD once.
 * No ICU headers/link dependency. Runtime ICU common library is required for
 * Unicode operations; conversion itself is independent. See the manual.
 */
#ifndef MULIB_UNIPROC_H
#define MULIB_UNIPROC_H
#include "mulib-dynload.h"
#include "mulib-utf8.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_uniproc mulib_uniproc_t;
typedef enum mulib_uniproc_form {
    MULIB_UNIPROC_NFC, MULIB_UNIPROC_NFD, MULIB_UNIPROC_NFKC, MULIB_UNIPROC_NFKD
} mulib_uniproc_form_t;
typedef enum mulib_uniproc_case {
    MULIB_UNIPROC_LOWER, MULIB_UNIPROC_UPPER, MULIB_UNIPROC_FOLD
} mulib_uniproc_case_t;
/** @brief Open ICU (NULL path probes native names). NULL means unavailable/OOM.
 * major=0 probes symbol suffixes; otherwise use the specified ICU major (1..999).
 * Contexts own their loader handle; callers serialize operations on a context. */
mulib_uniproc_t *mulib_uniproc_open(const char *path, unsigned major);
/** @brief Release context. ICU stays mapped until process exit so its shared
 * caches remain valid; no process-global u_cleanup is called. */
void mulib_uniproc_close(mulib_uniproc_t *context);
/** @brief Resolve an ICU common symbol with this context's version suffix.
 * Returns 0/-1; address borrowed until close. For extension modules. */
int mulib_uniproc_symbol(const mulib_uniproc_t *context, const char *name, void **address);
/** @brief Strict UTF-8 to malloc-owned UTF-16, terminated; length excludes NUL.
 * Optional offsets maps every UTF-16 unit (and terminal unit) to UTF-8 bytes.
 * Both halves of a surrogate pair map to the same byte. 0/-1, outputs unchanged
 * on failure. Input lengths limited to INT32_MAX for ICU interoperability. */
int mulib_uniproc_utf16(const char *text, size_t length, uint16_t **output,
                       size_t *units, size_t **offsets);
/** @brief Strict UTF-16 to malloc-owned, terminated UTF-8; 0/-1. */
int mulib_uniproc_utf8(const uint16_t *text, size_t units, char **output, size_t *length);
/** @brief Normalize UTF-8, preserving embedded NULs. malloc-owned result or NULL.
 * Optional output length excludes terminator. Invalid UTF-8/form fails. */
char *mulib_uniproc_normalize(mulib_uniproc_t *context, mulib_uniproc_form_t form,
                              const char *text, size_t length, size_t *output_length);
/** @brief Locale-sensitive lower/upper or default Unicode folding. NULL locale
 * means root locale, not process default. malloc-owned output or NULL. */
char *mulib_uniproc_case_map(mulib_uniproc_t *context, mulib_uniproc_case_t mode,
                             const char *locale, const char *text, size_t length,
                             size_t *output_length);
/** @brief Query Unicode general category (ICU UCharCategory numeric values),
 * canonical combining class and simple lower/upper mapping; 0/-1. */
int mulib_uniproc_properties(mulib_uniproc_t *context, uint32_t scalar,
                            int *category, unsigned *combining, uint32_t *lower,
                            uint32_t *upper);
/** @brief Query ISO 15924 script tag (four ASCII letters plus NUL) and Unicode
 * Default_Ignorable_Code_Point status. Either output may be NULL. 0/-1;
 * unchanged on failure. Common=Zyyy, Inherited=Zinh. Uses ICU Script property. */
int mulib_uniproc_script(mulib_uniproc_t *context, uint32_t scalar,
                         char script[5], int *default_ignorable);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_UNIPROC) && !defined(MULIB_UNIPROC_IMPLEMENTED)
#define MULIB_UNIPROC_IMPLEMENTED
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_uniproc { mulib_dynload_t library; unsigned major; };
int mulib_uniproc_symbol(const mulib_uniproc_t *c, const char *name, void **out) {
    char symbol[160];
    if (!c || !name || !out || strlen(name) > 140) return -1;
    if (c->major) (void)snprintf(symbol, sizeof symbol, "%s_%u", name, c->major);
    else (void)snprintf(symbol, sizeof symbol, "%s", name);
    return mulib_dynload_symbol(&c->library, symbol, out, NULL, 0) ? -1 : 0;
}
mulib_uniproc_t *mulib_uniproc_open(const char *path, unsigned major) {
    mulib_uniproc_t *c;
    void *p;
    unsigned i;
    const char *names[] = {"libicuuc.so", "libicuuc.dylib", "icuuc.dll"};
    char name[64];
    if (major > 999) return NULL;
    c = (mulib_uniproc_t *)calloc(1, sizeof *c);
    if (!c) return NULL;
    if (path) (void)mulib_dynload_open_pinned(&c->library, path, NULL, 0);
    else {
        for (i = 0; i < sizeof names / sizeof *names && !c->library.handle; ++i)
            (void)mulib_dynload_open_pinned(&c->library, names[i], NULL, 0);
        for (i = 150; i >= 40 && !c->library.handle; --i) {
#ifdef _WIN32
            (void)snprintf(name, sizeof name, "icuuc%u.dll", i);
#elif defined(__APPLE__)
            (void)snprintf(name, sizeof name, "libicuuc.%u.dylib", i);
#else
            (void)snprintf(name, sizeof name, "libicuuc.so.%u", i);
#endif
            (void)mulib_dynload_open_pinned(&c->library, name, NULL, 0);
        }
    }
    c->major = major;
    if (c->library.handle) {
        if (!mulib_uniproc_symbol(c, "u_getVersion", &p)) return c;
        if (!major) for (i = 150; i >= 40; --i) {
            c->major = i;
            if (!mulib_uniproc_symbol(c, "u_getVersion", &p)) return c;
        }
    }
    mulib_uniproc_close(c);
    return NULL;
}
void mulib_uniproc_close(mulib_uniproc_t *c) {
    if (c) { (void)mulib_dynload_close(&c->library, NULL, 0); free(c); }
}
int mulib_uniproc_utf16(const char *s, size_t n, uint16_t **out, size_t *units, size_t **offsets) {
    uint16_t *u;
    size_t *map = NULL, i = 0, count = 0, used;
    uint32_t cp;
    if (!out || !units || n > INT32_MAX || n > SIZE_MAX / sizeof(size_t) - 1 ||
        !mulib_utf8_validate((const unsigned char *)s, n)) return -1;
    u = (uint16_t *)malloc((n + 1) * sizeof *u);
    if (!u) return -1;
    if (offsets) {
        map = (size_t *)malloc((n + 1) * sizeof *map);
        if (!map) { free(u); return -1; }
    }
    while (i < n) {
        if (!mulib_utf8_decode((const unsigned char *)s + i, n - i, &cp, &used)) {
            free(map); free(u); return -1;
        }
        if (map) map[count] = i;
        if (cp > 0xffff) {
            cp -= 0x10000; u[count++] = (uint16_t)(0xd800 + (cp >> 10));
            if (map) map[count] = i;
            u[count++] = (uint16_t)(0xdc00 + (cp & 1023));
        } else u[count++] = (uint16_t)cp;
        i += used;
    }
    u[count] = 0;
    if (map) map[count] = n;
    *out = u; *units = count;
    if (offsets) *offsets = map;
    return 0;
}
int mulib_uniproc_utf8(const uint16_t *u, size_t n, char **out, size_t *length) {
    size_t i, k = 0, used;
    char *s;
    uint32_t cp;
    if ((!u && n) || !out || !length || n > INT32_MAX || n > (SIZE_MAX - 1) / 3) return -1;
    s = (char *)malloc(n * 3 + 1);
    if (!s) return -1;
    for (i = 0; i < n; ++i) {
        cp = u[i];
        if (cp >= 0xd800 && cp <= 0xdbff) {
            if (i + 1 == n || u[i + 1] < 0xdc00 || u[i + 1] > 0xdfff) { free(s); return -1; }
            cp = 0x10000 + ((cp - 0xd800) << 10) + (u[++i] - 0xdc00);
        } else if (cp >= 0xdc00 && cp <= 0xdfff) { free(s); return -1; }
        used = mulib_utf8_encode(cp, (unsigned char *)s + k, n * 3 - k);
        if (!used) { free(s); return -1; }
        k += used;
    }
    s[k] = 0; *out = s; *length = k; return 0;
}
/* memcpy avoids non-ISO object/function pointer casts on native loader ABIs. */
#define MULIB_UNIPROC_LOAD_(ctx, name, fn) \
    do { void *mulib_up_addr_; \
        if (sizeof(fn) != sizeof(mulib_up_addr_) || \
            mulib_uniproc_symbol(ctx, name, &mulib_up_addr_)) goto fail; \
        memcpy(&(fn), &mulib_up_addr_, sizeof(fn)); } while (0)
char *mulib_uniproc_normalize(mulib_uniproc_t *c, mulib_uniproc_form_t form,
                              const char *s, size_t n, size_t *length) {
    const void *(*get)(int32_t *);
    int32_t (*normalize)(const void *, const uint16_t *, int32_t, uint16_t *, int32_t, int32_t *);
    const char *names[] = {"unorm2_getNFCInstance", "unorm2_getNFDInstance", "unorm2_getNFKCInstance", "unorm2_getNFKDInstance"};
    uint16_t *u = NULL, *v = NULL;
    size_t units, bytes;
    int32_t error = 0, count;
    const void *normalizer;
    char *result = NULL;
    if (!c || form < MULIB_UNIPROC_NFC || form > MULIB_UNIPROC_NFKD) return NULL;
    MULIB_UNIPROC_LOAD_(c, names[form], get);
    MULIB_UNIPROC_LOAD_(c, "unorm2_normalize", normalize);
    if (mulib_uniproc_utf16(s, n, &u, &units, NULL)) goto fail;
    normalizer = get(&error);
    if (error > 0 || !normalizer) goto fail;
    count = normalize(normalizer, u, (int32_t)units, NULL, 0, &error);
    if ((error > 0 && error != 15) || count < 0 || count == INT32_MAX ||
        (size_t)count > SIZE_MAX / sizeof *v - 1) goto fail;
    v = (uint16_t *)malloc(((size_t)count + 1) * sizeof *v);
    if (!v) goto fail;
    error = 0;
    count = normalize(normalizer, u, (int32_t)units, v, count + 1, &error);
    if (error > 0 || count < 0 || mulib_uniproc_utf8(v, (size_t)count, &result, &bytes)) goto fail;
    if (length) *length = bytes;
fail:
    free(u); free(v); return result;
}
char *mulib_uniproc_case_map(mulib_uniproc_t *c, mulib_uniproc_case_t mode,
                             const char *locale, const char *s, size_t n, size_t *length) {
    int32_t (*map)(uint16_t *, int32_t, const uint16_t *, int32_t, const char *, int32_t *) = NULL;
    int32_t (*fold)(uint16_t *, int32_t, const uint16_t *, int32_t, uint32_t, int32_t *) = NULL;
    uint16_t *u = NULL, *v = NULL;
    size_t units, bytes;
    int32_t error = 0, count;
    char *result = NULL;
    if (!c || mode < MULIB_UNIPROC_LOWER || mode > MULIB_UNIPROC_FOLD) return NULL;
    if (mode == MULIB_UNIPROC_FOLD) { MULIB_UNIPROC_LOAD_(c, "u_strFoldCase", fold); }
    else { MULIB_UNIPROC_LOAD_(c, mode == MULIB_UNIPROC_LOWER ? "u_strToLower" : "u_strToUpper", map); }
    if (mulib_uniproc_utf16(s, n, &u, &units, NULL)) goto fail;
    count = fold ? fold(NULL, 0, u, (int32_t)units, 0, &error) :
        map(NULL, 0, u, (int32_t)units, locale ? locale : "", &error);
    if ((error > 0 && error != 15) || count < 0 || count == INT32_MAX ||
        (size_t)count > SIZE_MAX / sizeof *v - 1) goto fail;
    v = (uint16_t *)malloc(((size_t)count + 1) * sizeof *v);
    if (!v) goto fail;
    error = 0;
    count = fold ? fold(v, count + 1, u, (int32_t)units, 0, &error) :
        map(v, count + 1, u, (int32_t)units, locale ? locale : "", &error);
    if (error > 0 || count < 0 || mulib_uniproc_utf8(v, (size_t)count, &result, &bytes)) goto fail;
    if (length) *length = bytes;
fail:
    free(u); free(v); return result;
}
int mulib_uniproc_properties(mulib_uniproc_t *c, uint32_t cp, int *category,
                            unsigned *combining, uint32_t *lower, uint32_t *upper) {
    int8_t (*cat)(int32_t);
    uint8_t (*ccc)(int32_t);
    int32_t (*lo)(int32_t), (*up)(int32_t);
    if (!c || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return -1;
    MULIB_UNIPROC_LOAD_(c, "u_charType", cat);
    MULIB_UNIPROC_LOAD_(c, "u_getCombiningClass", ccc);
    MULIB_UNIPROC_LOAD_(c, "u_tolower", lo);
    MULIB_UNIPROC_LOAD_(c, "u_toupper", up);
    if (category) *category = cat((int32_t)cp);
    if (combining) *combining = ccc((int32_t)cp);
    if (lower) *lower = (uint32_t)lo((int32_t)cp);
    if (upper) *upper = (uint32_t)up((int32_t)cp);
    return 0;
fail:
    return -1;
}
#undef MULIB_UNIPROC_LOAD_
int mulib_uniproc_script(mulib_uniproc_t *c, uint32_t cp, char out[5], int *ignorable) {
    int32_t (*get_script)(int32_t, int32_t *);
    const char *(*name)(int32_t);
    int8_t (*property)(int32_t, int32_t);
    const char *tag;
    int32_t error = 0, script;
    void *address;
    if (!c || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return -1;
#define MULIB_UNIPROC_SCRIPT_LOAD_(fn, symbol) do { \
    if (sizeof(fn) != sizeof address || mulib_uniproc_symbol(c, symbol, &address)) return -1; \
    memcpy(&(fn), &address, sizeof(fn)); } while (0)
    MULIB_UNIPROC_SCRIPT_LOAD_(get_script, "uscript_getScript");
    MULIB_UNIPROC_SCRIPT_LOAD_(name, "uscript_getShortName");
    MULIB_UNIPROC_SCRIPT_LOAD_(property, "u_hasBinaryProperty");
#undef MULIB_UNIPROC_SCRIPT_LOAD_
    script = get_script((int32_t)cp, &error);
    if (error > 0 || !(tag = name(script)) || strlen(tag) != 4) return -1;
    if (out) memcpy(out, tag, 5);
    if (ignorable) *ignorable = property((int32_t)cp, 5) != 0; /* UCHAR_DEFAULT_IGNORABLE_CODE_POINT */
    return 0;
}
#ifdef __cplusplus
}
#endif
#endif
