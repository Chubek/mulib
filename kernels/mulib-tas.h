/** @file mulib-tas.h
 * @brief Owned, lossless element/attribute/text/end documentation token stream.
 * Define MULIB_IMPL_TAS plus XTAS, DSDOM and their dependencies.
 */
#ifndef MULIB_TAS_H
#define MULIB_TAS_H
#include "mulib-xtas.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_tas_kind { MULIB_TAS_OPEN, MULIB_TAS_ATTRIBUTE, MULIB_TAS_TEXT, MULIB_TAS_CLOSE } mulib_tas_kind_t;
typedef struct mulib_tas_token { mulib_tas_kind_t kind; const char *name, *value; } mulib_tas_token_t;
typedef struct mulib_tas mulib_tas_t;
/** Create/free an independent stream. destroy(NULL) is harmless. */
mulib_tas_t *mulib_tas_create(void);
void mulib_tas_destroy(mulib_tas_t *stream);
/** Copy a token. OPEN/CLOSE require name and no value, ATTRIBUTE requires
 * both, TEXT requires value and no name. 1/0, unchanged on failure.
 * At most 1000000 tokens; names/text must be XML-valid. */
int mulib_tas_push(mulib_tas_t *stream, mulib_tas_kind_t kind, const char *name, const char *value);
size_t mulib_tas_count(const mulib_tas_t *stream);
/** Borrow a token until push/destroy; NULL out of bounds. */
const mulib_tas_token_t *mulib_tas_at(const mulib_tas_t *stream, size_t index);
/** Snapshot a DOM, excluding comments and normalizing CDATA to TEXT. */
mulib_tas_t *mulib_tas_from_dom(const mulib_dsdom_t *node);
/** Validate balanced names, a single root, attribute placement/uniqueness and
 * XML depth. Return an owned element root or NULL; optional static diagnostic. */
