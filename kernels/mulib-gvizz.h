/** @file mulib-gvizz.h
 * @brief Small directed labelled graphs and deterministic native SVG rendering.
 * No Graphviz dependency. Define MULIB_IMPL_GVIZZ plus DSDOM/XML/STRBUF/UTF8.
 */
#ifndef MULIB_GVIZZ_H
#define MULIB_GVIZZ_H
#include "mulib-dsdom.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_gvizz mulib_gvizz_t;
typedef struct mulib_gvizz_edge { size_t from, to; const char *label; } mulib_gvizz_edge_t;
/** Independent graph, limited to 512 nodes/2048 edges and 1024-byte labels. */
mulib_gvizz_t *mulib_gvizz_create(void);
void mulib_gvizz_destroy(mulib_gvizz_t *graph);
/** Add unique ASCII identifier and copied UTF-8 label. 1/0, atomic on failure. */
int mulib_gvizz_node(mulib_gvizz_t *graph, const char *id, const char *label);
/** Add an edge between existing identifiers. Parallel edges and loops allowed. */
int mulib_gvizz_edge(mulib_gvizz_t *graph, const char *from, const char *to, const char *label);
size_t mulib_gvizz_node_count(const mulib_gvizz_t *graph);
size_t mulib_gvizz_edge_count(const mulib_gvizz_t *graph);
const char *mulib_gvizz_node_id(const mulib_gvizz_t *graph, size_t index);
const char *mulib_gvizz_node_label(const mulib_gvizz_t *graph, size_t index);
/** Borrow an edge; NULL out of range. Invalidated by edge insertion. */
const mulib_gvizz_edge_t *mulib_gvizz_edge_at(const mulib_gvizz_t *graph, size_t index);
/** Parse up to 1 MiB: `ID: label` or `ID -> ID: label`, one per line.
 * Edges implicitly declare missing nodes; declarations may relabel them.
 * Blank lines/# comments accepted. This is not DOT. NULL with diagnostic. */
mulib_gvizz_t *mulib_gvizz_parse(const char *source, size_t length, mulib_xml_error_t *error);
/** Owned SVG element, simple square-grid layout (not a crossing optimizer).
 * XML text escaping is automatic. NULL on allocation failure. */
