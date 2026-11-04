/** @file mulib-xll.h
 * @brief X Layout Language: owned text-layout programs, parser and direct C emitter.
 * Define MULIB_IMPL_XLL and the layout/JSON dependency switches in one TU.
 * Generated C includes mulib-layout.h, with no XLL runtime dependency.
 */
#ifndef MULIB_XLL_H
#define MULIB_XLL_H
#include "mulib-layout.h"
#include "mulib-json.h"
#include "mulib-strbuf.h"
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_xll mulib_xll_t;
/** @brief Byte diagnostic; line/column are 1-based byte coordinates. Message is
 * static. Semantic/API failures use offset 0, line/column 1. */
typedef struct mulib_xll_error { size_t offset, line, column; const char *message; } mulib_xll_error_t;
/** @brief Layout declaration. Inputs are borrowed, add/set copy all strings.
 * Text is length-delimited strict UTF-8 (NUL allowed). Other strings are NUL
 * terminated. Font size is pixels; x/y are renderer origins, not text metrics.
 * All fallback faces supplied to create/compute must use round(font_size*64).
 * Names: ASCII letter followed by letters/digits/underscore, at most 63 bytes. */
typedef struct mulib_xll_block {
    const char *name, *text, *font_family, *language;
    size_t length;
    double font_size, x, y;
    mulib_layout_options_t options;
} mulib_xll_block_t;
/** @brief Defaults: empty text, sans 16px, und, origin (0,0), layout defaults.
 * name remains NULL and must be supplied before add/set. */
mulib_xll_block_t mulib_xll_block_default(void);
/** @brief Allocate an empty owned program. NULL on OOM. */
mulib_xll_t *mulib_xll_new(void);
/** @brief Free program and all declarations; NULL harmless. */
void mulib_xll_destroy(mulib_xll_t *program);
/** @brief Append a unique named block transactionally. 0/-1. At most 1024 blocks
 * and 16 MiB total text. Metadata <=255 bytes; language <=63 bytes. Finite
 * geometry is bounded to +/-1e6; size/spacing positive, width nonnegative. */
int mulib_xll_add(mulib_xll_t *program, const mulib_xll_block_t *block, mulib_xll_error_t *error);
/** @brief Replace an existing block with the same validation/copying rules. 0/-1.
 * A borrowed get() view may be used as input, including its own text. */
int mulib_xll_set(mulib_xll_t *program, size_t index, const mulib_xll_block_t *block, mulib_xll_error_t *error);
/** @brief Number of declarations (zero for NULL). */
size_t mulib_xll_count(const mulib_xll_t *program);
/** @brief Borrow a declaration, or NULL out of range. Invalidated by mutation. */
const mulib_xll_block_t *mulib_xll_get(const mulib_xll_t *program, size_t index);
/** @brief Find exact case-sensitive name; SIZE_MAX if absent/invalid. */
size_t mulib_xll_find(const mulib_xll_t *program, const char *name);
/** @brief Parse xll 1 { layout name { property value; ... } ... }.
 * Supports JSON strings/numbers, // and block comments; rejects duplicate and
 * unknown properties, malformed UTF-8, trailing input and source >128 MiB.
 * Missing properties take defaults. Empty source is invalid; xll 1 {} is valid.
 * Returns owned program or NULL with diagnostic. No I/O or font loading. */
mulib_xll_t *mulib_xll_parse(const char *source, size_t length, mulib_xll_error_t *error);
/** @brief Canonical XLL source, malloc-owned, or NULL. Comments are not retained.
 * length optional, set to zero on failure. Locale-independent numeric spelling. */
char *mulib_xll_serialize(const mulib_xll_t *program, size_t *length);
/** @brief Create/configure a layout from a declaration and borrowed font set.
 * Caller resolves font_family/fallback; scale is checked for every supplied face.
 * Declaration language/base override the set. NULL on error. Caller destroys. */
mulib_layout_t *mulib_xll_create_layout(const mulib_xll_t *program, size_t index,
    const mulib_pglayout_fontset_t *fonts, mulib_xml_error_t *error);
/** @brief Compute one declaration directly; same empty-output/ownership and
 * transactional 0/-1 contract as layout_compute. Origin remains separate. */
int mulib_xll_compute(const mulib_xll_t *program, size_t index, const mulib_pglayout_fontset_t *fonts,
    mulib_layout_result_t *result, mulib_xml_error_t *error);
