/** @file mulib-docir.h
 * @brief Single-owner semantic trees, bounded snapshots and safe HTML/LaTeX export.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_DOCIR once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-docir.md for scope and limitations.
 */
#ifndef MULIB_DOCIR_H
#define MULIB_DOCIR_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { mulib_docir_ROOT=0,mulib_docir_SECTION=1,mulib_docir_PARAGRAPH=2,mulib_docir_TEXT=3,mulib_docir_LINK=4,mulib_docir_LIST=5,mulib_docir_ITEM=6,
    mulib_docir_HEADING=7,mulib_docir_MATH=8,mulib_docir_MATH_ROW=9,mulib_docir_MATH_SYMBOL=10,
    mulib_docir_MATH_FRACTION=11,mulib_docir_MATH_SUPERSCRIPT=12,mulib_docir_MATH_SUBSCRIPT=13,
    mulib_docir_MATH_RADICAL=14,mulib_docir_REFERENCE=15,mulib_docir_FIGURE=16,mulib_docir_CAPTION=17,
    mulib_docir_FOOTNOTE=18,mulib_docir_BIBLIOGRAPHY=19,mulib_docir_BIBITEM=20,mulib_docir_CITE=21,
    mulib_docir_LINEBREAK=22,mulib_docir_CODE=23 };
typedef struct mulib_docir_node { unsigned kind;char *text;struct mulib_docir_node **children;size_t count,capacity; } mulib_docir_node_t;
enum { mulib_docir_BOLD=1,mulib_docir_ITALIC=2,mulib_docir_MONOSPACE=4 };
typedef struct mulib_docir_style { unsigned flags;int32_t font_size_sp,indent_sp; } mulib_docir_style_t;
typedef struct mulib_docir_limits { size_t max_nodes,max_text_bytes;unsigned max_depth; } mulib_docir_limits_t;
enum { mulib_docir_OK=0,mulib_docir_E_ARGUMENT=1,mulib_docir_E_OWNERSHIP=2,mulib_docir_E_LIMIT=3,
    mulib_docir_E_MEMORY=4,mulib_docir_E_STRUCTURE=5,mulib_docir_E_TEXT=6,
    mulib_docir_E_REFERENCE=7,mulib_docir_E_UNSUPPORTED=8 };
typedef struct mulib_docir_report { unsigned code;size_t node; } mulib_docir_report_t;
/* Offsets into snapshot.strings; SIZE_MAX denotes an absent string/parent. */
typedef struct mulib_docir_record {
    unsigned kind,level;size_t parent,depth,child_count,text,id,target,source;
    mulib_docir_style_t style;size_t source_begin,source_end,source_line,source_column;
} mulib_docir_record_t;
typedef struct mulib_docir_snapshot { mulib_docir_record_t *records;size_t count;mulib_typecore_buf_t strings; } mulib_docir_snapshot_t;
enum { mulib_docir_CHECK_REFERENCES=1,mulib_docir_CHECK_DOCUMENT=2,mulib_docir_NORMALIZE_TEXT=1 };
enum { mulib_docir_FORMAT_HTML=0,mulib_docir_FORMAT_LATEX=1 };
mulib_docir_node_t *mulib_docir_new(unsigned kind,const char *text);
int mulib_docir_append(mulib_docir_node_t *parent,mulib_docir_node_t *child);
mulib_docir_node_t *mulib_docir_detach(mulib_docir_node_t *parent,size_t index);
const mulib_docir_node_t *mulib_docir_parent(const mulib_docir_node_t *node);
const mulib_docir_node_t *mulib_docir_owner(const mulib_docir_node_t *node);
int mulib_docir_set_text(mulib_docir_node_t *node,const char *text);
int mulib_docir_text_append(mulib_docir_node_t *node,const char *text,size_t length);
int mulib_docir_set_id(mulib_docir_node_t *node,const char *id);
int mulib_docir_set_target(mulib_docir_node_t *node,const char *uri);
int mulib_docir_set_style(mulib_docir_node_t *node,const mulib_docir_style_t *style);
int mulib_docir_set_level(mulib_docir_node_t *node,unsigned level);
int mulib_docir_set_source(mulib_docir_node_t *node,const char *file,size_t begin,size_t end,size_t line,size_t column);
int mulib_docir_snapshot_make(const mulib_docir_node_t *node,const mulib_docir_limits_t *limits,
                            mulib_docir_snapshot_t *snapshot,mulib_docir_report_t *report);
const char *mulib_docir_snapshot_string(const mulib_docir_snapshot_t *snapshot,size_t offset);
int mulib_docir_snapshot_validate(const mulib_docir_snapshot_t *snapshot,unsigned flags,mulib_docir_report_t *report);
void mulib_docir_snapshot_destroy(mulib_docir_snapshot_t *snapshot);
mulib_docir_node_t *mulib_docir_build(const mulib_docir_snapshot_t *snapshot,unsigned flags,mulib_docir_report_t *report);
mulib_docir_node_t *mulib_docir_clone(const mulib_docir_node_t *node,const mulib_docir_limits_t *limits,
                                    unsigned flags,mulib_docir_report_t *report);
int mulib_docir_html(const mulib_docir_node_t *node,mulib_typecore_buf_t *out);
int mulib_docir_latex(const mulib_docir_node_t *node,mulib_typecore_buf_t *out);
int mulib_docir_render_snapshot(const mulib_docir_snapshot_t *snapshot,unsigned format,mulib_typecore_buf_t *out);
void mulib_docir_destroy(mulib_docir_node_t *node);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DOCIR_H */
#if defined(MULIB_IMPL_DOCIR) && !defined(MULIB_DOCIR_H_IMPLEMENTED)
#define MULIB_DOCIR_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
#include "detail/mulib-docir-core.h"
#include "detail/mulib-docir-validate.h"
#include "detail/mulib-docir-render.h"
#ifdef __cplusplus
}
#endif
#endif
