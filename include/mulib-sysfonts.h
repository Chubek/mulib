/** @file mulib-sysfonts.h
 * @brief Owned system-font catalogs, matching and application fonts via Fontconfig.
 * Define MULIB_IMPL_SYSFONTS and MULIB_IMPL_DYNLOAD once. Native runtime is
 * optional at build time. No global Fontconfig initialization/finalization.
 */
#ifndef MULIB_SYSFONTS_H
#define MULIB_SYSFONTS_H
#include "mulib-dynload.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_sysfonts mulib_sysfonts_t;
/** @brief Owned strings plus collection/variation face index. */
typedef struct mulib_sysfonts_font { char *path, *family, *style; int index; } mulib_sysfonts_font_t;
/** @brief Create a private Fontconfig configuration loaded from system settings.
 * NULL path uses platform library name. NULL result means unavailable/OOM. */
mulib_sysfonts_t *mulib_sysfonts_open(const char *library_path);
/** @brief Destroy context; returned records remain valid. Fontconfig stays
 * mapped until process exit to preserve shared native caches. No FcFini call. */
void mulib_sysfonts_close(mulib_sysfonts_t *fonts);
/** @brief Match Fontconfig name syntax, e.g. "sans:weight=bold:lang=ar".
 * 0/-1, output must be zero-initialized; unchanged on failure. */
int mulib_sysfonts_match(mulib_sysfonts_t *fonts, const char *query, mulib_sysfonts_font_t *font);
/** @brief Enumerate installed faces, optionally filtered by a name pattern.
 * Returns malloc-owned records, 0/-1. Free records then array. */
int mulib_sysfonts_list(mulib_sysfonts_t *fonts, const char *query,
                        mulib_sysfonts_font_t **records, size_t *count);
/** @brief Register an application font file in this private configuration. 0/-1. */
int mulib_sysfonts_add(mulib_sysfonts_t *fonts, const char *path);
/** @brief Remove all application fonts from this private configuration. */
void mulib_sysfonts_clear(mulib_sysfonts_t *fonts);
/** @brief Release a record's strings and zero it. NULL harmless. */
void mulib_sysfonts_font_destroy(mulib_sysfonts_font_t *font);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYSFONTS) && !defined(MULIB_SYSFONTS_IMPLEMENTED)
#define MULIB_SYSFONTS_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Public Fontconfig ABI: FcFontSet, with opaque FcPattern pointers. */
typedef struct mulib_sysfonts_set_ { int count, capacity; void **patterns; } mulib_sysfonts_set_;
struct mulib_sysfonts {
    mulib_dynload_t library;
    void *config;
    void *(*init)(void);
    void (*destroy)(void *);
    void *(*parse)(const unsigned char *);
    void (*pattern_destroy)(void *);
    int (*substitute)(void *, void *, int);
    void (*defaults)(void *);
    void *(*match)(void *, void *, int *);
    int (*string)(const void *, const char *, int, unsigned char **);
    int (*integer)(const void *, const char *, int, int *);
    void *(*object_create)(void);
    int (*object_add)(void *, const char *);
    void (*object_destroy)(void *);
    mulib_sysfonts_set_ *(*list)(void *, void *, void *);
    void (*set_destroy)(mulib_sysfonts_set_ *);
    int (*add)(void *, const unsigned char *);
    void (*clear)(void *);
};
void mulib_sysfonts_font_destroy(mulib_sysfonts_font_t *f) {
    if (f) { free(f->path); free(f->family); free(f->style); memset(f, 0, sizeof *f); }
}
void mulib_sysfonts_close(mulib_sysfonts_t *f) {
    if (!f) return;
    if (f->config && f->destroy) f->destroy(f->config);
    (void)mulib_dynload_close(&f->library, NULL, 0); free(f);
}
mulib_sysfonts_t *mulib_sysfonts_open(const char *path) {
    mulib_sysfonts_t *f = (mulib_sysfonts_t *)calloc(1, sizeof *f);
    void *address;
    if (!f) return NULL;
#ifdef _WIN32
    if (!path) path = "libfontconfig-1.dll";
#elif defined(__APPLE__)
    if (!path) path = "libfontconfig.1.dylib";
#else
    if (!path) path = "libfontconfig.so.1";
#endif
    if (mulib_dynload_open_pinned(&f->library, path, NULL, 0)) goto fail;
#define MULIB_SYSFONTS_LOAD_(field, name) do { \
    if (sizeof f->field != sizeof address || mulib_dynload_symbol(&f->library, name, &address, NULL, 0)) goto fail; \
    memcpy(&f->field, &address, sizeof f->field); } while (0)
    MULIB_SYSFONTS_LOAD_(init, "FcInitLoadConfigAndFonts");
    MULIB_SYSFONTS_LOAD_(destroy, "FcConfigDestroy");
    MULIB_SYSFONTS_LOAD_(parse, "FcNameParse");
    MULIB_SYSFONTS_LOAD_(pattern_destroy, "FcPatternDestroy");
    MULIB_SYSFONTS_LOAD_(substitute, "FcConfigSubstitute");
    MULIB_SYSFONTS_LOAD_(defaults, "FcDefaultSubstitute");
    MULIB_SYSFONTS_LOAD_(match, "FcFontMatch");
    MULIB_SYSFONTS_LOAD_(string, "FcPatternGetString");
    MULIB_SYSFONTS_LOAD_(integer, "FcPatternGetInteger");
    MULIB_SYSFONTS_LOAD_(object_create, "FcObjectSetCreate");
    MULIB_SYSFONTS_LOAD_(object_add, "FcObjectSetAdd");
    MULIB_SYSFONTS_LOAD_(object_destroy, "FcObjectSetDestroy");
    MULIB_SYSFONTS_LOAD_(list, "FcFontList");
    MULIB_SYSFONTS_LOAD_(set_destroy, "FcFontSetDestroy");
    MULIB_SYSFONTS_LOAD_(add, "FcConfigAppFontAddFile");
    MULIB_SYSFONTS_LOAD_(clear, "FcConfigAppFontClear");
