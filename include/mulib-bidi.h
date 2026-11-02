/** @file mulib-bidi.h
 * @brief Unicode bidi paragraph resolution and visual runs through ICU UAX #9.
 * Define MULIB_IMPL_BIDI plus UNIPROC/DYNLOAD/UTF8 switches.
 */
#ifndef MULIB_BIDI_H
#define MULIB_BIDI_H
#include "mulib-uniproc.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Logical UTF-8 byte interval in visual run order; level odd means RTL. */
typedef struct mulib_bidi_run { size_t start, length; unsigned level; } mulib_bidi_run_t;
typedef struct mulib_bidi_result {
    mulib_bidi_run_t *runs;
    size_t count;
    unsigned paragraph_level;
} mulib_bidi_result_t;
/** @brief Resolve one paragraph. base=0 LTR, 1 RTL, -1 auto/LTR, -2 auto/RTL.
 * Owns output runs on success (0); -1 on invalid UTF-8/backend/OOM. Output must
 * be empty; failure leaves it unchanged. Runs are for shaping, not byte reversal.
 * Explicit embeddings/isolates and paired brackets use ICU's Unicode version. */
int mulib_bidi_resolve(mulib_uniproc_t *context, const char *text, size_t length,
                       int base, mulib_bidi_result_t *result);
/** @brief Free runs and zero result; NULL harmless. */
void mulib_bidi_destroy(mulib_bidi_result_t *result);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BIDI) && !defined(MULIB_BIDI_IMPLEMENTED)
#define MULIB_BIDI_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
void mulib_bidi_destroy(mulib_bidi_result_t *r) {
    if (r) { free(r->runs); memset(r, 0, sizeof *r); }
}
int mulib_bidi_resolve(mulib_uniproc_t *c, const char *s, size_t n, int base, mulib_bidi_result_t *out) {
    void *(*open)(void);
    void (*close)(void *) = NULL;
    void (*set)(void *, const uint16_t *, int32_t, uint8_t, uint8_t *, int32_t *);
    int32_t (*count)(void *, int32_t *);
    int (*run)(void *, int32_t, int32_t *, int32_t *);
    uint8_t (*level)(const void *, int32_t), (*para)(const void *);
    void *it = NULL, *address;
    uint16_t *u = NULL;
    size_t units, *map = NULL;
    int32_t error = 0, nr, i, start, len;
    mulib_bidi_result_t r = {NULL, 0, 0};
    int result = -1;
    if (!c || !out || out->runs || out->count || base < -2 || base > 1) return -1;
#define MULIB_BIDI_LOAD_(name, fn) do { \
    if (sizeof(fn) != sizeof address || mulib_uniproc_symbol(c, name, &address)) goto done; \
    memcpy(&(fn), &address, sizeof(fn)); } while (0)
    MULIB_BIDI_LOAD_("ubidi_open", open);
    MULIB_BIDI_LOAD_("ubidi_close", close);
    MULIB_BIDI_LOAD_("ubidi_setPara", set);
    MULIB_BIDI_LOAD_("ubidi_countRuns", count);
    MULIB_BIDI_LOAD_("ubidi_getVisualRun", run);
    MULIB_BIDI_LOAD_("ubidi_getLevelAt", level);
    MULIB_BIDI_LOAD_("ubidi_getParaLevel", para);
#undef MULIB_BIDI_LOAD_
    if (mulib_uniproc_utf16(s, n, &u, &units, &map)) goto done;
    it = open();
    if (!it) goto done;
    set(it, u, (int32_t)units, (uint8_t)(base == -1 ? 254 : base == -2 ? 255 : base), NULL, &error);
    if (error > 0) goto done;
    nr = count(it, &error);
    if (error > 0 || nr < 0 || (size_t)nr > SIZE_MAX / sizeof *r.runs) goto done;
    if (nr) {
        r.runs = (mulib_bidi_run_t *)calloc((size_t)nr, sizeof *r.runs);
        if (!r.runs) goto done;
    }
    r.count = (size_t)nr; r.paragraph_level = para(it);
    for (i = 0; i < nr; ++i) {
        (void)run(it, i, &start, &len);
        if (start < 0 || len <= 0 || (size_t)start > units || (size_t)len > units - (size_t)start ||
            (start > 0 && u[start] >= 0xdc00 && u[start] <= 0xdfff) ||
            ((size_t)(start+len) < units && u[start+len] >= 0xdc00 && u[start+len] <= 0xdfff)) goto done;
        r.runs[i].start = map[start];
        r.runs[i].length = map[start + len] - map[start];
        r.runs[i].level = level(it, start);
    }
    *out = r; r.runs = NULL; result = 0;
done:
    if (it && close) close(it);
    free(r.runs); free(map); free(u); return result;
}
#ifdef __cplusplus
}
#endif
#endif