mulib_dsdom_t *mulib_gvizz_svg(const mulib_gvizz_t *graph);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_GVIZZ) && !defined(MULIB_GVIZZ_IMPLEMENTED)
#define MULIB_GVIZZ_IMPLEMENTED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_gvizz_node_ { char *id, *label; } mulib_gvizz_node_;
struct mulib_gvizz { mulib_gvizz_node_ *nodes; mulib_gvizz_edge_t *edges; size_t count, edge_count; };
static char *mulib_gvizz_copy_(const char *s) {
    size_t n; char *p;
    if (!s || (n = strlen(s)) > 1024 || !mulib_xml_valid_text(s, n)) return NULL;
    p = (char *)malloc(n + 1); if (p) memcpy(p, s, n + 1); return p;
}
static int mulib_gvizz_id_(const char *s) {
    size_t i;
    if (!s || !*s || strlen(s) > 63) return 0;
    for (i = 0; s[i]; ++i)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z') || s[i] == '_' ||
              (i && ((s[i] >= '0' && s[i] <= '9') || s[i] == '-')))) return 0;
    return 1;
}
static size_t mulib_gvizz_find_(const mulib_gvizz_t *g, const char *id) {
    size_t i;
    if (!id) return g->count;
    for (i = 0; i < g->count; ++i) if (!strcmp(id, g->nodes[i].id)) break;
    return i;
}
mulib_gvizz_t *mulib_gvizz_create(void) { return (mulib_gvizz_t *)calloc(1, sizeof(mulib_gvizz_t)); }
void mulib_gvizz_destroy(mulib_gvizz_t *g) {
    size_t i;
    if (!g) return;
    for (i = 0; i < g->count; ++i) { free(g->nodes[i].id); free(g->nodes[i].label); }
    for (i = 0; i < g->edge_count; ++i) free((void *)g->edges[i].label);
    free(g->nodes); free(g->edges); free(g);
}
int mulib_gvizz_node(mulib_gvizz_t *g, const char *id, const char *label) {
    mulib_gvizz_node_ *p, n;
    if (!g || g->count == 512 || !mulib_gvizz_id_(id) || mulib_gvizz_find_(g, id) != g->count) return 0;
    n.id = mulib_gvizz_copy_(id); n.label = mulib_gvizz_copy_(label);
    if (!n.id || !n.label) { free(n.id); free(n.label); return 0; }
    p = (mulib_gvizz_node_ *)realloc(g->nodes, (g->count + 1) * sizeof *p);
    if (!p) { free(n.id); free(n.label); return 0; }
    g->nodes = p; g->nodes[g->count++] = n; return 1;
}
int mulib_gvizz_edge(mulib_gvizz_t *g, const char *from, const char *to, const char *label) {
    mulib_gvizz_edge_t *p, e;
    if (!g || g->edge_count == 2048) return 0;
    e.from = mulib_gvizz_find_(g, from); e.to = mulib_gvizz_find_(g, to);
    if (e.from == g->count || e.to == g->count || !(e.label = mulib_gvizz_copy_(label))) return 0;
    p = (mulib_gvizz_edge_t *)realloc(g->edges, (g->edge_count + 1) * sizeof *p);
    if (!p) { free((void *)e.label); return 0; }
    g->edges = p; g->edges[g->edge_count++] = e; return 1;
}
size_t mulib_gvizz_node_count(const mulib_gvizz_t *g) { return g ? g->count : 0; }
size_t mulib_gvizz_edge_count(const mulib_gvizz_t *g) { return g ? g->edge_count : 0; }
const char *mulib_gvizz_node_id(const mulib_gvizz_t *g, size_t i) { return g && i < g->count ? g->nodes[i].id : NULL; }
const char *mulib_gvizz_node_label(const mulib_gvizz_t *g, size_t i) { return g && i < g->count ? g->nodes[i].label : NULL; }
const mulib_gvizz_edge_t *mulib_gvizz_edge_at(const mulib_gvizz_t *g, size_t i) { return g && i < g->edge_count ? &g->edges[i] : NULL; }
static char *mulib_gvizz_trim_(char *s) {
    size_t n;
    while (*s == ' ' || *s == '\t') ++s;
    n = strlen(s); while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r')) s[--n] = 0;
    return s;
}
mulib_gvizz_t *mulib_gvizz_parse(const char *s, size_t length, mulib_xml_error_t *error) {
    mulib_gvizz_t *g = NULL;
    char *line = NULL;
    size_t at = 0, end, i;
    const char *message = "invalid graph input";
    if (error) { error->offset = 0; error->message = NULL; }
    if (length > 1024 * 1024 || !mulib_xml_valid_text(s, length)) goto fail;
    g = mulib_gvizz_create(); if (!g) { message = "allocation failure"; goto fail; }
    while (at < length) {
        char *a, *b, *label, *arrow, *colon;
        end = at; while (end < length && s[end] != '\n') ++end;
        line = (char *)malloc(end - at + 1); if (!line) { message = "allocation failure"; goto fail; }
        memcpy(line, s + at, end - at); line[end-at] = 0; a = mulib_gvizz_trim_(line);
        if (*a && *a != '#') {
            colon = strchr(a, ':'); label = colon ? mulib_gvizz_trim_(colon + 1) : NULL;
            if (colon) *colon = 0;
            arrow = strstr(a, "->");
            if (arrow) {
                *arrow = 0; b = mulib_gvizz_trim_(arrow + 2); a = mulib_gvizz_trim_(a);
                if (!mulib_gvizz_id_(a) || !mulib_gvizz_id_(b)) goto fail;
                if (mulib_gvizz_find_(g, a) == g->count && !mulib_gvizz_node(g, a, a)) goto fail;
                if (mulib_gvizz_find_(g, b) == g->count && !mulib_gvizz_node(g, b, b)) goto fail;
                if (!mulib_gvizz_edge(g, a, b, label ? label : "")) goto fail;
            } else {
                a = mulib_gvizz_trim_(a);
                if (!colon || !mulib_gvizz_id_(a)) goto fail;
                i = mulib_gvizz_find_(g, a);
                if (i == g->count) { if (!mulib_gvizz_node(g, a, label)) goto fail; }
                else {
                    char *copy = mulib_gvizz_copy_(label);
                    if (!copy) goto fail;
                    free(g->nodes[i].label); g->nodes[i].label = copy;
                }
            }
        }
        free(line); line = NULL; at = end + (end < length);
    }
    return g;
fail:
    if (error) { error->offset = at; error->message = message; }
    free(line); mulib_gvizz_destroy(g); return NULL;
}
static int mulib_gvizz_num_(mulib_dsdom_t *n, const char *key, size_t value) {
    char b[32]; (void)snprintf(b, sizeof b, "%zu", value); return mulib_xml_set_attr(n, key, b);
}
static mulib_dsdom_t *mulib_gvizz_child_(mulib_dsdom_t *p, const char *name, const char *text) {
    mulib_dsdom_t *n = mulib_dsdom_element(name, text);
    if (n && !mulib_xml_append(p, n)) { mulib_xml_destroy(n); n = NULL; }
    return n;
}
mulib_dsdom_t *mulib_gvizz_svg(const mulib_gvizz_t *g) {
    mulib_dsdom_t *svg, *n;
    size_t columns = 1, i, x, y, dx, dy, rows;
    const char base[] = "<svg xmlns=\"http://www.w3.org/2000/svg\" role=\"img\"><title>Directed graph</title></svg>";
    mulib_dsdom_t *doc;
    char path[200], view[80];
    if (!g) return NULL;
    while (columns * columns < g->count) ++columns;
    rows = g->count ? (g->count + columns - 1) / columns : 1;
    doc = mulib_xml_parse(base, sizeof base - 1, NULL); if (!doc) return NULL;
    svg = mulib_xml_detach(mulib_xml_root(doc)); mulib_xml_destroy(doc);
    (void)snprintf(view, sizeof view, "0 0 %zu %zu", columns * 180 + 40, rows * 120 + 40);
    if (!mulib_xml_set_attr(svg, "viewBox", view)) goto fail;
    for (i = 0; i < g->edge_count; ++i) {
        const mulib_gvizz_edge_t *e = &g->edges[i];
        int ex, ey, ax, ay, bx, by;
        x = e->from % columns * 180 + 110; y = e->from / columns * 120 + 70;
        dx = e->to % columns * 180 + 110; dy = e->to / columns * 120 + 70;
        if (e->from == e->to) {
            (void)snprintf(path, sizeof path, "M %zu %zu C %zu %zu %zu %zu %zu %zu", x+55,y,x+100,y-55,x+20,y-65,x,y-22);
            ex = (int)x; ey = (int)y-22;
            ax = ex-5; ay = ey-9; bx = ex+5; by = ey-9;
        } else {
            int vx = (int)dx-(int)x, vy = (int)dy-(int)y;
            int absx = vx < 0 ? -vx : vx, absy = vy < 0 ? -vy : vy;
            int scale = absx > absy ? absx : absy;
            double t = !absx ? 22.0/absy : !absy ? 65.0/absx :
                (65.0/absx < 22.0/absy ? 65.0/absx : 22.0/absy);
            int sx = (int)((double)x+vx*t), sy = (int)((double)y+vy*t);
            ex = (int)((double)dx-vx*t); ey = (int)((double)dy-vy*t);
            ax = ex-vx*9/scale-vy*5/scale; ay = ey-vy*9/scale+vx*5/scale;
            bx = ex-vx*9/scale+vy*5/scale; by = ey-vy*9/scale-vx*5/scale;
            (void)snprintf(path,sizeof path,"M %d %d L %d %d",sx,sy,ex,ey);
        }
        n = mulib_gvizz_child_(svg, "path", NULL);
        if (!n || !mulib_xml_set_attr(n,"d",path) || !mulib_xml_set_attr(n,"stroke","currentColor") ||
            !mulib_xml_set_attr(n,"fill","none")) goto fail;
        /* Explicit arrowheads avoid document-global SVG marker IDs. */
        (void)snprintf(path,sizeof path,"M %d %d L %d %d L %d %d Z",ex,ey,ax,ay,bx,by);
        n = mulib_gvizz_child_(svg,"path",NULL);
        if (!n || !mulib_xml_set_attr(n,"d",path) || !mulib_xml_set_attr(n,"fill","currentColor")) goto fail;
        n = mulib_gvizz_child_(svg, "text", e->label);
        if (!n || !mulib_gvizz_num_(n,"x",(x+dx)/2+6) || !mulib_gvizz_num_(n,"y",(y+dy)/2-6) ||
            !mulib_xml_set_attr(n,"font-size","12")) goto fail;
    }
    for (i = 0; i < g->count; ++i) {
        x = i % columns * 180 + 110; y = i / columns * 120 + 70;
        n = mulib_gvizz_child_(svg,"rect",NULL);
        if (!n || !mulib_gvizz_num_(n,"x",x-65) || !mulib_gvizz_num_(n,"y",y-22) ||
            !mulib_xml_set_attr(n,"width","130") || !mulib_xml_set_attr(n,"height","44") ||
            !mulib_xml_set_attr(n,"rx","6") || !mulib_xml_set_attr(n,"fill","white") ||
            !mulib_xml_set_attr(n,"stroke","currentColor")) goto fail;
        n = mulib_gvizz_child_(svg,"text",g->nodes[i].label);
        if (!n || !mulib_gvizz_num_(n,"x",x) || !mulib_gvizz_num_(n,"y",y+5) ||
            !mulib_xml_set_attr(n,"text-anchor","middle") || !mulib_xml_set_attr(n,"font-size","14")) goto fail;
    }
    return svg;
fail:
    mulib_xml_destroy(svg); return NULL;
}
#ifdef __cplusplus
}
#endif
#endif
