/** @file mulib-xtas.h
 * @brief Bounded, cancellable documentation-tree event stream.
 * Define MULIB_IMPL_XTAS plus DSDOM and its dependencies.
 */
#ifndef MULIB_XTAS_H
#define MULIB_XTAS_H
#include "mulib-dsdom.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_xtas_kind { MULIB_XTAS_ENTER, MULIB_XTAS_TEXT, MULIB_XTAS_LEAVE } mulib_xtas_kind_t;
/** Borrowed for the callback only. ENTER/LEAVE name an element; TEXT has text.
 * node gives access to all attributes. Root depth is zero. */
typedef struct mulib_xtas_event {
    mulib_xtas_kind_t kind;
    const mulib_dsdom_t *node;
    const char *name, *text;
    size_t depth;
} mulib_xtas_event_t;
/** Return nonzero to continue. Do not mutate the streamed tree. */
typedef int (*mulib_xtas_emit_fn)(void *user, const mulib_xtas_event_t *event);
/** 1 complete, 0 callback cancelled, -1 invalid tree/depth. No allocation.
 * Document wrappers are traversed, comments skipped, CDATA becomes TEXT. */
int mulib_xtas_stream(const mulib_dsdom_t *node, mulib_xtas_emit_fn emit, void *user);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_XTAS) && !defined(MULIB_XTAS_IMPLEMENTED)
#define MULIB_XTAS_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_xtas_stream_(const mulib_dsdom_t *n, mulib_xtas_emit_fn emit, void *user, size_t depth) {
    mulib_xtas_event_t e;
    const mulib_dsdom_t *c;
    int status;
    if (depth > MULIB_XML_DEPTH_LIMIT ||
        (mulib_xml_type(n) == MULIB_XML_ELEMENT && depth == MULIB_XML_DEPTH_LIMIT)) return -1;
    e.node = n; e.depth = depth; e.name = mulib_xml_name(n); e.text = mulib_xml_value(n);
    if (mulib_xml_type(n) == MULIB_XML_COMMENT) return 1;
    if (mulib_xml_type(n) == MULIB_XML_TEXT || mulib_xml_type(n) == MULIB_XML_CDATA) {
        e.kind = MULIB_XTAS_TEXT; return emit(user, &e) ? 1 : 0;
    }
    if (mulib_xml_type(n) == MULIB_XML_ELEMENT) {
        e.kind = MULIB_XTAS_ENTER; if (!emit(user, &e)) return 0;
    }
    for (c = mulib_xml_first(n); c; c = mulib_xml_next(c)) {
        status = mulib_xtas_stream_(c, emit, user, depth + (mulib_xml_type(n) == MULIB_XML_ELEMENT));
        if (status != 1) return status;
    }
    if (mulib_xml_type(n) == MULIB_XML_ELEMENT) {
        e.kind = MULIB_XTAS_LEAVE; if (!emit(user, &e)) return 0;
    }
    return 1;
}
int mulib_xtas_stream(const mulib_dsdom_t *node, mulib_xtas_emit_fn emit, void *user) {
    return node && emit ? mulib_xtas_stream_(node, emit, user, 0) : -1;
}
#ifdef __cplusplus
}
#endif
#endif
