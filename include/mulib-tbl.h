/** @file mulib-tbl.h
 * @brief Rectangular documentation tables, pipe/CSV readers and portable writers.
 * Define MULIB_IMPL_TBL plus DSDOM, XML, CSV, TEX, STRBUF and UTF8.
 */
#ifndef MULIB_TBL_H
#define MULIB_TBL_H
#include "mulib-dsdom.h"
#include "mulib-csv.h"
#include "mulib-tex.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mulib_dsdom_t mulib_tbl_t;
typedef enum mulib_tbl_align { MULIB_TBL_DEFAULT, MULIB_TBL_LEFT, MULIB_TBL_CENTER, MULIB_TBL_RIGHT } mulib_tbl_align_t;
/** Owned split row. Initialize to zero; free before reuse. A separator contains
 * at least three dashes per field and optional alignment colons. */
typedef struct mulib_tbl_fields { char **items; mulib_tbl_align_t *align; size_t count; int separator; } mulib_tbl_fields_t;
/** @brief Split a pipe row without evaluating markup. Protect escaped pipes, matched
 * backtick runs and balanced (),[]/{} contents. 1/0; output unchanged on error. */
int mulib_tbl_split(const char *source, size_t length, mulib_tbl_fields_t *out, mulib_xml_error_t *error);
/** @brief Free split fields and zero the descriptor; NULL is harmless. */
void mulib_tbl_fields_free(mulib_tbl_fields_t *fields);
/** @brief New owned table element. XML tree edits and ownership rules apply. */
mulib_tbl_t *mulib_tbl_create(void);
/** @brief Copy and append a literal row atomically. First row establishes width.
 * At most 4096 rows, 256 columns; header is 0/1. Returns borrowed row or NULL. */
mulib_dsdom_t *mulib_tbl_add_row(mulib_tbl_t *, const char *const *fields, size_t count, int header, const mulib_tbl_align_t *align);
/** @brief Count rows, or zero for an empty/invalid root. */
size_t mulib_tbl_rows(const mulib_tbl_t *);
/** @brief Count first-row columns, or zero for an empty/invalid root. */
size_t mulib_tbl_columns(const mulib_tbl_t *);
/** @brief Borrow a zero-based cell; NULL out of bounds or for an invalid root. */
mulib_dsdom_t *mulib_tbl_cell(const mulib_tbl_t *, size_t row, size_t column);
/** @brief Validate rectangular table/row/cell structure, alignment, header flags,
 * nesting <=64 and <=8 MiB text/attributes. Spanning cells are not supported. */
int mulib_tbl_validate(const mulib_tbl_t *, mulib_xml_error_t *error);
/** @brief Parse pipe (default) or CSV. Pipe separators set headers/alignment. Empty
 * input returns an empty table. Source <=1 MiB, XML-valid UTF-8. */
mulib_tbl_t *mulib_tbl_parse(const char *, size_t, const char *format, mulib_xml_error_t *error);
/** @brief Owned xml/html/latex/pipe/csv serialization. Non-XML writers use descendant
 * cell text, not raw markup; HTML escapes text. Length excludes NUL, zero on
 * error. CSV is quoted losslessly; pipe rejects multiline cells. */
