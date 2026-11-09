/** @file mulib-dyegram.h
 * @brief Native SVG sequence diagrams using labelled graph message order.
 * Define MULIB_IMPL_DYEGRAM plus GVIZZ and its dependencies.
 */
#ifndef MULIB_DYEGRAM_H
#define MULIB_DYEGRAM_H
#include "mulib-gvizz.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Render graph nodes as participants and edges as chronological messages.
 * Self messages are loops; returns owned SVG, NULL on invalid input/OOM.
 * Graph's edge-list parser supplies the diagram source language. */
mulib_dsdom_t *mulib_dyegram_svg(const mulib_gvizz_t *messages);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DYEGRAM) && !defined(MULIB_DYEGRAM_IMPLEMENTED)
#define MULIB_DYEGRAM_IMPLEMENTED
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
static mulib_dsdom_t *mulib_dyegram_child_(mulib_dsdom_t *p, const char *tag, const char *text) {
    mulib_dsdom_t *n = mulib_dsdom_element(tag,text);
    if (n && !mulib_xml_append(p,n)) { mulib_xml_destroy(n); n = NULL; }
    return n;
}
static int mulib_dyegram_num_(mulib_dsdom_t *n, const char *key, size_t v) {
    char b[32]; (void)snprintf(b,sizeof b,"%zu",v); return mulib_xml_set_attr(n,key,b);
}
mulib_dsdom_t *mulib_dyegram_svg(const mulib_gvizz_t *g) {
    mulib_dsdom_t *svg, *n;
    size_t i, x, y, to, count, height;
    char b[200];
    if (!g) return NULL;
    count = mulib_gvizz_node_count(g); height = 100 + 70 * mulib_gvizz_edge_count(g);
    svg = mulib_dsdom_element("svg",NULL);
    if (!svg) return NULL;
    (void)snprintf(b,sizeof b,"0 0 %zu %zu",count ? count * 180 + 40 : 220,height);
    if (!mulib_xml_set_attr(svg,"xmlns","http://www.w3.org/2000/svg") ||
        !mulib_xml_set_attr(svg,"viewBox",b) || !mulib_xml_set_attr(svg,"role","img") ||
        !mulib_dyegram_child_(svg,"title","Sequence diagram")) goto fail;
    for (i = 0; i < count; ++i) {
        x = i * 180 + 110;
        n = mulib_dyegram_child_(svg,"rect",NULL);
        if (!n || !mulib_dyegram_num_(n,"x",x-70) || !mulib_xml_set_attr(n,"y","10") ||
            !mulib_xml_set_attr(n,"width","140") || !mulib_xml_set_attr(n,"height","36") ||
            !mulib_xml_set_attr(n,"fill","white") || !mulib_xml_set_attr(n,"stroke","currentColor")) goto fail;
        n = mulib_dyegram_child_(svg,"text",mulib_gvizz_node_label(g,i));
        if (!n || !mulib_dyegram_num_(n,"x",x) || !mulib_xml_set_attr(n,"y","34") ||
            !mulib_xml_set_attr(n,"text-anchor","middle")) goto fail;
        n = mulib_dyegram_child_(svg,"path",NULL);
        (void)snprintf(b,sizeof b,"M %zu 46 V %zu",x,height-10);
        if (!n || !mulib_xml_set_attr(n,"d",b) || !mulib_xml_set_attr(n,"stroke","gray") ||
            !mulib_xml_set_attr(n,"stroke-dasharray","4 4")) goto fail;
    }
    for (i = 0; i < mulib_gvizz_edge_count(g); ++i) {
        const mulib_gvizz_edge_t *e = mulib_gvizz_edge_at(g,i);
        x = e->from * 180 + 110; to = e->to * 180 + 110; y = i * 70 + 80;
        if (x == to) (void)snprintf(b,sizeof b,"M %zu %zu h 55 v 25 h -55 l 7 -5 m -7 5 l 7 5",x,y);
        else (void)snprintf(b,sizeof b,"M %zu %zu H %zu l %d -5 m %d 5 l %d 5",x,y,to,to>x?-7:7,to>x?7:-7,to>x?-7:7);
        n = mulib_dyegram_child_(svg,"path",NULL);
        if (!n || !mulib_xml_set_attr(n,"d",b) || !mulib_xml_set_attr(n,"fill","none") ||
            !mulib_xml_set_attr(n,"stroke","currentColor")) goto fail;
        n = mulib_dyegram_child_(svg,"text",e->label);
        if (!n || !mulib_dyegram_num_(n,"x",(x+to)/2+5) || !mulib_dyegram_num_(n,"y",y-8) ||
            !mulib_xml_set_attr(n,"font-size","13")) goto fail;
    }
    return svg;
fail:
    mulib_xml_destroy(svg); return NULL;
}
#ifdef __cplusplus
}
#endif
#endif
