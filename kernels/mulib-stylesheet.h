/** @file mulib-stylesheet.h
 * @brief Typed layout styles, inheritance and CSS specificity/importance cascade.
 * Uses mulib-css selectors and XML node identity. Define MULIB_IMPL_STYLESHEET
 * plus CSS/XML/STRBUF/UTF8 switches. Unsupported properties/values fail.
 */
#ifndef MULIB_STYLESHEET_H
#define MULIB_STYLESHEET_H
#include "mulib-css.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_stylesheet mulib_stylesheet_t;
/** @brief Pixel lengths; width/height -1 mean auto. line_height is a multiplier.
 * Colors are 0xRRGGBBAA. align=0 left,1 center,2 right; hidden=1 display:none.
 * Font family is an unquoted UTF-8 string of at most 127 bytes. */
typedef struct mulib_stylesheet_style {
    double font_size, line_height, width, height, margin, padding;
    uint32_t color, background;
    int align, hidden;
    char font_family[128];
} mulib_stylesheet_style_t;
/** @brief Defaults: 16px sans, 1.2 line height, black, transparent background,
 * auto size, zero margin/padding, left aligned, visible. */
mulib_stylesheet_style_t mulib_stylesheet_default(void);
/** @brief Parse and validate all rules. NULL on syntax/unsupported values/OOM;
 * optional error message is static (semantic errors use offset zero). */
mulib_stylesheet_t *mulib_stylesheet_parse(const char *source, size_t length, mulib_xml_error_t *error);
/** @brief Release sheet. NULL harmless. */
void mulib_stylesheet_destroy(mulib_stylesheet_t *sheet);
/** @brief Resolve author rules and the node's style attribute. Inherit color,
 * font-family, font-size, line-height and text-align from parent (or defaults).
 * Importance precedes inline specificity, selector specificity, source order.
 * 0/-1, output unchanged on failure; sheet may be NULL. */
int mulib_stylesheet_resolve(const mulib_stylesheet_t *sheet, const mulib_xml_node_t *node,
                            const mulib_stylesheet_style_t *parent,
                            mulib_stylesheet_style_t *style, mulib_xml_error_t *error);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_STYLESHEET) && !defined(MULIB_STYLESHEET_IMPLEMENTED)