mulib_dsdom_t *mulib_tas_to_dom(const mulib_tas_t *stream, mulib_xml_error_t *error);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TAS) && !defined(MULIB_TAS_IMPLEMENTED)
#define MULIB_TAS_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_tas { mulib_tas_token_t *tokens; size_t count, capacity; };
static char *mulib_tas_copy_(const char *s) {
    size_t n = strlen(s); char *p;
    if (n == SIZE_MAX) return NULL;
    p = (char *)malloc(n + 1); if (p) memcpy(p, s, n + 1); return p;
}
mulib_tas_t *mulib_tas_create(void) { return (mulib_tas_t *)calloc(1, sizeof(mulib_tas_t)); }
void mulib_tas_destroy(mulib_tas_t *s) {
    size_t i;
    if (!s) return;
    for (i = 0; i < s->count; ++i) { free((void *)s->tokens[i].name); free((void *)s->tokens[i].value); }
    free(s->tokens); free(s);
}
int mulib_tas_push(mulib_tas_t *s, mulib_tas_kind_t kind, const char *name, const char *value) {
    mulib_tas_token_t t, *p;
    size_t cap;
    if (!s || s->count == 1000000 || kind < MULIB_TAS_OPEN || kind > MULIB_TAS_CLOSE) return 0;
    if (kind == MULIB_TAS_TEXT ? (name || !value) : (!name || !mulib_xml_valid_name(name))) return 0;
    if (kind == MULIB_TAS_ATTRIBUTE ? !value : (kind != MULIB_TAS_TEXT && value != NULL)) return 0;
    if (value && !mulib_xml_valid_text(value, strlen(value))) return 0;
    t.kind = kind; t.name = name ? mulib_tas_copy_(name) : NULL; t.value = value ? mulib_tas_copy_(value) : NULL;
    if ((name && !t.name) || (value && !t.value)) goto fail;
    if (s->count == s->capacity) {
        cap = s->capacity ? s->capacity * 2 : 32;
        if (cap > SIZE_MAX / sizeof *p) goto fail;
        p = (mulib_tas_token_t *)realloc(s->tokens, cap * sizeof *p);
        if (!p) goto fail;
        s->tokens = p; s->capacity = cap;
    }
    s->tokens[s->count++] = t; return 1;
fail:
    free((void *)t.name); free((void *)t.value); return 0;
}
size_t mulib_tas_count(const mulib_tas_t *s) { return s ? s->count : 0; }
const mulib_tas_token_t *mulib_tas_at(const mulib_tas_t *s, size_t i) { return s && i < s->count ? &s->tokens[i] : NULL; }
static int mulib_tas_event_(void *user, const mulib_xtas_event_t *e) {
    mulib_tas_t *s = (mulib_tas_t *)user;
    size_t i;
    if (e->kind == MULIB_XTAS_TEXT) return mulib_tas_push(s, MULIB_TAS_TEXT, NULL, e->text);
    if (e->kind == MULIB_XTAS_LEAVE) return mulib_tas_push(s, MULIB_TAS_CLOSE, e->name, NULL);
    if (!mulib_tas_push(s, MULIB_TAS_OPEN, e->name, NULL)) return 0;
    for (i = 0; i < mulib_xml_attr_count(e->node); ++i)
        if (!mulib_tas_push(s, MULIB_TAS_ATTRIBUTE, mulib_xml_attr_name(e->node, i), mulib_xml_attr_value(e->node, i))) return 0;
    return 1;
}
mulib_tas_t *mulib_tas_from_dom(const mulib_dsdom_t *n) {
    mulib_tas_t *s = mulib_tas_create();
    if (s && mulib_xtas_stream(n, mulib_tas_event_, s) != 1) { mulib_tas_destroy(s); s = NULL; }
    return s;
}
mulib_dsdom_t *mulib_tas_to_dom(const mulib_tas_t *s, mulib_xml_error_t *e) {
    mulib_dsdom_t *root = NULL, *stack[MULIB_XML_DEPTH_LIMIT], *n;
    size_t depth = 0, i = 0;
    int attributes = 0;
    const char *message = "malformed token stream";
    if (e) { e->offset = 0; e->message = NULL; }
    if (!s) goto fail;
    for (i = 0; i < s->count; ++i) {
        const mulib_tas_token_t *t = &s->tokens[i];
        switch (t->kind) {
        case MULIB_TAS_OPEN:
            if (depth == MULIB_XML_DEPTH_LIMIT || (!depth && root)) goto fail;
            n = mulib_xml_new(MULIB_XML_ELEMENT, t->name);
            if (!n) { message = "allocation failure"; goto fail; }
            if (depth && !mulib_xml_append(stack[depth-1], n)) { mulib_xml_destroy(n); goto fail; }
            if (!depth) root = n;
            stack[depth++] = n; attributes = 1; break;
        case MULIB_TAS_ATTRIBUTE:
            if (!depth || !attributes || mulib_xml_attr(stack[depth-1], t->name)) goto fail;
            if (!mulib_xml_set_attr(stack[depth-1], t->name, t->value)) { message = "allocation failure"; goto fail; }
            break;
        case MULIB_TAS_TEXT:
            if (!depth) goto fail;
            attributes = 0; n = mulib_xml_new(MULIB_XML_TEXT, t->value);
            if (!n) { message = "allocation failure"; goto fail; }
            if (!mulib_xml_append(stack[depth-1], n)) { mulib_xml_destroy(n); goto fail; }
            break;
        case MULIB_TAS_CLOSE:
            if (!depth || strcmp(mulib_xml_name(stack[depth-1]), t->name)) goto fail;
            --depth; attributes = 0; break;
        }
    }
    if (depth || !root) goto fail;
    return root;
fail:
    if (e) { e->offset = i; e->message = message; }
    mulib_xml_destroy(root); return NULL;
}
#ifdef __cplusplus
}
#endif
#endif
