/** @file mulib-unibreak.h
 * @brief ICU Unicode line, word, sentence and extended-grapheme boundaries.
 * Define MULIB_IMPL_UNIBREAK and UNIPROC/DYNLOAD/UTF8 switches.
 */
#ifndef MULIB_UNIBREAK_H
#define MULIB_UNIBREAK_H
#include "mulib-uniproc.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_unibreak_kind {
    MULIB_UNIBREAK_GRAPHEME, MULIB_UNIBREAK_WORD, MULIB_UNIBREAK_LINE, MULIB_UNIBREAK_SENTENCE
} mulib_unibreak_kind_t;
/** @brief UTF-8 byte boundary and ICU rule status (line: >=100 is mandatory). */
typedef struct mulib_unibreak_boundary { size_t offset; int status; } mulib_unibreak_boundary_t;
/** @brief Compute malloc-owned boundaries including 0 and end (empty has one).
 * Context borrowed during call. locale NULL selects root. Strict UTF-8, 0/-1;
 * outputs unchanged on failure. Data/dictionary rules follow runtime ICU. */
int mulib_unibreak_find(mulib_uniproc_t *context, mulib_unibreak_kind_t kind,
                        const char *locale, const char *text, size_t length,
                        mulib_unibreak_boundary_t **boundaries, size_t *count);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UNIBREAK) && !defined(MULIB_UNIBREAK_IMPLEMENTED)
#define MULIB_UNIBREAK_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_unibreak_find(mulib_uniproc_t *c, mulib_unibreak_kind_t kind, const char *locale,
                        const char *s, size_t n, mulib_unibreak_boundary_t **out, size_t *count) {
    void *(*open)(int, const char *, const uint16_t *, int32_t, int32_t *);
    void (*close)(void *) = NULL;
    int32_t (*first)(void *), (*next)(void *), (*status)(void *);
    void *it = NULL, *address;
    uint16_t *u = NULL;
    size_t units, *map = NULL, k = 0;
    int32_t error = 0, at;
    mulib_unibreak_boundary_t *b = NULL;
    int result = -1;
    if (!c || !out || !count || kind < MULIB_UNIBREAK_GRAPHEME || kind > MULIB_UNIBREAK_SENTENCE) return -1;
#define MULIB_UNIBREAK_LOAD_(name, fn) do { \
    if (sizeof(fn) != sizeof address || mulib_uniproc_symbol(c, name, &address)) goto done; \
    memcpy(&(fn), &address, sizeof(fn)); } while (0)
    MULIB_UNIBREAK_LOAD_("ubrk_open", open);
    MULIB_UNIBREAK_LOAD_("ubrk_close", close);
    MULIB_UNIBREAK_LOAD_("ubrk_first", first);
    MULIB_UNIBREAK_LOAD_("ubrk_next", next);
    MULIB_UNIBREAK_LOAD_("ubrk_getRuleStatus", status);
#undef MULIB_UNIBREAK_LOAD_
    if (mulib_uniproc_utf16(s, n, &u, &units, &map) || units > SIZE_MAX / sizeof *b - 1) goto done;
    b = (mulib_unibreak_boundary_t *)malloc((units + 1) * sizeof *b);
    if (!b) goto done;
    it = open((int)kind, locale ? locale : "", u, (int32_t)units, &error);
    if (!it || error > 0) goto done;
    for (at = first(it); at != -1; at = next(it)) {
        if (at < 0 || (size_t)at > units || k > units ||
            (k && map[at] <= b[k-1].offset) ||
            (at > 0 && (size_t)at < units && u[at] >= 0xdc00 && u[at] <= 0xdfff)) goto done;
        b[k].offset = map[at]; b[k++].status = status(it);
    }
    if (!k || b[0].offset != 0 || b[k-1].offset != n) goto done;
    *out = b; *count = k; b = NULL; result = 0;
done:
    if (it && close) close(it);
    free(b); free(map); free(u); return result;
}
#ifdef __cplusplus
}
#endif
#endif