char *mulib_tbl_render(const mulib_tbl_t *, const char *format, size_t *length, mulib_xml_error_t *error);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TBL) && !defined(MULIB_TBL_IMPLEMENTED)
#define MULIB_TBL_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_tbl_error_(mulib_xml_error_t *e, size_t at, const char *message) {
    if (e) { e->offset = at; e->message = message; } return 0;
}
static void mulib_tbl_trim_(const char **s, size_t *n) {
    while (*n && (**s == ' ' || **s == '\t' || **s == '\r')) { ++*s; --*n; }
    while (*n && ((*s)[*n-1] == ' ' || (*s)[*n-1] == '\t' || (*s)[*n-1] == '\r')) --*n;
}
void mulib_tbl_fields_free(mulib_tbl_fields_t *f) {
    size_t i; if (!f) return;
    for (i = 0; i < f->count; ++i) free(f->items[i]);
    free(f->items); free(f->align); memset(f,0,sizeof *f);
}
int mulib_tbl_split(const char *s, size_t n, mulib_tbl_fields_t *out, mulib_xml_error_t *e) {
    mulib_tbl_fields_t f = {0};
    size_t at, begin, depth = 0, code = 0, steps=0;
    char stack[64];
    if (e) { e->offset = 0; e->message = NULL; }
    if (!out || out->items || out->align || out->count || n > 1024*1024 || !mulib_xml_valid_text(s,n))
        return mulib_tbl_error_(e,0,"invalid table row/output");
    mulib_tbl_trim_(&s,&n);
    if (!n) { f.separator=0;*out=f;return 1; }
    at = n && s[0] == '|' ? 1 : 0; begin = at; f.separator = 1;
    for (; at <= n; ++at) {
        if (++steps>4*1024*1024) goto work_limit;
        if (at < n) {
            size_t run;
            if (!code && s[at] == '\\' && at+1 < n) { ++at; continue; }
            if (s[at] == '`') {
                run = 1; while (at+run < n && s[at+run] == '`') ++run;
                if (!code) {
                    size_t j = at+run;
                    while (j < n) { size_t k = 0; if (++steps>4*1024*1024) goto work_limit; if (s[j] == '`') { while (j+k < n && s[j+k] == '`') ++k; steps+=k; if(steps>4*1024*1024)goto work_limit; if (k == run) break; } j += k ? k : 1; }
                    if (j < n) code = run;
                } else if (code == run) code = 0;
                at += run-1; continue;
            }
            if (!code && (s[at] == '(' || s[at] == '[' || s[at] == '{')) {
                if (depth == sizeof stack) { mulib_tbl_fields_free(&f); return mulib_tbl_error_(e,at,"table row nesting limit"); }
                stack[depth++] = s[at];
            } else if (!code && depth && ((s[at] == ')' && stack[depth-1] == '(') ||
                (s[at] == ']' && stack[depth-1] == '[') || (s[at] == '}' && stack[depth-1] == '{'))) --depth;
            if (s[at] != '|' || code || depth) continue;
        }
        if (at == n && begin == n && n && s[n-1] == '|') break;
        {
            const char *text = s+begin; size_t length = at-begin, left, right, j;
            char **items, *copy;
            mulib_tbl_align_t *align;
            if (f.count == 256) { mulib_tbl_fields_free(&f); return mulib_tbl_error_(e,at,"table column limit"); }
            mulib_tbl_trim_(&text,&length);
            copy = (char *)malloc(length+1); if (!copy) goto memory;
            memcpy(copy,text,length); copy[length] = 0;
            items = (char **)realloc(f.items,(f.count+1)*sizeof *items);
            if (!items) { free(copy); goto memory; } f.items = items;
            align = (mulib_tbl_align_t *)realloc(f.align,(f.count+1)*sizeof *align);
            if (!align) { free(copy); goto memory; } f.align = align;
            left = length && text[0] == ':'; right = length > left && text[length-1] == ':';
            j = left; while (j < length-right && text[j] == '-') ++j;
            if (j != length-right || j-left < 3) f.separator = 0;
            f.align[f.count] = left && right ? MULIB_TBL_CENTER : right ? MULIB_TBL_RIGHT : left ? MULIB_TBL_LEFT : MULIB_TBL_DEFAULT;
            f.items[f.count++] = copy; begin = at+1;
        }
    }
    if (!f.count) f.separator = 0;
    *out = f; return 1;
memory:
    mulib_tbl_fields_free(&f); return mulib_tbl_error_(e,at,"table row allocation failure");
work_limit:
    mulib_tbl_fields_free(&f); return mulib_tbl_error_(e,at,"table row work limit");
}
mulib_tbl_t *mulib_tbl_create(void) { return mulib_dsdom_element("table",NULL); }
static int mulib_tbl_named_(const mulib_dsdom_t *n, const char *name) { return mulib_xml_name(n) && !strcmp(mulib_xml_name(n),name); }
static int mulib_tbl_content_(const mulib_dsdom_t *,size_t,size_t *,size_t *);
static int mulib_tbl_measure_(const mulib_tbl_t *,size_t *,size_t *,mulib_xml_error_t *);
size_t mulib_tbl_rows(const mulib_tbl_t *t) { size_t n = 0; const mulib_dsdom_t *r; if (!mulib_tbl_named_(t,"table")) return 0; for (r=mulib_xml_first(t);r;r=mulib_xml_next(r)) ++n; return n; }
size_t mulib_tbl_columns(const mulib_tbl_t *t) { size_t n=0; const mulib_dsdom_t *c; if (!mulib_tbl_named_(t,"table")) return 0; for (c=mulib_xml_first(mulib_xml_first(t));c;c=mulib_xml_next(c)) ++n; return n; }
mulib_dsdom_t *mulib_tbl_cell(const mulib_tbl_t *t, size_t r, size_t col) {
    mulib_dsdom_t *n; if (!mulib_tbl_named_(t,"table")) return NULL;
    n=mulib_xml_first(t); while (n && r--) n=mulib_xml_next(n); n=mulib_xml_first(n);
    while (n && col--) n=mulib_xml_next(n);
    return n;
}
mulib_dsdom_t *mulib_tbl_add_row(mulib_tbl_t *t, const char *const *fields, size_t count, int header, const mulib_tbl_align_t *align) {
    static const char *const names[] = {"default","left","center","right"};
    mulib_dsdom_t *r, *c; size_t i,columns,nodes=100000,bytes=8*1024*1024;
    if (!fields || !count || count>256 || (header!=0 && header!=1) || !mulib_tbl_measure_(t,&nodes,&bytes,NULL)) return NULL;
    columns=mulib_tbl_columns(t);
    if ((columns && columns!=count) || mulib_tbl_rows(t)>=4096) return NULL;
    r=mulib_dsdom_element("row",NULL); if (!r) return NULL;
    for (i=0;i<count;++i) {
        c=mulib_dsdom_element("cell",fields[i]);
        if (!fields[i] || !c) { mulib_xml_destroy(c); goto fail; }
        if (!mulib_xml_append(r,c)) { mulib_xml_destroy(c); goto fail; }
        if ((header && !mulib_xml_set_attr(c,"header","true")) ||
            (align && (align[i]<MULIB_TBL_DEFAULT || align[i]>MULIB_TBL_RIGHT ||
                (align[i] && !mulib_xml_set_attr(c,"align",names[align[i]]))))) goto fail;
    }
    if (!mulib_tbl_content_(r,1,&nodes,&bytes) || !mulib_xml_append(t,r)) goto fail;
    return r;
fail:
    mulib_xml_destroy(r); return NULL;
}
static int mulib_tbl_content_(const mulib_dsdom_t *n, size_t depth, size_t *nodes, size_t *bytes) {
    const mulib_dsdom_t *c; size_t i,len; const char *text=mulib_xml_name(n);
    if (!n || !*nodes || depth>64 || (mulib_xml_type(n)!=MULIB_XML_ELEMENT && mulib_xml_type(n)!=MULIB_XML_TEXT)) return 0;
    --*nodes; if (!text) text=mulib_xml_value(n); len=strlen(text); if (len>*bytes) return 0; *bytes-=len;
    for (i=0;i<mulib_xml_attr_count(n);++i) {
        len=strlen(mulib_xml_attr_name(n,i)); if (len>*bytes) return 0; *bytes-=len;
        len=strlen(mulib_xml_attr_value(n,i)); if (len>*bytes) return 0; *bytes-=len;
    }
    for (c=mulib_xml_first(n);c;c=mulib_xml_next(c)) if (!mulib_tbl_content_(c,depth+1,nodes,bytes)) return 0;
    return 1;
}
static int mulib_tbl_measure_(const mulib_tbl_t *t,size_t *nodes,size_t *bytes,mulib_xml_error_t *e) {
    const mulib_dsdom_t *r,*c; size_t rows=0,columns=0,count;
    if (e) { e->offset=0; e->message=NULL; }
    if (!mulib_tbl_named_(t,"table") || !mulib_tbl_content_(t,0,nodes,bytes)) return mulib_tbl_error_(e,0,"invalid/bounded table tree");
    for (r=mulib_xml_first(t);r;r=mulib_xml_next(r)) {
        if (++rows>4096 || !mulib_tbl_named_(r,"row")) goto invalid;
        count=0;
        for (c=mulib_xml_first(r);c;c=mulib_xml_next(c)) {
            const char *a=mulib_xml_attr(c,"align"),*h=mulib_xml_attr(c,"header");
            if (++count>256 || !mulib_tbl_named_(c,"cell") ||
                (a && strcmp(a,"default") && strcmp(a,"left") && strcmp(a,"center") && strcmp(a,"right")) ||
                (h && strcmp(h,"true") && strcmp(h,"false")) || mulib_xml_attr(c,"colspan") || mulib_xml_attr(c,"rowspan")) goto invalid;
        }
        if (!count || (columns && columns!=count)) goto invalid;
        columns=count;
    }
    return 1;
invalid:
    return mulib_tbl_error_(e,rows-1,"ragged/invalid table row, alignment or span");
}
int mulib_tbl_validate(const mulib_tbl_t *t,mulib_xml_error_t *e) { size_t nodes=100000,bytes=8*1024*1024;return mulib_tbl_measure_(t,&nodes,&bytes,e); }
typedef struct mulib_tbl_csv_ { mulib_tbl_t *table; } mulib_tbl_csv_;
static int mulib_tbl_csv_row_(void *u,const char *const *f,size_t n) { return mulib_tbl_add_row(((mulib_tbl_csv_ *)u)->table,f,n,0,NULL)!=NULL; }
mulib_tbl_t *mulib_tbl_parse(const char *s,size_t length,const char *format,mulib_xml_error_t *e) {
    mulib_tbl_t *t=NULL; size_t at=0,first=0; mulib_tbl_align_t align[256]={0};
    if (e) { e->offset=0;e->message=NULL; }
    if (length>1024*1024 || !mulib_xml_valid_text(s,length)) { (void)mulib_tbl_error_(e,0,"invalid/oversized table source"); return NULL; }
    t=mulib_tbl_create(); if (!t) goto memory;
    if (format && !strcmp(format,"csv")) {
        char *copy=(char *)malloc(length+1); mulib_tbl_csv_ ctx;
        int ok; if (!copy) goto memory; if (length) memcpy(copy,s,length); copy[length]=0; ctx.table=t;
        ok=mulib_csv_parse(copy,mulib_tbl_csv_row_,&ctx); free(copy);
        if (!ok) { (void)mulib_tbl_error_(e,0,"invalid/ragged CSV table or allocation failure"); goto fail; }
    } else if (!format || !strcmp(format,"pipe")) {
        while (at<length) {
            size_t end=at,n,i; const char *line; mulib_tbl_fields_t f={0};
            while (end<length && s[end]!='\n') ++end;
            line=s+at;n=end-at;mulib_tbl_trim_(&line,&n);
            if (n) {
                if (!mulib_tbl_split(line,n,&f,e)) goto fail;
                if (f.separator) {
                    if (mulib_tbl_rows(t)!=1 || first || f.count!=mulib_tbl_columns(t)) { mulib_tbl_fields_free(&f); (void)mulib_tbl_error_(e,at,"misplaced table separator"); goto fail; }
                    first=1; memcpy(align,f.align,f.count*sizeof *align);
                    for (i=0;i<f.count;++i) {
                        static const char *const a[]={"default","left","center","right"};
                        mulib_dsdom_t *c=mulib_tbl_cell(t,0,i);
                        if (!mulib_xml_set_attr(c,"header","true") || (align[i] && !mulib_xml_set_attr(c,"align",a[align[i]]))) { mulib_tbl_fields_free(&f); goto memory; }
                    }
                } else {
                    for (i=0;i<f.count;++i) { size_t in=0,out=0; while(f.items[i][in]) { if(f.items[i][in]=='\\'&&(f.items[i][in+1]=='|'||f.items[i][in+1]=='\\'))++in;f.items[i][out++]=f.items[i][in++]; }f.items[i][out]=0; }
                    if (!mulib_tbl_add_row(t,(const char *const *)f.items,f.count,0,align)) { mulib_tbl_fields_free(&f); (void)mulib_tbl_error_(e,at,"ragged/bounded table or allocation failure"); goto fail; }
                }
                mulib_tbl_fields_free(&f);
            }
            at=end+(end<length);
        }
    } else { (void)mulib_tbl_error_(e,0,"unknown table format"); goto fail; }
    return t;
memory:
    (void)mulib_tbl_error_(e,at,"table allocation failure");
fail:
    mulib_xml_destroy(t); return NULL;
}
char *mulib_tbl_render(const mulib_tbl_t *t,const char *format,size_t *length,mulib_xml_error_t *e) {
    mulib_strbuf_t b={0}; const mulib_dsdom_t *r,*c; size_t i; int html,tex,csv,pipe;
    if (length) *length=0;
    if (!mulib_tbl_validate(t,e)) return NULL;
    if (!format || !strcmp(format,"xml")) { char *xml=mulib_xml_serialize(t,length);if(!xml)(void)mulib_tbl_error_(e,0,"table XML serialization failure");return xml; }
    html=!strcmp(format,"html");tex=!strcmp(format,"latex");csv=!strcmp(format,"csv");pipe=!strcmp(format,"pipe");
    if (!html && !tex && !csv && !pipe) { (void)mulib_tbl_error_(e,0,"unknown table output"); return NULL; }
    if(tex&&!mulib_tbl_rows(t)) { if(!mulib_strbuf_append(&b,""))goto fail;return b.data; }
    if (html && !mulib_strbuf_append(&b,"<table>")) goto fail;
    if (tex) {
        if (!mulib_strbuf_append(&b,"\\begin{tabular}{")) goto fail;
        for (i=0;i<mulib_tbl_columns(t);++i) { const char *a=mulib_xml_attr(mulib_tbl_cell(t,0,i),"align"); if (!mulib_strbuf_putc(&b,a&&!strcmp(a,"right")?'r':a&&!strcmp(a,"center")?'c':'l')) goto fail; }
        if (!mulib_strbuf_append(&b,"}\n")) goto fail;
    }
    for (r=mulib_xml_first(t);r;r=mulib_xml_next(r)) {
        if (html && !mulib_strbuf_append(&b,"<tr>")) goto fail;
        if (pipe && !mulib_strbuf_putc(&b,'|')) goto fail;
        for (c=mulib_xml_first(r),i=0;c;c=mulib_xml_next(c),++i) {
            char *text=mulib_dsdom_text(c,NULL); const char *h=mulib_xml_attr(c,"header"),*tag=h&&!strcmp(h,"true")?"th":"td";
            int ok=1; size_t j;
            if (!text) goto fail;
            if (html) {
                const char *a=mulib_xml_attr(c,"align");
                ok=mulib_strbuf_putc(&b,'<')&&mulib_strbuf_append(&b,tag);
                if(ok&&a&&strcmp(a,"default"))ok=mulib_strbuf_append(&b," align=\"")&&mulib_strbuf_append(&b,a)&&mulib_strbuf_putc(&b,'"');
                ok=ok&&mulib_strbuf_putc(&b,'>')&&mulib_xml_escape(&b,text,strlen(text),0)&&mulib_strbuf_append(&b,"</")&&mulib_strbuf_append(&b,tag)&&mulib_strbuf_putc(&b,'>');
            }
            else if (tex) ok=(!i||mulib_strbuf_append(&b," & "))&&mulib_tex_escape(&b,text,strlen(text));
            else if (csv) {
                ok=(!i||mulib_strbuf_putc(&b,','))&&mulib_strbuf_putc(&b,'"');
                for (j=0;ok&&text[j];++j) ok=(text[j]!='"'||mulib_strbuf_putc(&b,'"'))&&mulib_strbuf_putc(&b,text[j]);
                ok=ok&&mulib_strbuf_putc(&b,'"');
            } else {
                for (j=0;ok&&text[j];++j) { if (text[j]=='\n'||text[j]=='\r') { ok=0;break; } ok=((text[j]!='|'&&text[j]!='\\')||mulib_strbuf_putc(&b,'\\'))&&mulib_strbuf_putc(&b,text[j]); }
                ok=ok&&mulib_strbuf_putc(&b,'|');
            }
            free(text); if (!ok) goto fail;
        }
        if (!mulib_strbuf_append(&b,html?"</tr>":tex?" \\\\\n":csv?"\r\n":"\n")) goto fail;
        if (pipe && r==mulib_xml_first(t)) {
            int header=0; for (c=mulib_xml_first(r);c;c=mulib_xml_next(c)) { const char *h=mulib_xml_attr(c,"header"); if(h&&!strcmp(h,"true"))header=1; }
            if (header) { if (!mulib_strbuf_putc(&b,'|')) goto fail; for(c=mulib_xml_first(r);c;c=mulib_xml_next(c)) { const char *a=mulib_xml_attr(c,"align"); if (!mulib_strbuf_append(&b,a&&!strcmp(a,"center")?":---:|":a&&!strcmp(a,"right")?"---:|":a&&!strcmp(a,"left")?":---|":"---|")) goto fail; } if(!mulib_strbuf_putc(&b,'\n'))goto fail; }
        }
    }
    if (!mulib_strbuf_append(&b,html?"</table>":tex?"\\end{tabular}\n":"")) goto fail;
    if (length) *length=b.length;
    return b.data;
fail:
    mulib_strbuf_free(&b); (void)mulib_tbl_error_(e,0,"table serialization failure (allocation or multiline pipe cell)"); return NULL;
}
#ifdef __cplusplus
}
#endif
#endif