#undef MULIB_SYSFONTS_LOAD_
    f->config = f->init();
    if (f->config) return f;
fail:
    mulib_sysfonts_close(f); return NULL;
}
static char *mulib_sysfonts_copy_(const unsigned char *s) {
    size_t n = strlen((const char *)s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}
static int mulib_sysfonts_record_(mulib_sysfonts_t *f, void *p, mulib_sysfonts_font_t *out) {
    unsigned char *s;
    mulib_sysfonts_font_t r = {NULL, NULL, NULL, 0};
    if (f->string(p, "file", 0, &s) || !(r.path = mulib_sysfonts_copy_(s))) goto fail;
    if (f->string(p, "family", 0, &s)) s = (unsigned char *)"";
    if (!(r.family = mulib_sysfonts_copy_(s))) goto fail;
    if (f->string(p, "style", 0, &s)) s = (unsigned char *)"";
    if (!(r.style = mulib_sysfonts_copy_(s))) goto fail;
    (void)f->integer(p, "index", 0, &r.index);
    *out = r; return 0;
fail:
    mulib_sysfonts_font_destroy(&r); return -1;
}
int mulib_sysfonts_match(mulib_sysfonts_t *f, const char *query, mulib_sysfonts_font_t *out) {
    void *p, *match;
    int status = 0, result;
    if (!f || !query || !out || out->path || out->family || out->style) return -1;
    p = f->parse((const unsigned char *)query);
    if (!p) return -1;
    if (!f->substitute(f->config, p, 0)) { f->pattern_destroy(p); return -1; }
    f->defaults(p);
    match = f->match(f->config, p, &status);
    f->pattern_destroy(p);
    if (!match) return -1;
    result = status ? -1 : mulib_sysfonts_record_(f, match, out);
    f->pattern_destroy(match); return result;
}
int mulib_sysfonts_list(mulib_sysfonts_t *f, const char *query, mulib_sysfonts_font_t **out, size_t *count) {
    void *p = NULL, *objects = NULL;
    mulib_sysfonts_set_ *set = NULL;
    mulib_sysfonts_set_ view;
    mulib_sysfonts_font_t *r = NULL;
    size_t i = 0;
    int result = -1;
    if (!f || !out || !count) return -1;
    p = f->parse((const unsigned char *)(query ? query : ""));
    objects = f->object_create();
    if (!p || !objects || !f->object_add(objects, "file") || !f->object_add(objects, "family") ||
        !f->object_add(objects, "style") || !f->object_add(objects, "index")) goto done;
    set = f->list(f->config, p, objects);
    if (!set) goto done;
    memcpy(&view, set, sizeof view);
    if (view.count < 0 || view.capacity < view.count ||
        (view.count && !view.patterns) || (size_t)view.count > SIZE_MAX / sizeof *r) goto done;
    if (view.count) {
        r = (mulib_sysfonts_font_t *)calloc((size_t)view.count, sizeof *r);
        if (!r) goto done;
    }
    for (i = 0; i < (size_t)view.count; ++i) {
        void *pattern;
        memcpy(&pattern, (const unsigned char *)view.patterns + i*sizeof pattern, sizeof pattern);
        if (!pattern || mulib_sysfonts_record_(f, pattern, &r[i])) goto done;
    }
    *out = r; *count = (size_t)view.count; r = NULL; result = 0;
done:
    if (r) { while (i) mulib_sysfonts_font_destroy(&r[--i]); free(r); }
    if (set) f->set_destroy(set);
    if (objects) f->object_destroy(objects);
    if (p) f->pattern_destroy(p);
    return result;
}
int mulib_sysfonts_add(mulib_sysfonts_t *f, const char *path) {
    return f && path && f->add(f->config, (const unsigned char *)path) ? 0 : -1;
}
void mulib_sysfonts_clear(mulib_sysfonts_t *f) { if (f) f->clear(f->config); }
#ifdef __cplusplus
}
#endif
#endif
