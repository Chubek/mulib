/** @file mulib-uniname.h
 * @brief Unicode character names and reverse lookup using the ICU database.
 * Define MULIB_IMPL_UNINAME and UNIPROC/DYNLOAD/UTF8 implementation switches.
 */
#ifndef MULIB_UNINAME_H
#define MULIB_UNINAME_H
#include "mulib-uniproc.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Return a malloc-owned Unicode name, or NULL for unnamed/invalid code
 * points or errors. extended=1 also names controls/unassigned/noncharacters. */
char *mulib_uniname_name(mulib_uniproc_t *context, uint32_t codepoint, int extended);
/** @brief Look up a Unicode name (case-insensitive ICU matching); 0/-1.
 * extended=1 accepts ICU extended names. Output unchanged on failure. */
int mulib_uniname_lookup(mulib_uniproc_t *context, const char *name, int extended,
                         uint32_t *codepoint);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UNINAME) && !defined(MULIB_UNINAME_IMPLEMENTED)
#define MULIB_UNINAME_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
char *mulib_uniname_name(mulib_uniproc_t *c, uint32_t cp, int extended) {
    int32_t (*fn)(int32_t, int, char *, int32_t, int32_t *);
    void *address;
    int32_t n, error = 0;
    char *s;
    if (!c || cp > 0x10ffff || (extended != 0 && extended != 1) ||
        sizeof fn != sizeof address || mulib_uniproc_symbol(c, "u_charName", &address)) return NULL;
    memcpy(&fn, &address, sizeof fn);
    n = fn((int32_t)cp, extended ? 2 : 0, NULL, 0, &error);
    if ((error > 0 && error != 15) || n <= 0 || n == INT32_MAX) return NULL;
    s = (char *)malloc((size_t)n + 1);
    if (!s) return NULL;
    error = 0;
    (void)fn((int32_t)cp, extended ? 2 : 0, s, n + 1, &error);
    if (error > 0) { free(s); return NULL; }
    return s;
}
int mulib_uniname_lookup(mulib_uniproc_t *c, const char *name, int extended, uint32_t *cp) {
    int32_t (*fn)(int, const char *, int32_t *);
    void *address;
    int32_t result, error = 0;
    if (!c || !name || !cp || (extended != 0 && extended != 1) ||
        sizeof fn != sizeof address || mulib_uniproc_symbol(c, "u_charFromName", &address)) return -1;
    memcpy(&fn, &address, sizeof fn);
    result = fn(extended ? 2 : 0, name, &error);
    if (error > 0 || result < 0 || result > 0x10ffff) return -1;
    *cp = (uint32_t)result; return 0;
}
#ifdef __cplusplus
}
#endif
#endif