/** @brief Emit malloc-owned C11 source. prefix follows the block-name grammar;
 * symbols start mulib_xll_<prefix>_. Exports count/get/create/compute, embeds
 * byte arrays (including NUL/non-ASCII), and calls layout APIs directly.
 * No dependency implementation switches or main() are emitted. NULL on error.
 * Optional length excludes terminator, zero on failure. */
char *mulib_xll_emit_c(const mulib_xll_t *program, const char *prefix, size_t *length, mulib_xll_error_t *error);
/** @brief Generate fully before writing to a borrowed stream. 0/-1; never closes
 * or flushes the stream. I/O errors can leave a partial file; caller handles
 * fclose/fflush errors. Compile/validation failure writes nothing. */
int mulib_xll_write_c(const mulib_xll_t *program, const char *prefix, FILE *stream, mulib_xll_error_t *error);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_XLL) && !defined(MULIB_XLL_IMPLEMENTED)
#define MULIB_XLL_IMPLEMENTED
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_XLL_BYTES_ (16u*1024u*1024u)
#define MULIB_XLL_SOURCE_BYTES_ (128u*1024u*1024u)
struct mulib_xll { mulib_xll_block_t *blocks; size_t count, bytes; };
static const char *const mulib_xll_wrap_[] = {"none", "word", "grapheme", "word_grapheme"};
static const char *const mulib_xll_align_[] = {"start", "end", "center", "left", "right"};
static const char *const mulib_xll_base_[] = {"auto_rtl", "auto", "ltr", "rtl"};
static void mulib_xll_error_(mulib_xll_error_t *e, const char *message) {
    if (e) { e->offset = 0; e->line = 1; e->column = 1; e->message = message; }
}
static int mulib_xll_letter_(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static int mulib_xll_ident_(const char *s) {
    size_t i;
    if (!s || !mulib_xll_letter_(*s)) return 0;
    for (i = 1; s[i]; ++i)
        if (i >= 63 || !(mulib_xll_letter_(s[i]) || (s[i] >= '0' && s[i] <= '9') || s[i] == '_')) return 0;
    return 1;
}
static char *mulib_xll_copy_(const char *s, size_t n) {
    char *p;
    if (n == SIZE_MAX || (!s && n)) return NULL;
    p = (char *)malloc(n+1);
    if (p) { if (n) memcpy(p, s, n); p[n] = 0; }
    return p;
}
static void mulib_xll_free_block_(mulib_xll_block_t *b) {
    free((void *)b->name); free((void *)b->text); free((void *)b->font_family); free((void *)b->language);
    memset(b, 0, sizeof *b);
}
mulib_xll_block_t mulib_xll_block_default(void) {
    mulib_xll_block_t b;
    memset(&b, 0, sizeof b); b.text = ""; b.font_family = "sans"; b.language = "und";
    b.font_size = 16; b.options = mulib_layout_default(); return b;
}
mulib_xll_t *mulib_xll_new(void) { return (mulib_xll_t *)calloc(1, sizeof(mulib_xll_t)); }
void mulib_xll_destroy(mulib_xll_t *p) {
    size_t i;
    if (!p) return;
    for (i = 0; i < p->count; ++i) mulib_xll_free_block_(&p->blocks[i]);
    free(p->blocks); free(p);
}
size_t mulib_xll_count(const mulib_xll_t *p) { return p ? p->count : 0; }
const mulib_xll_block_t *mulib_xll_get(const mulib_xll_t *p, size_t i) { return p && i < p->count ? &p->blocks[i] : NULL; }
size_t mulib_xll_find(const mulib_xll_t *p, const char *name) {
    size_t i;
    if (p && name) for (i = 0; i < p->count; ++i) if (!strcmp(name, p->blocks[i].name)) return i;
    return SIZE_MAX;
}
static int mulib_xll_metadata_(const char *s, size_t limit) {
    size_t n, i;
    if (!s || !(n = strlen(s)) || n > limit || !mulib_utf8_validate((const unsigned char *)s, n)) return 0;
    for (i = 0; i < n; ++i) if ((unsigned char)s[i] < 32 || (unsigned char)s[i] == 127) return 0;
    return 1;
}
static int mulib_xll_store_(mulib_xll_t *p, size_t i, const mulib_xll_block_t *b, mulib_xll_error_t *e) {
    mulib_xll_block_t copy;
    mulib_xll_block_t *grown;
    size_t found, old, total;
    mulib_xll_error_(e, NULL);
    if (!p || !b || i > p->count || (i == p->count && (p->count == 1024 || p->count >= SIZE_MAX/sizeof *p->blocks)) ||
        !mulib_xll_ident_(b->name) || b->length > MULIB_XLL_BYTES_ ||
        !mulib_utf8_validate((const unsigned char *)b->text, b->length) ||
        !mulib_xll_metadata_(b->font_family, 255) || !mulib_xll_metadata_(b->language, 63) ||
        !isfinite(b->font_size) || b->font_size < 1.0/64 || b->font_size > 1000000 ||
        !isfinite(b->x) || fabs(b->x) > 1000000 || !isfinite(b->y) || fabs(b->y) > 1000000 ||
        !isfinite(b->options.width) || b->options.width < 0 || b->options.width > 1000000 ||
        !isfinite(b->options.spacing) || b->options.spacing <= 0 || b->options.spacing > 1000000 ||
        b->options.wrap < MULIB_LAYOUT_WRAP_NONE || b->options.wrap > MULIB_LAYOUT_WRAP_WORD_GRAPHEME ||
        b->options.alignment < MULIB_LAYOUT_START || b->options.alignment > MULIB_LAYOUT_RIGHT ||
        b->options.base < -2 || b->options.base > 1) {
        mulib_xll_error_(e, "invalid XLL declaration or limit exceeded"); return -1;
    }
    found = mulib_xll_find(p, b->name);
    old = i < p->count ? p->blocks[i].length : 0;
    if ((found != SIZE_MAX && found != i) || b->length > MULIB_XLL_BYTES_-(p->bytes-old)) {
        mulib_xll_error_(e, "duplicate layout name or text budget exceeded"); return -1;
    }
    total = p->bytes-old+b->length;
    copy = *b;
    copy.name = mulib_xll_copy_(b->name, strlen(b->name));
    copy.text = mulib_xll_copy_(b->text, b->length);
    copy.font_family = mulib_xll_copy_(b->font_family, strlen(b->font_family));
    copy.language = mulib_xll_copy_(b->language, strlen(b->language));
    if (!copy.name || !copy.text || !copy.font_family || !copy.language) goto oom;
    if (i == p->count) {
        grown = (mulib_xll_block_t *)realloc(p->blocks, (p->count+1)*sizeof *grown);
        if (!grown) goto oom;
        p->blocks = grown; ++p->count;
    } else mulib_xll_free_block_(&p->blocks[i]);
    p->blocks[i] = copy; p->bytes = total; return 0;
oom:
    mulib_xll_free_block_(&copy); mulib_xll_error_(e, "allocation failure"); return -1;
}
int mulib_xll_add(mulib_xll_t *p, const mulib_xll_block_t *b, mulib_xll_error_t *e) {
    return mulib_xll_store_(p, p ? p->count : 0, b, e);
}
int mulib_xll_set(mulib_xll_t *p, size_t i, const mulib_xll_block_t *b, mulib_xll_error_t *e) {
    if (!p || i >= p->count) { mulib_xll_error_(e, "layout index out of range"); return -1; }
    return mulib_xll_store_(p, i, b, e);
}
typedef struct mulib_xll_parser_ {
    const char *s; size_t n, p, start, end; int token;
    mulib_json_t *value; mulib_xll_error_t *error;
} mulib_xll_parser_;
static int mulib_xll_fail_(mulib_xll_parser_ *p, size_t at, const char *message) {
    if (p->error) {
        size_t i;
        mulib_xll_error_(p->error, message); p->error->offset = at;
        for (i = 0; i < at; ++i) {
            if (p->s[i] == '\r') {
                ++p->error->line; p->error->column = 1;
                if (i+1 < at && p->s[i+1] == '\n') ++i;
            } else if (p->s[i] == '\n') { ++p->error->line; p->error->column = 1; }
            else ++p->error->column;
        }
    }
    return -1;
}
static int mulib_xll_space_(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
/* Tokens: punctuation as ASCII, 256 identifier, 257 string, 258 number, 0 EOF. */
static int mulib_xll_next_(mulib_xll_parser_ *p) {
    size_t start;
    char c;
    mulib_json_error_t je;
    mulib_json_destroy(p->value); p->value = NULL;
    for (;;) {
        while (p->p < p->n && mulib_xll_space_(p->s[p->p])) ++p->p;
        if (p->n-p->p < 2 || p->s[p->p] != '/') break;
        if (p->s[p->p+1] == '/') {
            p->p += 2; while (p->p < p->n && p->s[p->p] != '\n' && p->s[p->p] != '\r') ++p->p;
        } else if (p->s[p->p+1] == '*') {
            start = p->p; p->p += 2;
            while (p->n-p->p >= 2 && !(p->s[p->p] == '*' && p->s[p->p+1] == '/')) ++p->p;
            if (p->n-p->p < 2) return mulib_xll_fail_(p, start, "unterminated comment");
            p->p += 2;
        } else break;
    }
    p->start = p->p;
    if (p->p == p->n) { p->end = p->p; p->token = 0; return 0; }
    c = p->s[p->p++];
    if (c == '{' || c == '}' || c == ';') p->token = c;
    else if (mulib_xll_letter_(c)) {
        while (p->p < p->n && (mulib_xll_letter_(p->s[p->p]) ||
            (p->s[p->p] >= '0' && p->s[p->p] <= '9') || p->s[p->p] == '_')) ++p->p;
        if (p->p-p->start > 63) return mulib_xll_fail_(p, p->start, "identifier exceeds 63 bytes");
        p->token = 256;
    } else if (c == '"' || c == '-' || (c >= '0' && c <= '9')) {
        if (c == '"') {
            int closed = 0;
            while (p->p < p->n) {
                char q = p->s[p->p++];
                if (q == '"') { closed = 1; break; }
                if (q == '\\' && p->p < p->n) ++p->p;
            }
            if (!closed) return mulib_xll_fail_(p, p->start, "unterminated string");
        } else {
            while (p->p < p->n) {
                char q = p->s[p->p];
                if (!((q >= '0' && q <= '9') || q == '.' || q == 'e' || q == 'E' || q == '+' || q == '-')) break;
                ++p->p;
            }
            if (p->p-p->start > 128) return mulib_xll_fail_(p, p->start, "number exceeds 128 bytes");
        }
        p->value = mulib_json_parse(p->s+p->start, p->p-p->start, &je);
        if (!p->value) return mulib_xll_fail_(p, p->start+je.offset, je.message ? je.message : "invalid literal");
        p->token = c == '"' ? 257 : 258;
    } else return mulib_xll_fail_(p, p->start, "unexpected character");
    p->end = p->p; return 0;
}
static int mulib_xll_word_(const mulib_xll_parser_ *p, const char *s) {
    return p->token == 256 && strlen(s) == p->end-p->start && !memcmp(p->s+p->start, s, p->end-p->start);
}
static int mulib_xll_expect_(mulib_xll_parser_ *p, int token, const char *message) {
    if (p->token != token) return mulib_xll_fail_(p, p->start, message);
    return mulib_xll_next_(p);
}
static int mulib_xll_string_(mulib_xll_parser_ *p, char **out, size_t *length, int metadata) {
    const char *s; size_t n;
    if (p->token != 257) return mulib_xll_fail_(p, p->start, "expected quoted string");
    s = mulib_json_text(p->value, &n);
    if (n > (metadata ? 255 : MULIB_XLL_BYTES_)) return mulib_xll_fail_(p, p->start, "string exceeds size limit");
    if (metadata && memchr(s, 0, n)) return mulib_xll_fail_(p, p->start, "NUL in metadata");
    *out = mulib_xll_copy_(s, n);
    if (!*out) return mulib_xll_fail_(p, p->start, "allocation failure");
    if (length) *length = n;
    return mulib_xll_next_(p);
}
static int mulib_xll_number_(mulib_xll_parser_ *p, double *out) {
    if (p->token != 258) return mulib_xll_fail_(p, p->start, "expected JSON number");
    *out = mulib_json_numeric(p->value); return mulib_xll_next_(p);
}
static int mulib_xll_enum_(mulib_xll_parser_ *p, const char *const *names, size_t count, int *out) {
    size_t i;
    for (i = 0; i < count; ++i) if (mulib_xll_word_(p, names[i])) break;
    if (i == count) return mulib_xll_fail_(p, p->start, "unknown enumeration value");
    *out = (int)i; return mulib_xll_next_(p);
}
mulib_xll_t *mulib_xll_parse(const char *s, size_t n, mulib_xll_error_t *error) {
    mulib_xll_parser_ p;
    mulib_xll_t *doc = NULL;
    char *text = NULL, *family = NULL, *language = NULL;
    memset(&p, 0, sizeof p); p.s = s; p.n = n; p.error = error;
    mulib_xll_error_(error, NULL);
    if (n > MULIB_XLL_SOURCE_BYTES_ || !mulib_utf8_validate((const unsigned char *)s, n) || (n && memchr(s, 0, n))) {
        mulib_xll_error_(error, "source exceeds limit, contains NUL or invalid UTF-8"); return NULL;
    }
    if (mulib_xll_next_(&p)) goto fail;
    if (!mulib_xll_word_(&p, "xll")) { mulib_xll_fail_(&p, p.start, "expected xll version header"); goto fail; }
    if (mulib_xll_next_(&p)) goto fail;
    if (p.token != 258 || p.end-p.start != 1 || s[p.start] != '1') {
        mulib_xll_fail_(&p, p.start, "unsupported XLL version (expected 1)"); goto fail;
    }
    if (mulib_xll_next_(&p) || mulib_xll_expect_(&p, '{', "expected '{'")) goto fail;
    doc = mulib_xll_new();
    if (!doc) { mulib_xll_fail_(&p, p.start, "allocation failure"); goto fail; }
    while (p.token != '}') {
        mulib_xll_block_t b = mulib_xll_block_default();
        char name[64]; unsigned seen = 0; size_t begin = p.start;
        if (!mulib_xll_word_(&p, "layout")) { mulib_xll_fail_(&p, p.start, "expected layout declaration"); goto fail; }
        if (mulib_xll_next_(&p)) goto fail;
        if (p.token != 256) { mulib_xll_fail_(&p, p.start, "expected layout name"); goto fail; }
        memcpy(name, s+p.start, p.end-p.start); name[p.end-p.start] = 0; b.name = name;
        if (mulib_xll_next_(&p) || mulib_xll_expect_(&p, '{', "expected '{'")) goto fail;
        while (p.token != '}') {
            static const char *const properties[] = {"text", "font", "language", "width", "spacing", "origin", "wrap", "align", "direction"};
            size_t k; int v;
            for (k = 0; k < 9; ++k) if (mulib_xll_word_(&p, properties[k])) break;
            if (k == 9 || (seen & (1u << k))) {
                mulib_xll_fail_(&p, p.start, k == 9 ? "unknown layout property" : "duplicate layout property"); goto fail;
            }
            seen |= 1u << k;
            if (mulib_xll_next_(&p)) goto fail;
            switch (k) {
            case 0: if (mulib_xll_string_(&p, &text, &b.length, 0)) goto fail; b.text = text; break;
            case 1:
                if (mulib_xll_string_(&p, &family, NULL, 1) || mulib_xll_number_(&p, &b.font_size)) goto fail;
                b.font_family = family; break;
            case 2: if (mulib_xll_string_(&p, &language, NULL, 1)) goto fail; b.language = language; break;
            case 3: if (mulib_xll_number_(&p, &b.options.width)) goto fail; break;
            case 4: if (mulib_xll_number_(&p, &b.options.spacing)) goto fail; break;
            case 5: if (mulib_xll_number_(&p, &b.x) || mulib_xll_number_(&p, &b.y)) goto fail; break;
            case 6:
                if (mulib_xll_enum_(&p, mulib_xll_wrap_, 4, &v)) goto fail;
                b.options.wrap = (mulib_layout_wrap_t)v; break;
            case 7:
                if (mulib_xll_enum_(&p, mulib_xll_align_, 5, &v)) goto fail;
                b.options.alignment = (mulib_layout_alignment_t)v; break;
            default:
                if (mulib_xll_enum_(&p, mulib_xll_base_, 4, &v)) goto fail;
                b.options.base = v-2; break;
            }
            if (mulib_xll_expect_(&p, ';', "expected ';' after property")) goto fail;
        }
        if (mulib_xll_add(doc, &b, error)) {
            mulib_xll_fail_(&p, begin, error && error->message ? error->message : "invalid layout declaration"); goto fail;
        }
        free(text); free(family); free(language); text = family = language = NULL;
        if (mulib_xll_next_(&p)) goto fail;
    }
    if (mulib_xll_next_(&p)) goto fail;
    if (p.token) { mulib_xll_fail_(&p, p.start, "trailing input"); goto fail; }
    mulib_json_destroy(p.value); return doc;
fail:
    free(text); free(family); free(language); mulib_json_destroy(p.value); mulib_xll_destroy(doc); return NULL;
}
/* JSON's number formatter normalizes the locale decimal separator and retains
 * round-trip double precision; both XLL and emitted C use the same spelling. */
static int mulib_xll_json_(mulib_strbuf_t *out, mulib_json_t *value) {
    char *s; int ok;
    if (!value) return 0;
    s = mulib_json_stringify(value, 0); mulib_json_destroy(value);
    if (!s) return 0;
    ok = mulib_strbuf_append(out, s); free(s); return ok;
}
static int mulib_xll_double_(mulib_strbuf_t *out, double x) { return mulib_xll_json_(out, mulib_json_number(x)); }
static int mulib_xll_c_double_(mulib_strbuf_t *out, double x) {
    size_t start = out->length;
    if (!mulib_xll_double_(out, x)) return 0;
    /* A decimal floating literal retains negative zero; the integer -0 does not. */
    return strpbrk(out->data+start, ".eE") != NULL || mulib_strbuf_append(out, ".0");
}
static int mulib_xll_size_(mulib_strbuf_t *out, size_t x) {
    char s[3*sizeof(size_t)+1]; int n = snprintf(s, sizeof s, "%zu", x);
    return n > 0 && (size_t)n < sizeof s && mulib_strbuf_append_n(out, s, (size_t)n);
}
char *mulib_xll_serialize(const mulib_xll_t *p, size_t *length) {
    mulib_strbuf_t out = {0}; size_t i;
    if (length) *length = 0;
    if (!p || !mulib_strbuf_append(&out, "xll 1 {\n")) goto fail;
    for (i = 0; i < p->count; ++i) {
        const mulib_xll_block_t *b = &p->blocks[i];
        if (!mulib_strbuf_append(&out, "  layout ") || !mulib_strbuf_append(&out, b->name) ||
            !mulib_strbuf_append(&out, " {\n    text ") || !mulib_xll_json_(&out, mulib_json_string_n(b->text, b->length)) ||
            !mulib_strbuf_append(&out, ";\n    font ") || !mulib_xll_json_(&out, mulib_json_string(b->font_family)) ||
            !mulib_strbuf_putc(&out, ' ') || !mulib_xll_double_(&out, b->font_size) ||
            !mulib_strbuf_append(&out, ";\n    language ") || !mulib_xll_json_(&out, mulib_json_string(b->language)) ||
            !mulib_strbuf_append(&out, ";\n    width ") || !mulib_xll_double_(&out, b->options.width) ||
            !mulib_strbuf_append(&out, ";\n    spacing ") || !mulib_xll_double_(&out, b->options.spacing) ||
            !mulib_strbuf_append(&out, ";\n    origin ") || !mulib_xll_double_(&out, b->x) ||
            !mulib_strbuf_putc(&out, ' ') || !mulib_xll_double_(&out, b->y) ||
            !mulib_strbuf_append(&out, ";\n    wrap ") || !mulib_strbuf_append(&out, mulib_xll_wrap_[b->options.wrap]) ||
            !mulib_strbuf_append(&out, ";\n    align ") || !mulib_strbuf_append(&out, mulib_xll_align_[b->options.alignment]) ||
            !mulib_strbuf_append(&out, ";\n    direction ") || !mulib_strbuf_append(&out, mulib_xll_base_[b->options.base+2]) ||
            !mulib_strbuf_append(&out, ";\n  }\n")) goto fail;
    }
    if (!mulib_strbuf_append(&out, "}\n")) goto fail;
    if (length) *length = out.length;
    return out.data;
fail:
    mulib_strbuf_free(&out); return NULL;
}
mulib_layout_t *mulib_xll_create_layout(const mulib_xll_t *p, size_t index,
    const mulib_pglayout_fontset_t *fonts, mulib_xml_error_t *error) {
    const mulib_xll_block_t *b = mulib_xll_get(p, index);
    mulib_pglayout_fontset_t set;
    mulib_layout_t *layout;
    size_t i;
    if (error) { error->offset = 0; error->message = "invalid XLL layout/font set or mismatched scale"; }
    if (!b || !fonts || !fonts->unicode || !fonts->fonts || !fonts->count || fonts->count > SIZE_MAX/sizeof *fonts->fonts) return NULL;
    for (i = 0; i < fonts->count; ++i)
        if (mulib_fontshape_scale(fonts->fonts[i]) != (int)(b->font_size*64+0.5)) return NULL;
    set = *fonts; set.language = b->language; set.base = b->options.base;
    layout = mulib_layout_create(&set);
    if (!layout || mulib_layout_configure(layout, &b->options) || mulib_layout_set_text(layout, b->text, b->length)) {
        mulib_layout_destroy(layout);
        if (error) error->message = "layout allocation/configuration failure";
        return NULL;
    }
    if (error) error->message = NULL;
    return layout;
}
int mulib_xll_compute(const mulib_xll_t *p, size_t i, const mulib_pglayout_fontset_t *fonts,
    mulib_layout_result_t *out, mulib_xml_error_t *error) {
    mulib_layout_t *layout = mulib_xll_create_layout(p, i, fonts, error);
    int result;
    if (!layout) return -1;
    result = mulib_layout_compute(layout, out, error); mulib_layout_destroy(layout); return result;
}
/* Emit bounded lines of byte initializers instead of C strings: no escaping,
 * execution-charset dependence, trigraphs, or long concatenated string limits. */
static int mulib_xll_bytes_(mulib_strbuf_t *out, const char *symbol, size_t index, const char *s, size_t n) {
    size_t i; static const char hex[] = "0123456789abcdef";
    if (!mulib_strbuf_append(out, "static const unsigned char ") || !mulib_strbuf_append(out, symbol) ||
        !mulib_strbuf_append(out, "_bytes_") || !mulib_xll_size_(out, index) || !mulib_strbuf_append(out, "[] = {\n")) return 0;
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i]; char v[] = {'0', 'x', hex[c >> 4], hex[c & 15], ','};
        if (!mulib_strbuf_append_n(out, v, sizeof v) || (i%16 == 15 && !mulib_strbuf_putc(out, '\n'))) return 0;
    }
    return mulib_strbuf_append(out, "0\n};\n");
}
static int mulib_xll_ref_(mulib_strbuf_t *out, const char *symbol, size_t index) {
    return mulib_strbuf_append(out, "(const char *)") && mulib_strbuf_append(out, symbol) &&
        mulib_strbuf_append(out, "_bytes_") && mulib_xll_size_(out, index);
}
/* Templates contain @ as a symbol placeholder, never source text. */
static int mulib_xll_template_(mulib_strbuf_t *out, const char *s, const char *symbol) {
    const char *p = s;
    while (*p) {
        if (*p == '@') {
            if (!mulib_strbuf_append_n(out, s, (size_t)(p-s)) || !mulib_strbuf_append(out, symbol)) return 0;
            s = ++p;
        } else ++p;
    }
    return mulib_strbuf_append_n(out, s, (size_t)(p-s));
}
char *mulib_xll_emit_c(const mulib_xll_t *p, const char *prefix, size_t *length, mulib_xll_error_t *error) {
    mulib_strbuf_t out = {0}; size_t i, j;
    char symbol[80];
    if (length) *length = 0;
    mulib_xll_error_(error, NULL);
    if (!p || !mulib_xll_ident_(prefix)) { mulib_xll_error_(error, "invalid program or C symbol prefix"); return NULL; }
    (void)snprintf(symbol, sizeof symbol, "mulib_xll_%s", prefix);
    if (!mulib_strbuf_append(&out, "/* Generated from XLL 1. Application supplies layout implementations. */\n"
        "#include \"mulib-layout.h\"\n\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n") ||
        !mulib_xll_template_(&out, "typedef struct @_block {\n"
        "  const char *name, *text, *font_family, *language;\n  size_t length;\n"
        "  double font_size, x, y;\n  mulib_layout_options_t options;\n} @_block_t;\n", symbol)) goto fail;
    for (i = 0; i < p->count; ++i) {
        const mulib_xll_block_t *b = &p->blocks[i];
        if (!mulib_xll_bytes_(&out, symbol, 4*i, b->name, strlen(b->name)) ||
            !mulib_xll_bytes_(&out, symbol, 4*i+1, b->text, b->length) ||
            !mulib_xll_bytes_(&out, symbol, 4*i+2, b->font_family, strlen(b->font_family)) ||
            !mulib_xll_bytes_(&out, symbol, 4*i+3, b->language, strlen(b->language))) goto fail;
    }
    if (p->count) {
        if (!mulib_xll_template_(&out, "static const @_block_t @_blocks[] = {\n", symbol)) goto fail;
        for (i = 0; i < p->count; ++i) {
            const mulib_xll_block_t *b = &p->blocks[i];
            if (!mulib_strbuf_putc(&out, '{')) goto fail;
            for (j = 0; j < 4; ++j) if (!mulib_xll_ref_(&out, symbol, 4*i+j) || !mulib_strbuf_putc(&out, ',')) goto fail;
            if (!mulib_xll_size_(&out, b->length) || !mulib_strbuf_putc(&out, ',') || !mulib_xll_c_double_(&out, b->font_size) ||
                !mulib_strbuf_putc(&out, ',') || !mulib_xll_c_double_(&out, b->x) || !mulib_strbuf_putc(&out, ',') ||
                !mulib_xll_c_double_(&out, b->y) || !mulib_strbuf_append(&out, ",{ ") || !mulib_xll_c_double_(&out, b->options.width) ||
                !mulib_strbuf_putc(&out, ',') || !mulib_xll_c_double_(&out, b->options.spacing) ||
                !mulib_strbuf_append(&out, ",(mulib_layout_wrap_t)") || !mulib_xll_size_(&out, (size_t)b->options.wrap) ||
                !mulib_strbuf_append(&out, ",(mulib_layout_alignment_t)") || !mulib_xll_size_(&out, (size_t)b->options.alignment) ||
                !mulib_strbuf_putc(&out, ',') || !mulib_xll_double_(&out, b->options.base) || !mulib_strbuf_append(&out, "}},\n")) goto fail;
        }
        if (!mulib_strbuf_append(&out, "};\n")) goto fail;
    }
    if (!mulib_xll_template_(&out, "size_t @_count(void) { return ", symbol) || !mulib_xll_size_(&out, p->count) ||
        !mulib_xll_template_(&out, "; }\nconst @_block_t *@_get(size_t index) {\n", symbol)) goto fail;
    if (p->count) {
        if (!mulib_xll_template_(&out, "  return index < @_count() ? &@_blocks[index] : NULL;\n}\n", symbol)) goto fail;
    } else if (!mulib_strbuf_append(&out, "  (void)index; return NULL;\n}\n")) goto fail;
    if (!mulib_xll_template_(&out,
        "mulib_layout_t *@_create(size_t index, const mulib_pglayout_fontset_t *fonts, mulib_xml_error_t *error) {\n"
        "  const @_block_t *b = @_get(index);\n  mulib_pglayout_fontset_t set;\n  mulib_layout_t *layout;\n  size_t i;\n"
        "  if (error) { error->offset = 0; error->message = \"invalid XLL layout/font set or mismatched scale\"; }\n"
        "  if (!b || !fonts || !fonts->unicode || !fonts->fonts || !fonts->count || fonts->count > SIZE_MAX/sizeof *fonts->fonts) return NULL;\n"
        "  for (i = 0; i < fonts->count; ++i)\n"
        "    if (mulib_fontshape_scale(fonts->fonts[i]) != (int)(b->font_size*64+0.5)) return NULL;\n"
        "  set = *fonts; set.language = b->language; set.base = b->options.base;\n"
        "  layout = mulib_layout_create(&set);\n"
        "  if (!layout || mulib_layout_configure(layout, &b->options) || mulib_layout_set_text(layout, b->text, b->length)) {\n"
        "    mulib_layout_destroy(layout);\n    if (error) error->message = \"layout allocation/configuration failure\";\n    return NULL;\n  }\n"
        "  if (error) error->message = NULL;\n  return layout;\n}\n"
        "int @_compute(size_t index, const mulib_pglayout_fontset_t *fonts, mulib_layout_result_t *out, mulib_xml_error_t *error) {\n"
        "  mulib_layout_t *layout = @_create(index, fonts, error);\n  int result;\n  if (!layout) return -1;\n"
        "  result = mulib_layout_compute(layout, out, error);\n  mulib_layout_destroy(layout); return result;\n}\n"
        "#ifdef __cplusplus\n}\n#endif\n", symbol)) goto fail;
    if (length) *length = out.length;
    return out.data;
fail:
    mulib_strbuf_free(&out); mulib_xll_error_(error, "allocation failure during C emission"); return NULL;
}
int mulib_xll_write_c(const mulib_xll_t *p, const char *prefix, FILE *stream, mulib_xll_error_t *error) {
    char *source; size_t n; int result;
    if (!stream) { mulib_xll_error_(error, "NULL output stream"); return -1; }
    source = mulib_xll_emit_c(p, prefix, &n, error);
    if (!source) return -1;
    result = fwrite(source, 1, n, stream) == n && !ferror(stream) ? 0 : -1;
    free(source);
    if (result) mulib_xll_error_(error, "output write failure");
    return result;
}
#undef MULIB_XLL_BYTES_
#undef MULIB_XLL_SOURCE_BYTES_
#ifdef __cplusplus
}
#endif
#endif
