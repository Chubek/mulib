/* MuLIB graphical extension: retgtk-uiml. C11, header-only. */
#ifndef MULIB_RETGTK_UIML_H
#define MULIB_RETGTK_UIML_H
#include "mulib-retgtk-builder.h"
/* Strict, bounded subset: <tag id="number" text="literal"> ... </tag> and />.
 * Rejects unknown attributes, entities, text nodes and malformed markup. */
#include <ctype.h>
typedef struct{mret_builder_node *v;size_t n,cap;}mulib_retgtk_uiml_t;
static inline void mulib_retgtk_uiml_free(mulib_retgtk_uiml_t *u){if(!u)return;for(size_t i=0;i<u->n;i++){free((void*)u->v[i].tag);free((void*)u->v[i].text);}free(u->v);memset(u,0,sizeof *u);}
static inline void mret_xml_space(const char **p){while(isspace((unsigned char)**p))++*p;}
static inline char *mret_xml_token(const char **p){const char *a=*p;while(isalnum((unsigned char)**p)||**p=='_'||**p=='-')++*p;size_t n=(size_t)(*p-a);if(!n||n>128)return NULL;char *s=malloc(n+1);if(s){memcpy(s,a,n);s[n]=0;}return s;}
static inline char *mret_xml_string(const char **p){if(**p!='"'&&**p!='\'')return NULL;char q=*(*p)++;const char *a=*p;while(**p&&**p!=q&&**p!='&'&&**p!='<'){unsigned char ch=(unsigned char)**p;if(ch<32&&ch!='\t'&&ch!='\r'&&ch!='\n')return NULL;++*p;}if(**p!=q)return NULL;size_t n=(size_t)(*p-a);char *s=malloc(n+1);if(s){memcpy(s,a,n);s[n]=0;}++*p;return s;}

#ifndef MULIB_RETGTK_UIML_MAX_BYTES
/* Maximum inspected bytes, including the required NUL terminator. */
#define MULIB_RETGTK_UIML_MAX_BYTES 1048576u
#endif
static inline int mret_xml_id(const char *s, uint64_t *id)
{
    uint64_t value = 0;
    if (!*s) return 0;
    for (; *s; ++s) {
        if (*s < '0' || *s > '9') return 0;
        unsigned digit = (unsigned)(*s - '0');
        if (value > (UINT64_MAX - digit) / 10) return 0;
        value = value * 10 + digit;
    }
    if (!value) return 0;
    *id = value;
    return 1;
}

/* out must be zero initialized or a previously parsed document. Failure leaves
 * it untouched; success releases the previous document. This is a bounded
 * XML-like subset, not an XML parser (no entities, namespaces or text nodes). */
static inline int mulib_retgtk_uiml_parse(mulib_retgtk_uiml_t *out,const char *src)
{
    if (!out || !src) return 0;
    size_t bytes = 0;
    while (bytes < MULIB_RETGTK_UIML_MAX_BYTES && src[bytes]) ++bytes;
    /* Do not inspect src[limit]: callers may supply exactly limit bytes. */
    if (bytes == MULIB_RETGTK_UIML_MAX_BYTES) return 0;
    mulib_retgtk_uiml_t u = {0};
    size_t stack[MULIB_RETGTK_BUILDER_MAX_DEPTH], depth = 0;
    const char *p = src;
    for (;;) {
        mret_xml_space(&p);
        if (!*p) break;
        if (*p++ != '<') goto fail;
        if (*p == '/') {
            ++p;
            char *tag = mret_xml_token(&p);
            int ok = tag && depth && !strcmp(tag, u.v[stack[depth - 1]].tag);
            free(tag);
            mret_xml_space(&p);
            if (!ok || *p != '>') goto fail;
            ++p; --depth;
            continue;
        }
        if ((!depth && u.n) || depth == MULIB_RETGTK_BUILDER_MAX_DEPTH ||
            u.n >= MULIB_RETGTK_BUILDER_MAX_NODES ||
            !mgx_grow((void **)&u.v, &u.cap, u.n + 1, sizeof *u.v)) goto fail;
        size_t i = u.n++;
        u.v[i] = (mret_builder_node){mret_xml_token(&p), NULL, i + 1,
                                    depth ? stack[depth - 1] : SIZE_MAX};
        if (!u.v[i].tag) goto fail;
        int seen_id = 0;
        for (;;) {
            const char *before_space = p;
            mret_xml_space(&p);
            if (*p == '>' || (*p == '/' && p[1] == '>')) break;
            if (before_space == p) goto fail;
            char *attribute = mret_xml_token(&p);
            mret_xml_space(&p);
            if (!attribute || *p != '=') { free(attribute); goto fail; }
            ++p;
            mret_xml_space(&p);
            char *value = mret_xml_string(&p);
            if (!value) { free(attribute); goto fail; }
            int ok = 1;
            if (!strcmp(attribute, "id") && !seen_id) {
                ok = mret_xml_id(value, &u.v[i].id);
                seen_id = 1;
            } else if (!strcmp(attribute, "text") && !u.v[i].text) {
                u.v[i].text = value;
                value = NULL;
            } else ok = 0;
            free(attribute); free(value);
            if (!ok) goto fail;
        }
        if (*p == '/') p += 2;
        else { ++p; stack[depth++] = i; }
    }
    if (depth || !u.n || !mret_builder_ids_valid(u.v, u.n)) goto fail;
    mulib_retgtk_uiml_free(out);
    *out = u;
    return 1;
fail:
    mulib_retgtk_uiml_free(&u);
    return 0;
}
#endif
