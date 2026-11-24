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
typedef struct mulib_bidi_paragraph mulib_bidi_paragraph_t;
/** @brief Resolve and own a copy of paragraph text/state for subsequent line
 * extraction. Context is borrowed and must outlive paragraph. base as resolve.
 * NULL on invalid UTF-8, backend/OOM errors. Serialize calls on a paragraph. */
mulib_bidi_paragraph_t *mulib_bidi_paragraph_open(mulib_uniproc_t *context,
                                                const char *text, size_t length, int base);
/** @brief Apply UAX #9 line resetting/reordering to a paragraph byte range.
 * UTF-8 scalar boundaries are required. Runs use absolute paragraph byte offsets;
 * they retain paragraph embedding context (unlike resolving a substring).
 * Output must be empty. 0/-1, unchanged on failure. A line cannot cross a
 * paragraph separator; include it only at the end of its containing paragraph. */
int mulib_bidi_paragraph_line(mulib_bidi_paragraph_t *paragraph, size_t start,
                              size_t length, mulib_bidi_result_t *result);
/** @brief Release copied paragraph text, native state and offset mapping. */
void mulib_bidi_paragraph_close(mulib_bidi_paragraph_t *paragraph);
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
struct mulib_bidi_paragraph {
    void *state;
    uint16_t *text;
    size_t units, bytes, *map;
    void *(*open)(void);
    void (*close)(void *);
    void (*set)(void *, const uint16_t *, int32_t, uint8_t, uint8_t *, int32_t *);
    void (*line)(const void *, int32_t, int32_t, void *, int32_t *);
    int32_t (*count)(void *, int32_t *);
    int (*run)(void *, int32_t, int32_t *, int32_t *);
    uint8_t (*level)(const void *, int32_t), (*base)(const void *);
    int32_t (*paragraph)(const void *, int32_t, int32_t *, int32_t *, uint8_t *, int32_t *);
};
void mulib_bidi_paragraph_close(mulib_bidi_paragraph_t *p) {
    if (!p) return;
    if (p->state && p->close) p->close(p->state);
    free(p->text); free(p->map); free(p);
}
mulib_bidi_paragraph_t *mulib_bidi_paragraph_open(mulib_uniproc_t *c, const char *s, size_t n, int base) {
    mulib_bidi_paragraph_t *p;
    void *address;
    int32_t error = 0;
    if (!c || base < -2 || base > 1) return NULL;
    p = (mulib_bidi_paragraph_t *)calloc(1, sizeof *p);
    if (!p) return NULL;
#define MULIB_BIDI_PLOAD_(field, name) do { \
    if (sizeof p->field != sizeof address || mulib_uniproc_symbol(c, name, &address)) goto fail; \
    memcpy(&p->field, &address, sizeof p->field); } while (0)
    MULIB_BIDI_PLOAD_(open, "ubidi_open");
    MULIB_BIDI_PLOAD_(close, "ubidi_close");
    MULIB_BIDI_PLOAD_(set, "ubidi_setPara");
    MULIB_BIDI_PLOAD_(line, "ubidi_setLine");
    MULIB_BIDI_PLOAD_(count, "ubidi_countRuns");
    MULIB_BIDI_PLOAD_(run, "ubidi_getVisualRun");
    MULIB_BIDI_PLOAD_(level, "ubidi_getLevelAt");
    MULIB_BIDI_PLOAD_(base, "ubidi_getParaLevel");
    MULIB_BIDI_PLOAD_(paragraph, "ubidi_getParagraph");
#undef MULIB_BIDI_PLOAD_
    if (mulib_uniproc_utf16(s, n, &p->text, &p->units, &p->map)) goto fail;
    p->bytes = n; p->state = p->open();
    if (!p->state) goto fail;
    p->set(p->state, p->text, (int32_t)p->units,
           (uint8_t)(base == -1 ? 254 : base == -2 ? 255 : base), NULL, &error);
    if (error > 0) goto fail;
    return p;
fail:
    mulib_bidi_paragraph_close(p); return NULL;
}
static size_t mulib_bidi_unit_(const mulib_bidi_paragraph_t *p, size_t byte) {
    size_t lo = 0, hi = p->units;
    while (lo < hi) { size_t mid = lo + (hi-lo)/2; if (p->map[mid] < byte) lo = mid+1; else hi = mid; }
    return lo;
}
int mulib_bidi_paragraph_line(mulib_bidi_paragraph_t *p, size_t start, size_t length, mulib_bidi_result_t *out) {
    mulib_bidi_result_t r = {0};
    size_t begin, end;
    void *line;
    int32_t error = 0, nr, i, at, n;
    uint8_t paragraph_level;
    if (!p || !out || out->runs || out->count || start > p->bytes || length > p->bytes-start) return -1;
    begin = mulib_bidi_unit_(p, start); end = mulib_bidi_unit_(p, start+length);
    if (p->map[begin] != start || p->map[end] != start+length) return -1;
    paragraph_level = p->base(p->state);
    if (p->units) {
        (void)p->paragraph(p->state, (int32_t)(begin < p->units ? begin : p->units-1),
                           NULL, NULL, &paragraph_level, &error);
        if (error > 0) return -1;
    }
    if (!length) { r.paragraph_level = paragraph_level; *out = r; return 0; }
    line = p->open();
    if (!line) return -1;
    p->line(p->state, (int32_t)begin, (int32_t)end, line, &error);
    if (error > 0) goto fail;
    nr = p->count(line, &error);
    if (error > 0 || nr < 0 || (size_t)nr > SIZE_MAX / sizeof *r.runs) goto fail;
    if (nr) { r.runs = (mulib_bidi_run_t *)calloc((size_t)nr, sizeof *r.runs); if (!r.runs) goto fail; }
    /* ICU may optimize an all-even/all-odd line to a unidirectional object;
     * its getParaLevel can then expose that line's level rather than the
     * originating paragraph's base. Preserve the paragraph contract here. */
    r.count = (size_t)nr; r.paragraph_level = paragraph_level;
    for (i = 0; i < nr; ++i) {
        (void)p->run(line, i, &at, &n);
        if (at < 0 || n <= 0 || (size_t)at > end-begin || (size_t)n > end-begin-(size_t)at) goto fail;
        r.runs[i].start = p->map[begin+(size_t)at];
        r.runs[i].length = p->map[begin+(size_t)at+(size_t)n]-r.runs[i].start;
        r.runs[i].level = p->level(line, at);
    }
    p->close(line); *out = r; return 0;
fail:
    p->close(line); mulib_bidi_destroy(&r); return -1;
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
