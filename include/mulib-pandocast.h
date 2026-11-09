/** @file mulib-pandocast.h
 * @brief DocstrIL to Pandoc JSON AST 1.23.1, without a Pandoc dependency.
 * Define MULIB_IMPL_PANDOCAST plus DSDOM, JSON and dependencies.
 */
#ifndef MULIB_PANDOCAST_H
#define MULIB_PANDOCAST_H
#include "mulib-dsdom.h"
#include "mulib-json.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Owned JSON AST with pandoc-api-version, meta and blocks. Extension elements
 * become Span/Div with data-docstril attributes; SVG becomes RawBlock(html).
 * Simple rectangular tables use the modern Table/Row/Cell schema. */
mulib_json_t *mulib_pandocast_from_docstril(const mulib_dsdom_t *document, mulib_xml_error_t *error);
/** malloc-owned compact JSON; optional length is zero on failure. */
char *mulib_pandocast_serialize(const mulib_dsdom_t *document, size_t *length, mulib_xml_error_t *error);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PANDOCAST) && !defined(MULIB_PANDOCAST_IMPLEMENTED)
#define MULIB_PANDOCAST_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
/* These helpers always consume their value, including on failure. */
static int mulib_pandocast_push_(mulib_json_t *a, mulib_json_t *v) {
    if (v && mulib_json_append(a,v)) return 1;
    mulib_json_destroy(v); return 0;
}
static int mulib_pandocast_set_(mulib_json_t *a, const char *key, mulib_json_t *v) {
    if (v && mulib_json_set(a,key,v)) return 1;
    mulib_json_destroy(v); return 0;
}
static mulib_json_t *mulib_pandocast_array_(void) { return mulib_json_new(MULIB_JSON_ARRAY); }
static mulib_json_t *mulib_pandocast_tag_(const char *tag, mulib_json_t *content) {
    if (!content && strcmp(tag,"Space") && strcmp(tag,"SoftBreak") && strcmp(tag,"LineBreak") &&
        strcmp(tag,"HorizontalRule") && strcmp(tag,"AlignDefault") && strcmp(tag,"AlignLeft") && strcmp(tag,"AlignCenter") && strcmp(tag,"AlignRight") && strcmp(tag,"ColWidthDefault") &&
        strcmp(tag,"DefaultStyle") && strcmp(tag,"DefaultDelim")) return NULL;
    mulib_json_t *o = mulib_json_new(MULIB_JSON_OBJECT);
    if (!o || !mulib_pandocast_set_(o,"t",mulib_json_string(tag))) { mulib_json_destroy(content); mulib_json_destroy(o); return NULL; }
    if (content && !mulib_pandocast_set_(o,"c",content)) { mulib_json_destroy(o); return NULL; }
    return o;
}
static mulib_json_t *mulib_pandocast_attr_(const mulib_dsdom_t *n, int extension) {
    mulib_json_t *a = mulib_pandocast_array_(), *classes = NULL, *pairs = NULL, *pair;
    const char *id = mulib_xml_attr(n,"id"), *cl = mulib_xml_attr(n,"class");
    size_t i;
    if (!a || !mulib_pandocast_push_(a,mulib_json_string(id ? id : ""))) goto fail;
    classes = mulib_pandocast_array_(); if (!classes) goto fail;
    while (cl && *cl) {
        const char *start;
        while (*cl == ' ' || *cl == '\t' || *cl == '\n') ++cl;
        start = cl; while (*cl && *cl != ' ' && *cl != '\t' && *cl != '\n') ++cl;
        if (cl != start && !mulib_pandocast_push_(classes,mulib_json_string_n(start,(size_t)(cl-start)))) goto fail;
    }
    if (n && mulib_xml_name(n) && !strcmp(mulib_xml_name(n),"code-block")) {
        const char *language = mulib_xml_attr(n,"language");
        if (language && *language && !mulib_pandocast_push_(classes,mulib_json_string(language))) goto fail;
    }
    if (!mulib_json_append(a,classes)) goto fail;
    classes = NULL; pairs = mulib_pandocast_array_(); if (!pairs) goto fail;
    for (i = 0; i < mulib_xml_attr_count(n) + (extension ? 1u : 0u); ++i) {
        const char *key = i == mulib_xml_attr_count(n) ? "data-docstril" : mulib_xml_attr_name(n,i);
        const char *value = i == mulib_xml_attr_count(n) ? mulib_xml_name(n) : mulib_xml_attr_value(n,i);
        if (!strcmp(key,"id") || !strcmp(key,"class")) continue;
        pair = mulib_pandocast_array_();
        if (!pair || !mulib_pandocast_push_(pair,mulib_json_string(key)) ||
            !mulib_pandocast_push_(pair,mulib_json_string(value))) { mulib_json_destroy(pair); goto fail; }
        if (!mulib_pandocast_push_(pairs,pair)) goto fail;
    }
    if (!mulib_json_append(a,pairs)) goto fail;
    return a;
fail:
    mulib_json_destroy(classes); mulib_json_destroy(pairs); mulib_json_destroy(a); return NULL;
}
static int mulib_pandocast_text_(mulib_json_t *a, const char *s) {
    size_t at = 0, start;
    while (s[at]) {
        if (s[at] == ' ' || s[at] == '\t' || s[at] == '\n' || s[at] == '\r') {
            const char *tag = s[at] == '\n' || s[at] == '\r' ? "SoftBreak" : "Space";
            ++at;
            if (!mulib_pandocast_push_(a,mulib_pandocast_tag_(tag,NULL))) return 0;
        } else {
            start = at; while (s[at] && s[at] != ' ' && s[at] != '\t' && s[at] != '\n' && s[at] != '\r') ++at;
            if (!mulib_pandocast_push_(a,mulib_pandocast_tag_("Str",mulib_json_string_n(s+start,at-start)))) return 0;
        }
    }
    return 1;
}
static mulib_json_t *mulib_pandocast_inlines_(const mulib_dsdom_t *n, unsigned depth);
static mulib_json_t *mulib_pandocast_blocks_(const mulib_dsdom_t *n, unsigned depth);
static int mulib_pandocast_inline_(mulib_json_t *out, const mulib_dsdom_t *n, unsigned depth) {
    const char *name = mulib_xml_name(n), *tag = NULL, *target;
    mulib_json_t *c = NULL, *a = NULL, *dest = NULL;
    char *text;
    if (depth > 96) return 0;
    if (!name) return mulib_pandocast_text_(out,mulib_xml_value(n));
    if (!strcmp(name,"strong")) tag = "Strong";
    else if (!strcmp(name,"emphasis")) tag = "Emph";
    else if (!strcmp(name,"underline")) tag = "Underline";
    else if (!strcmp(name,"strike")) tag = "Strikeout";
    else if (!strcmp(name,"superscript")) tag = "Superscript";
    else if (!strcmp(name,"subscript")) tag = "Subscript";
    if (tag) return mulib_pandocast_push_(out,mulib_pandocast_tag_(tag,mulib_pandocast_inlines_(n,depth+1)));
    if (!strcmp(name,"break")) return mulib_pandocast_push_(out,mulib_pandocast_tag_("LineBreak",NULL));
    a = mulib_pandocast_array_(); if (!a) goto fail;
    if (!mulib_pandocast_push_(a,mulib_pandocast_attr_(n,strcmp(name,"code") && strcmp(name,"link") && strcmp(name,"image")))) goto fail;
    if (!strcmp(name,"code")) {
        text = mulib_dsdom_text(n,NULL); if (!text) goto fail;
        c = mulib_json_string(text); free(text);
        if (!mulib_pandocast_push_(a,c)) goto fail;
        return mulib_pandocast_push_(out,mulib_pandocast_tag_("Code",a));
    }
    if (!mulib_pandocast_push_(a,mulib_pandocast_inlines_(n,depth+1))) goto fail;
    if (!strcmp(name,"link") || !strcmp(name,"image")) {
        target = mulib_xml_attr(n,!strcmp(name,"link") ? "href" : "src");
        dest = mulib_pandocast_array_();
        if (!dest || !mulib_pandocast_push_(dest,mulib_json_string(target ? target : "")) ||
            !mulib_pandocast_push_(dest,mulib_json_string(mulib_xml_attr(n,"title") ? mulib_xml_attr(n,"title") : ""))) goto fail;
        if (!mulib_json_append(a,dest)) goto fail;
        dest = NULL;
        tag = !strcmp(name,"link") ? "Link" : "Image";
    } else tag = "Span";
    return mulib_pandocast_push_(out,mulib_pandocast_tag_(tag,a));
fail:
    mulib_json_destroy(a); mulib_json_destroy(dest); return 0;
}
static mulib_json_t *mulib_pandocast_inlines_(const mulib_dsdom_t *n, unsigned depth) {
    mulib_json_t *a = mulib_pandocast_array_();
    const mulib_dsdom_t *c;
    if (!a) return NULL;
    for (c = mulib_xml_first(n); c; c = mulib_xml_next(c))
        if (!mulib_pandocast_inline_(a,c,depth+1)) { mulib_json_destroy(a); return NULL; }
    return a;
}
static const char *mulib_pandocast_align_(const mulib_dsdom_t *n) {
    const char *a=mulib_xml_attr(n,"align");
    return a&&!strcmp(a,"left")?"AlignLeft":a&&!strcmp(a,"center")?"AlignCenter":a&&!strcmp(a,"right")?"AlignRight":"AlignDefault";
}
static mulib_json_t *mulib_pandocast_row_(const mulib_dsdom_t *row, size_t columns, unsigned depth) {
    mulib_json_t *a = mulib_pandocast_array_(), *cells = NULL, *cell = NULL;
    const mulib_dsdom_t *c;
    size_t count = 0;
    if (!a || !mulib_pandocast_push_(a,mulib_pandocast_attr_(row,0))) goto fail;
    cells = mulib_pandocast_array_(); if (!cells) goto fail;
    for (c = mulib_xml_first(row); c; c = mulib_xml_next(c)) {
        ++count; cell = mulib_pandocast_array_();
        if (!cell || !mulib_pandocast_push_(cell,mulib_pandocast_attr_(c,0)) ||
            !mulib_pandocast_push_(cell,mulib_pandocast_tag_(mulib_pandocast_align_(c),NULL)) ||
            !mulib_pandocast_push_(cell,mulib_json_number(1)) || !mulib_pandocast_push_(cell,mulib_json_number(1)) ||
            !mulib_pandocast_push_(cell,mulib_pandocast_blocks_(c,depth+1))) goto fail;
        if (!mulib_json_append(cells,cell)) goto fail;
        cell = NULL;
    }
    if (count != columns || !mulib_json_append(a,cells)) goto fail;
    return a;
fail:
    mulib_json_destroy(cell); mulib_json_destroy(cells); mulib_json_destroy(a); return NULL;
}
static mulib_json_t *mulib_pandocast_table_(const mulib_dsdom_t *n, unsigned depth) {
    mulib_json_t *a = mulib_pandocast_array_(), *caption = NULL, *specs = NULL, *spec = NULL;
    mulib_json_t *head = NULL, *body = NULL, *bodies = NULL, *foot = NULL, *rows = NULL, *heads = NULL;
    const mulib_dsdom_t *r = mulib_xml_first(n), *c;
    size_t columns = 0, i;
    int header = 0;
    for (c = mulib_xml_first(r); c; c = mulib_xml_next(c)) ++columns;
    if (r && mulib_xml_first(r)) {
        const char *h = mulib_xml_attr(mulib_xml_first(r),"header"); header = h && !strcmp(h,"true");
    }
    if (!a || !mulib_pandocast_push_(a,mulib_pandocast_attr_(n,0))) goto fail;
    caption = mulib_pandocast_array_();
    if (!caption || !mulib_pandocast_push_(caption,mulib_json_new(MULIB_JSON_NULL)) ||
        !mulib_pandocast_push_(caption,mulib_pandocast_array_()) || !mulib_json_append(a,caption)) goto fail;
    caption = NULL; specs = mulib_pandocast_array_(); if (!specs) goto fail;
    for (i = 0,c=mulib_xml_first(r); i < columns; ++i,c=mulib_xml_next(c)) {
        spec = mulib_pandocast_array_();
        if (!spec || !mulib_pandocast_push_(spec,mulib_pandocast_tag_(mulib_pandocast_align_(c),NULL)) ||
            !mulib_pandocast_push_(spec,mulib_pandocast_tag_("ColWidthDefault",NULL)) || !mulib_json_append(specs,spec)) goto fail;
        spec = NULL;
    }
    if (!mulib_json_append(a,specs)) goto fail;
    specs = NULL; head = mulib_pandocast_array_(); heads = mulib_pandocast_array_();
    if (!head || !heads || !mulib_pandocast_push_(head,mulib_pandocast_attr_(NULL,0))) goto fail;
    if (header) { if (!mulib_pandocast_push_(heads,mulib_pandocast_row_(r,columns,depth+1))) goto fail; r = mulib_xml_next(r); }
    if (!mulib_json_append(head,heads)) goto fail;
    heads = NULL; if (!mulib_json_append(a,head)) goto fail;
    head = NULL; body = mulib_pandocast_array_(); rows = mulib_pandocast_array_(); bodies = mulib_pandocast_array_();
    if (!body || !rows || !bodies || !mulib_pandocast_push_(body,mulib_pandocast_attr_(NULL,0)) ||
        !mulib_pandocast_push_(body,mulib_json_number(0)) || !mulib_pandocast_push_(body,mulib_pandocast_array_())) goto fail;
    for (; r; r = mulib_xml_next(r)) if (!mulib_pandocast_push_(rows,mulib_pandocast_row_(r,columns,depth+1))) goto fail;
    if (!mulib_json_append(body,rows)) goto fail;
    rows = NULL; if (!mulib_json_append(bodies,body)) goto fail;
    body = NULL; if (!mulib_json_append(a,bodies)) goto fail;
    bodies = NULL; foot = mulib_pandocast_array_();
    if (!foot || !mulib_pandocast_push_(foot,mulib_pandocast_attr_(NULL,0)) ||
        !mulib_pandocast_push_(foot,mulib_pandocast_array_()) || !mulib_json_append(a,foot)) goto fail;
    return mulib_pandocast_tag_("Table",a);
fail:
    mulib_json_destroy(a); mulib_json_destroy(caption); mulib_json_destroy(specs); mulib_json_destroy(spec);
    mulib_json_destroy(head); mulib_json_destroy(heads); mulib_json_destroy(body); mulib_json_destroy(rows);
    mulib_json_destroy(bodies); mulib_json_destroy(foot); return NULL;
}
static int mulib_pandocast_is_inline_(const mulib_dsdom_t *n) {
    const char *name = mulib_xml_name(n);
    static const char *const names[] = {"strong","emphasis","underline","strike","superscript","subscript","code","link","image","span","break","kbd","index-entry"};
    size_t i;
    if (!name) return 1;
    for (i = 0; i < sizeof names / sizeof names[0]; ++i) if (!strcmp(name,names[i])) return 1;
    return mulib_xml_attr(n,"inline") && !strcmp(mulib_xml_attr(n,"inline"),"true");
}
static mulib_json_t *mulib_pandocast_block_(const mulib_dsdom_t *n, unsigned depth) {
    const char *name = mulib_xml_name(n), *ordered;
    mulib_json_t *a = NULL, *items = NULL, *item, *attrs = NULL;
    const mulib_dsdom_t *c;
    char *text;
    if (depth > 96) return NULL;
    if (!strcmp(name,"paragraph")) return mulib_pandocast_tag_("Para",mulib_pandocast_inlines_(n,depth+1));
    if (!strcmp(name,"blockquote")) return mulib_pandocast_tag_("BlockQuote",mulib_pandocast_blocks_(n,depth+1));
    if (!strcmp(name,"rule")) return mulib_pandocast_tag_("HorizontalRule",NULL);
    if (!strcmp(name,"table")) return mulib_pandocast_table_(n,depth+1);
    a = mulib_pandocast_array_(); if (!a) goto fail;
    if (!strcmp(name,"heading")) {
        if (!mulib_pandocast_push_(a,mulib_json_number(mulib_xml_attr(n,"level")[0]-'0')) ||
            !mulib_pandocast_push_(a,mulib_pandocast_attr_(n,0)) ||
            !mulib_pandocast_push_(a,mulib_pandocast_inlines_(n,depth+1))) goto fail;
        return mulib_pandocast_tag_("Header",a);
    }
    if (!strcmp(name,"code-block")) {
        if (!mulib_pandocast_push_(a,mulib_pandocast_attr_(n,0))) goto fail;
        text = mulib_dsdom_text(n,NULL); if (!text) goto fail;
        item = mulib_json_string(text); free(text);
        if (!mulib_pandocast_push_(a,item)) goto fail;
        return mulib_pandocast_tag_("CodeBlock",a);
    }
    if (!strcmp(name,"svg")) {
        text = mulib_xml_serialize(n,NULL); if (!text) goto fail;
        item = mulib_json_string(text); free(text);
        if (!mulib_pandocast_push_(a,mulib_json_string("html"))) { mulib_json_destroy(item); goto fail; }
        if (!mulib_pandocast_push_(a,item)) goto fail;
        return mulib_pandocast_tag_("RawBlock",a);
    }
    if (!strcmp(name,"list")) {
        items = mulib_pandocast_array_(); if (!items) goto fail;
        for (c = mulib_xml_first(n); c; c = mulib_xml_next(c))
            if (!mulib_pandocast_push_(items,mulib_pandocast_blocks_(c,depth+1))) goto fail;
        ordered = mulib_xml_attr(n,"ordered");
        if (!ordered || strcmp(ordered,"true")) { mulib_json_destroy(a); return mulib_pandocast_tag_("BulletList",items); }
        attrs = mulib_pandocast_array_();
        if (!attrs || !mulib_pandocast_push_(attrs,mulib_json_number(1)) ||
            !mulib_pandocast_push_(attrs,mulib_pandocast_tag_("DefaultStyle",NULL)) ||
            !mulib_pandocast_push_(attrs,mulib_pandocast_tag_("DefaultDelim",NULL)) || !mulib_json_append(a,attrs)) goto fail;
        attrs = NULL; if (!mulib_json_append(a,items)) goto fail;
        items = NULL; return mulib_pandocast_tag_("OrderedList",a);
    }
    if (!mulib_pandocast_push_(a,mulib_pandocast_attr_(n,1)) ||
        !mulib_pandocast_push_(a,mulib_pandocast_blocks_(n,depth+1))) goto fail;
    return mulib_pandocast_tag_("Div",a);
fail:
    mulib_json_destroy(attrs); mulib_json_destroy(items); mulib_json_destroy(a); return NULL;
}
static mulib_json_t *mulib_pandocast_blocks_(const mulib_dsdom_t *n, unsigned depth) {
    mulib_json_t *a = mulib_pandocast_array_(), *inlines = NULL;
    const mulib_dsdom_t *c;
    if (!a) return NULL;
    for (c = mulib_xml_first(n); c; c = mulib_xml_next(c)) {
        if (mulib_pandocast_is_inline_(c)) {
            if (!inlines) inlines = mulib_pandocast_array_();
            if (!inlines || !mulib_pandocast_inline_(inlines,c,depth+1)) goto fail;
        } else {
            if (inlines) {
                mulib_json_t *p = mulib_pandocast_tag_("Para",inlines); inlines = NULL;
                if (!mulib_pandocast_push_(a,p)) goto fail;
            }
            if (!mulib_pandocast_push_(a,mulib_pandocast_block_(c,depth+1))) goto fail;
        }
    }
    if (inlines) {
        mulib_json_t *p = mulib_pandocast_tag_("Para",inlines); inlines = NULL;
        if (!mulib_pandocast_push_(a,p)) goto fail;
    }
    return a;
fail:
    mulib_json_destroy(inlines); mulib_json_destroy(a); return NULL;
}
static int mulib_pandocast_valid_(const mulib_dsdom_t *n, size_t *nodes, size_t *bytes, unsigned depth) {
    const mulib_dsdom_t *c;
    const char *name = mulib_xml_name(n), *value = name ? name : mulib_xml_value(n);
    size_t length, i;
    if (!n || !*nodes || depth > 96 || !value ||
        (mulib_xml_type(n) != MULIB_XML_ELEMENT && mulib_xml_type(n) != MULIB_XML_TEXT)) return 0;
    --*nodes; length = strlen(value); if (length > *bytes) return 0; *bytes -= length;
    if (name && !strcmp(name,"heading")) {
        value = mulib_xml_attr(n,"level");
        if (!value || value[0] < '1' || value[0] > '6' || value[1]) return 0;
    }
    for (i = 0; i < mulib_xml_attr_count(n); ++i) {
        length = strlen(mulib_xml_attr_name(n,i)); if (length > *bytes) return 0; *bytes -= length;
        length = strlen(mulib_xml_attr_value(n,i)); if (length > *bytes) return 0; *bytes -= length;
    }
    for (c = mulib_xml_first(n); c; c = mulib_xml_next(c)) {
        const char *child = mulib_xml_name(c);
        if (name && ((!strcmp(name,"table") && (!child || strcmp(child,"row"))) ||
            (!strcmp(name,"row") && (!child || strcmp(child,"cell"))) ||
            (!strcmp(name,"list") && (!child || strcmp(child,"item"))))) return 0;
        if (!mulib_pandocast_valid_(c,nodes,bytes,depth+1)) return 0;
    }
    return 1;
}
mulib_json_t *mulib_pandocast_from_docstril(const mulib_dsdom_t *d, mulib_xml_error_t *e) {
    mulib_json_t *root = NULL, *version = NULL;
    size_t nodes = 100000, bytes = 8*1024*1024;
    if (e) { e->offset = 0; e->message = NULL; }
    if (!d || !mulib_xml_name(d) || strcmp(mulib_xml_name(d),"docstril") ||
        !mulib_xml_attr(d,"version") || strcmp(mulib_xml_attr(d,"version"),"1") ||
        !mulib_pandocast_valid_(d,&nodes,&bytes,0)) {
        if (e) e->message = "invalid/bounded DocstrIL document";
        return NULL;
    }
    root = mulib_json_new(MULIB_JSON_OBJECT); version = mulib_pandocast_array_();
    if (!root || !version || !mulib_pandocast_push_(version,mulib_json_number(1)) ||
        !mulib_pandocast_push_(version,mulib_json_number(23)) || !mulib_pandocast_push_(version,mulib_json_number(1)) ||
        !mulib_json_set(root,"pandoc-api-version",version)) goto fail;
    version = NULL;
    if (!mulib_pandocast_set_(root,"meta",mulib_json_new(MULIB_JSON_OBJECT)) ||
        !mulib_pandocast_set_(root,"blocks",mulib_pandocast_blocks_(d,0))) goto fail;
    return root;
fail:
    mulib_json_destroy(version); mulib_json_destroy(root);
    if (e) { e->offset = 0; e->message = "Pandoc conversion failed (allocation/depth/ragged table)"; }
    return NULL;
}
char *mulib_pandocast_serialize(const mulib_dsdom_t *d, size_t *length, mulib_xml_error_t *e) {
    mulib_json_t *ast = mulib_pandocast_from_docstril(d,e);
    char *s;
    if (length) *length = 0;
    if (!ast) return NULL;
    s = mulib_json_stringify(ast,0); mulib_json_destroy(ast);
    if (s && length) *length = strlen(s);
    if (!s && e) { e->offset = 0; e->message = "Pandoc JSON allocation/depth failure"; }
    return s;
}
#ifdef __cplusplus
}
#endif
#endif