#define MULIB_STYLESHEET_IMPLEMENTED
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_stylesheet { mulib_css_sheet_t *css; };
typedef struct mulib_stylesheet_rank_ { size_t ids, classes, types; int important, inline_style, set; } mulib_stylesheet_rank_;
mulib_stylesheet_style_t mulib_stylesheet_default(void) {
    mulib_stylesheet_style_t s = {16, 1.2, -1, -1, 0, 0, UINT32_C(0x000000ff), 0, 0, 0, "sans"};
    return s;
}
static int mulib_stylesheet_error_(mulib_xml_error_t *e, const char *message) {
    if (e) { e->offset = 0; e->message = message; }
    return -1;
}
static int mulib_stylesheet_equal_(const char *a, const char *b) {
    while (*a && *b) {
        unsigned char x = (unsigned char)*a++, y = (unsigned char)*b++;
        if (x >= 'A' && x <= 'Z') x = (unsigned char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (unsigned char)(y + 32);
        if (x != y) return 0;
    }
    return *a == *b;
}
static int mulib_stylesheet_number_(const char *s, int pixels, double *out) {
    double value = 0, fraction = 0.1;
    int digits = 0;
    while (*s >= '0' && *s <= '9') {
        digits = 1; value = value * 10 + (*s++ - '0');
        if (value > 1000000) return -1;
    }
    if (*s == '.') {
        ++s;
        if (*s < '0' || *s > '9') return -1;
        while (*s >= '0' && *s <= '9') { digits = 1; value += (*s++ - '0') * fraction; fraction *= 0.1; }
    }
    if (!digits || value > 1000000 || (*s && !(pixels && mulib_stylesheet_equal_(s, "px")))) return -1;
    *out = value; return 0;
}
static int mulib_stylesheet_color_(const char *s, uint32_t *out) {
    size_t n, i;
    uint32_t value = 0;
    if (mulib_stylesheet_equal_(s, "transparent")) { *out = 0; return 0; }
    if (mulib_stylesheet_equal_(s, "black")) { *out = 255; return 0; }
    if (mulib_stylesheet_equal_(s, "white")) { *out = UINT32_MAX; return 0; }
    if (*s++ != '#') return -1;
    n = strlen(s);
    if (n != 3 && n != 4 && n != 6 && n != 8) return -1;
    for (i = 0; i < n; ++i) {
        unsigned d;
        if (s[i] >= '0' && s[i] <= '9') d = (unsigned)(s[i] - '0');
        else if (s[i] >= 'a' && s[i] <= 'f') d = (unsigned)(s[i] - 'a' + 10);
        else if (s[i] >= 'A' && s[i] <= 'F') d = (unsigned)(s[i] - 'A' + 10);
        else return -1;
        value = n <= 4 ? (value << 8) | (d * 17) : (value << 4) | d;
    }
    if (n == 3 || n == 6) value = (value << 8) | 255;
    *out = value; return 0;
}
static int mulib_stylesheet_property_(mulib_stylesheet_style_t *s, const char *name, const char *v) {
    static const char *names[] = {"font-size", "line-height", "width", "height", "margin", "padding",
                                 "color", "background-color", "text-align", "display", "font-family"};
    int p;
    double d;
    uint32_t color;
    for (p = 0; p < 11; ++p) if (mulib_stylesheet_equal_(name, names[p])) break;
    if (p == 11) return -1;
    if (p <= 5) {
        if ((p == 2 || p == 3) && mulib_stylesheet_equal_(v, "auto")) d = -1;
        else if (mulib_stylesheet_number_(v, p != 1, &d) || ((p == 0 || p == 1) && d <= 0)) return -1;
        switch (p) {
        case 0: s->font_size = d; break; case 1: s->line_height = d; break;
        case 2: s->width = d; break; case 3: s->height = d; break;
        case 4: s->margin = d; break; default: s->padding = d; break;
        }
    } else if (p == 6 || p == 7) {
        if (mulib_stylesheet_color_(v, &color)) return -1;
        if (p == 6) s->color = color; else s->background = color;
    } else if (p == 8) {
        if (mulib_stylesheet_equal_(v, "left")) s->align = 0;
        else if (mulib_stylesheet_equal_(v, "center")) s->align = 1;
        else if (mulib_stylesheet_equal_(v, "right")) s->align = 2;
        else return -1;
    } else if (p == 9) {
        if (mulib_stylesheet_equal_(v, "none")) s->hidden = 1;
        else if (mulib_stylesheet_equal_(v, "block")) s->hidden = 0;
        else return -1;
    } else {
        size_t n = strlen(v);
        if (!n || n >= sizeof s->font_family || strpbrk(v, "\"',;()") ||
            !mulib_utf8_validate((const unsigned char *)v, n)) return -1;
        memcpy(s->font_family, v, n + 1);
    }
    return p;
}
static int mulib_stylesheet_better_(mulib_stylesheet_rank_ a, mulib_stylesheet_rank_ b) {
    if (!b.set) return 1;
    if (a.important != b.important) return a.important > b.important;
    if (a.inline_style != b.inline_style) return a.inline_style > b.inline_style;
    if (a.ids != b.ids) return a.ids > b.ids;
    if (a.classes != b.classes) return a.classes > b.classes;
    return a.types >= b.types;
}
/* CSS parser validates syntax. Split groups outside attributes/quotes and count
 * specificity only for matching branches, including attributes as class rank. */
static int mulib_stylesheet_selector_rank_(const mulib_xml_node_t *node, const char *selector, mulib_stylesheet_rank_ *out) {
    mulib_css_specificity_t rank = {0};
    mulib_stylesheet_rank_ result = {0};
    int matched = mulib_css_specificity(node, selector, &rank);
    if (matched < 0) return -1;
    result.ids = rank.ids; result.classes = rank.classes; result.types = rank.types;
    result.set = matched; *out = result; return matched;
}
void mulib_stylesheet_destroy(mulib_stylesheet_t *s) {
    if (s) { mulib_css_destroy(s->css); free(s); }
}
mulib_stylesheet_t *mulib_stylesheet_parse(const char *source, size_t n, mulib_xml_error_t *e) {
    mulib_stylesheet_t *s = (mulib_stylesheet_t *)calloc(1, sizeof *s);
    size_t i, j;
    if (e) { e->offset = 0; e->message = NULL; }
    if (!s) { (void)mulib_stylesheet_error_(e, "allocation failure"); return NULL; }
    s->css = mulib_css_parse(source, n, e);
    if (!s->css) { free(s); return NULL; }
    for (i = 0; i < mulib_css_rule_count(s->css); ++i) {
        const mulib_css_style_t *d = mulib_css_declarations(s->css, i);
        mulib_stylesheet_style_t test = mulib_stylesheet_default();
        if (mulib_css_matches(NULL, mulib_css_selector(s->css, i)) < 0) goto invalid;
        for (j = 0; j < mulib_css_count(d); ++j)
            if (mulib_stylesheet_property_(&test, mulib_css_name(d, j), mulib_css_value(d, j)) < 0) goto invalid;
    }
    return s;
invalid:
    (void)mulib_stylesheet_error_(e, "unsupported selector, property or value");
    mulib_stylesheet_destroy(s); return NULL;
}
static int mulib_stylesheet_apply_(const mulib_css_style_t *d, mulib_stylesheet_rank_ rank,
                                   mulib_stylesheet_rank_ *ranks, mulib_stylesheet_style_t *s) {
    size_t j;
    for (j = 0; j < mulib_css_count(d); ++j) {
        mulib_stylesheet_style_t test = *s;
        int p = mulib_stylesheet_property_(&test, mulib_css_name(d, j), mulib_css_value(d, j));
        if (p < 0) return -1;
        rank.important = mulib_css_important(d, j); rank.set = 1;
        if (mulib_stylesheet_better_(rank, ranks[p])) { *s = test; ranks[p] = rank; }
    }
    return 0;
}
int mulib_stylesheet_resolve(const mulib_stylesheet_t *sheet, const mulib_xml_node_t *node,
                            const mulib_stylesheet_style_t *parent, mulib_stylesheet_style_t *out, mulib_xml_error_t *e) {
    mulib_stylesheet_style_t s = mulib_stylesheet_default();
    mulib_stylesheet_rank_ ranks[11] = {{0}};
    mulib_css_style_t *inline_style;
    const char *text;
    size_t i;
    if (e) { e->offset = 0; e->message = NULL; }
    if (!node || !out || mulib_xml_type(node) != MULIB_XML_ELEMENT) return mulib_stylesheet_error_(e, "invalid element/output");
    if (parent) {
        const char *end = (const char *)memchr(parent->font_family, 0, sizeof parent->font_family);
        if (!isfinite(parent->font_size) || parent->font_size <= 0 || parent->font_size > 1e6 ||
            !isfinite(parent->line_height) || parent->line_height <= 0 || parent->line_height > 1e6 ||
            parent->align < 0 || parent->align > 2 || !end || end == parent->font_family ||
            !mulib_utf8_validate((const unsigned char *)parent->font_family, (size_t)(end-parent->font_family)))
            return mulib_stylesheet_error_(e, "invalid inherited style");
        s.font_size = parent->font_size; s.line_height = parent->line_height; s.color = parent->color;
        s.align = parent->align; memcpy(s.font_family, parent->font_family, sizeof s.font_family);
    }
    if (sheet) for (i = 0; i < mulib_css_rule_count(sheet->css); ++i) {
        mulib_stylesheet_rank_ rank;
        int match = mulib_stylesheet_selector_rank_(node, mulib_css_selector(sheet->css, i), &rank);
        if (match < 0) return mulib_stylesheet_error_(e, "selector matching failed");
        if (match && mulib_stylesheet_apply_(mulib_css_declarations(sheet->css, i), rank, ranks, &s))
            return mulib_stylesheet_error_(e, "unsupported property or value");
    }
    text = mulib_xml_attr(node, "style");
    if (text) {
        mulib_stylesheet_rank_ rank = {0, 0, 0, 0, 1, 1};
        int status;
        inline_style = mulib_css_parse_declarations(text, strlen(text), e);
        if (!inline_style) return -1;
        status = mulib_stylesheet_apply_(inline_style, rank, ranks, &s);
        mulib_css_style_destroy(inline_style);
        if (status) return mulib_stylesheet_error_(e, "unsupported inline property or value");
    }
    *out = s; return 0;
}
#ifdef __cplusplus
}
#endif
#endif
