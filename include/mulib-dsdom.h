/** @file mulib-dsdom.h
 * @brief Documentation DOM utilities over MuLIB's owned UTF-8 XML tree.
 * Define MULIB_IMPL_DSDOM, XML, STRBUF and UTF8 in one translation unit.
 */
#ifndef MULIB_DSDOM_H
#define MULIB_DSDOM_H
#include "mulib-xml.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Documentation nodes use XML's ownership, attributes and tree-edit API. */
typedef mulib_xml_node_t mulib_dsdom_t;
/** Copy a subtree, including attributes; NULL on invalid kind/depth/OOM. */
mulib_dsdom_t *mulib_dsdom_clone(const mulib_dsdom_t *node);
/** Create an element with optional copied text. Caller owns the result. */
mulib_dsdom_t *mulib_dsdom_element(const char *name, const char *text);
/** Concatenate descendant text/CDATA in tree order; malloc-owned, even if empty.
 * Comments are excluded. NULL on depth/allocation failure. */
char *mulib_dsdom_text(const mulib_dsdom_t *node, size_t *length);
/** Borrow the first element with the given id, including the starting node. */
mulib_dsdom_t *mulib_dsdom_find_id(const mulib_dsdom_t *node, const char *id);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DSDOM) && !defined(MULIB_DSDOM_IMPLEMENTED)
#define MULIB_DSDOM_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static mulib_dsdom_t *mulib_dsdom_clone_(const mulib_dsdom_t *n, unsigned depth) {
    mulib_dsdom_t *copy, *c;
    const mulib_dsdom_t *child;
    size_t i;
    mulib_xml_type_t type;
    if (!n) return NULL;
    type = mulib_xml_type(n);
    if (type == MULIB_XML_ELEMENT && depth > MULIB_XML_DEPTH_LIMIT) return NULL;
    copy = mulib_xml_new(type, type == MULIB_XML_ELEMENT ? mulib_xml_name(n) : mulib_xml_value(n));
    if (!copy) return NULL;
    for (i = 0; i < mulib_xml_attr_count(n); ++i)
        if (!mulib_xml_set_attr(copy, mulib_xml_attr_name(n, i), mulib_xml_attr_value(n, i))) goto fail;
    for (child = mulib_xml_first(n); child; child = mulib_xml_next(child)) {
        c = mulib_dsdom_clone_(child, depth + (type == MULIB_XML_ELEMENT));
        if (!c) goto fail;
        if (!mulib_xml_append(copy, c)) { mulib_xml_destroy(c); goto fail; }
    }
    return copy;
fail:
    mulib_xml_destroy(copy); return NULL;
}
mulib_dsdom_t *mulib_dsdom_clone(const mulib_dsdom_t *node) { return mulib_dsdom_clone_(node, 1); }
mulib_dsdom_t *mulib_dsdom_element(const char *name, const char *text) {
    mulib_dsdom_t *n = mulib_xml_new(MULIB_XML_ELEMENT, name), *t;
    if (!n || !text) return n;
    t = mulib_xml_new(MULIB_XML_TEXT, text);
    if (!t || !mulib_xml_append(n, t)) { mulib_xml_destroy(t); mulib_xml_destroy(n); return NULL; }
    return n;
}
static int mulib_dsdom_text_(const mulib_dsdom_t *n, mulib_strbuf_t *b, unsigned depth) {
    const mulib_dsdom_t *c;
    if (mulib_xml_type(n) == MULIB_XML_ELEMENT && depth > MULIB_XML_DEPTH_LIMIT) return 0;
    if (mulib_xml_type(n) == MULIB_XML_TEXT || mulib_xml_type(n) == MULIB_XML_CDATA)
        return mulib_strbuf_append(b, mulib_xml_value(n));
    for (c = mulib_xml_first(n); c; c = mulib_xml_next(c))
        if (!mulib_dsdom_text_(c, b, depth + (mulib_xml_type(n) == MULIB_XML_ELEMENT))) return 0;
    return 1;
}
char *mulib_dsdom_text(const mulib_dsdom_t *node, size_t *length) {
    mulib_strbuf_t b;
    if (length) *length = 0;
    mulib_strbuf_init(&b);
    if (!node || !mulib_strbuf_append(&b, "") || !mulib_dsdom_text_(node, &b, 1)) {
        mulib_strbuf_free(&b); return NULL;
    }
    if (length) *length = b.length;
    return b.data;
}
mulib_dsdom_t *mulib_dsdom_find_id(const mulib_dsdom_t *node, const char *id) {
    const mulib_dsdom_t *n = node;
    if (!n || !id) return NULL;
    for (;;) {
        const char *value = mulib_xml_attr(n, "id");
        if (value && !strcmp(value, id)) return (mulib_dsdom_t *)n;
        if (mulib_xml_first(n)) { n = mulib_xml_first(n); continue; }
        while (n != node && !mulib_xml_next(n)) n = mulib_xml_parent(n);
        if (n == node) return NULL;
        n = mulib_xml_next(n);
    }
}
#ifdef __cplusplus
}
#endif
#endif
