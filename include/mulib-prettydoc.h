/**
 * @file mulib-prettydoc.h
 * @brief Portable C11 document algebra, structural source layouts, and annotated events.
 * Define MULIB_IMPL_PRETTYDOC in exactly one translation unit.
 * No nonstandard dependencies. NOT a glyph shaper or a complete Oppen/Leijen implementation.
 */
#ifndef MULIB_PRETTYDOC_H
#define MULIB_PRETTYDOC_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_prettydoc mulib_prettydoc_t;
typedef uint32_t mulib_pretty_id_t;
#define MULIB_PRETTY_NONE UINT32_MAX

typedef enum mulib_pretty_kind {
 MULIB_PRETTY_TEXT, MULIB_PRETTY_SOFTLINE, MULIB_PRETTY_HARDLINE,
 MULIB_PRETTY_CAT, MULIB_PRETTY_GROUP, MULIB_PRETTY_NEST,
 MULIB_PRETTY_ALIGN, MULIB_PRETTY_CHOICE, MULIB_PRETTY_ANNOTATE,
 MULIB_PRETTY_FILL, MULIB_PRETTY_REGION, MULIB_PRETTY_PARAGRAPH_REGION
} mulib_pretty_kind_t;
typedef enum mulib_pretty_style {
 MULIB_PRETTY_NORMAL, MULIB_PRETTY_KEYWORD, MULIB_PRETTY_IDENTIFIER,
 MULIB_PRETTY_NUMBER, MULIB_PRETTY_OPERATOR, MULIB_PRETTY_COMMENT,
 MULIB_PRETTY_STRING, MULIB_PRETTY_MATH_IDENT,
 MULIB_PRETTY_MATH_NUMBER, MULIB_PRETTY_MATH_OPERATOR
} mulib_pretty_style_t;
typedef enum mulib_pretty_algorithm {
 MULIB_PRETTY_GREEDY, /* greedily fit the next text fragment at each softline */
 MULIB_PRETTY_FITS,   /* Wadler-style group flattening when it fits */
 MULIB_PRETTY_SEARCH  /* bounded group-layout enumeration and cost scoring */
} mulib_pretty_algorithm_t;
typedef enum mulib_pretty_paragraph {
 MULIB_PRETTY_WORD_GREEDY, MULIB_PRETTY_MIN_RAGGEDNESS
} mulib_pretty_paragraph_t;
typedef struct mulib_pretty_options {
 size_t width, indent_limit, max_output;
 mulib_pretty_algorithm_t algorithm;
 mulib_pretty_paragraph_t paragraph;
 unsigned search_group_limit; /* 0..12; bounded exhaustive group layouts */
 size_t ribbon_width; /* columns after leading indentation; 0 disables */
} mulib_pretty_options_t;
typedef struct mulib_pretty_piece {
 size_t start, length;
 mulib_pretty_style_t style;
} mulib_pretty_piece_t;
typedef struct mulib_pretty_layout {
 char *text; size_t length;
 mulib_pretty_piece_t *pieces; size_t count;
 size_t lines, max_columns;
} mulib_pretty_layout_t;

mulib_prettydoc_t *mulib_prettydoc_new(void);
void mulib_prettydoc_free(mulib_prettydoc_t *doc);
mulib_pretty_id_t mulib_pretty_text(mulib_prettydoc_t *doc,const char *text);
mulib_pretty_id_t mulib_pretty_fill(mulib_prettydoc_t *doc,const char *text);
mulib_pretty_id_t mulib_pretty_softline(mulib_prettydoc_t *doc);
mulib_pretty_id_t mulib_pretty_hardline(mulib_prettydoc_t *doc);
mulib_pretty_id_t mulib_pretty_cat(mulib_prettydoc_t *doc,mulib_pretty_id_t a,mulib_pretty_id_t b);
mulib_pretty_id_t mulib_pretty_group(mulib_prettydoc_t *doc,mulib_pretty_id_t a);
mulib_pretty_id_t mulib_pretty_nest(mulib_prettydoc_t *doc,size_t spaces,mulib_pretty_id_t a);
mulib_pretty_id_t mulib_pretty_align(mulib_prettydoc_t *doc,mulib_pretty_id_t a);
mulib_pretty_id_t mulib_pretty_choice(mulib_prettydoc_t *doc,mulib_pretty_id_t a,mulib_pretty_id_t b);
mulib_pretty_id_t mulib_pretty_annotate(mulib_prettydoc_t *doc,mulib_pretty_style_t style,mulib_pretty_id_t a);
mulib_pretty_id_t mulib_pretty_region(mulib_prettydoc_t *doc,mulib_pretty_algorithm_t algorithm,mulib_pretty_id_t a);
mulib_pretty_id_t mulib_pretty_paragraph_region(mulib_prettydoc_t *doc,mulib_pretty_paragraph_t paragraph,mulib_pretty_id_t a);
void mulib_pretty_options_init(mulib_pretty_options_t *opt);
/* Layout must be zero-initialized. Returns 1 on success, 0 on error. */
int mulib_pretty_layout(const mulib_prettydoc_t *doc,mulib_pretty_id_t root,
                       const mulib_pretty_options_t *opt,mulib_pretty_layout_t *out);
void mulib_pretty_layout_free(mulib_pretty_layout_t *out);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_PRETTYDOC) && !defined(MULIB_PRETTYDOC_IMPLEMENTED)
#define MULIB_PRETTYDOC_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_pretty_node_ {
 mulib_pretty_kind_t kind;
 mulib_pretty_id_t a,b;
 size_t amount;
 char *text;
} mulib_pretty_node_;
struct mulib_prettydoc {
 mulib_pretty_node_ *nodes;
 size_t count,cap;
};
/* Decode one non-NUL scalar, rejecting invalid XML controls too. */
static int mulib_pretty_scalar_(const unsigned char **cursor,uint32_t *value){
  const unsigned char *s=*cursor;
  uint32_t cp;unsigned count;
  if(*s<0x80){cp=*s++;count=0;}
  else if(*s>=0xc2 && *s<=0xdf){cp=*s++&0x1f;count=1;}
  else if(*s>=0xe0 && *s<=0xef){cp=*s++&0x0f;count=2;}
  else if(*s>=0xf0 && *s<=0xf4){cp=*s++&0x07;count=3;}
  else return 0;
  for(unsigned i=0;i<count;i++){
   if((*s&0xc0)!=0x80 || !*s)return 0;
   cp=(cp<<6)|(*s++&0x3f);
  }
  if((count==1&&cp<0x80)||(count==2&&cp<0x800)||
     (count==3&&cp<0x10000)||(cp>=0xd800&&cp<=0xdfff)||cp>0x10ffff)return 0;
  if((cp<0x20 && cp!='\t' && cp!='\n' && cp!='\r')||cp==0xfffe||cp==0xffff)return 0;
  *cursor=s;*value=cp;
 return 1;
}
static int mulib_pretty_utf8_(const unsigned char *s){
 uint32_t cp;
 while(*s)if(!mulib_pretty_scalar_(&s,&cp))return 0;
 return 1;
}
/* BEGIN GENERATED WIDTH TABLE */
/* Unicode 16.0.0; see tests/generate_pretty_width.py. */
static const struct {uint32_t first,last;unsigned width;} mulib_pretty_widths_[]={
 {0xad,0xad,0}, {0x300,0x36f,0}, {0x483,0x489,0}, {0x591,0x5bd,0},
 {0x5bf,0x5bf,0}, {0x5c1,0x5c2,0}, {0x5c4,0x5c5,0}, {0x5c7,0x5c7,0},
 {0x600,0x605,0}, {0x610,0x61a,0}, {0x61c,0x61c,0}, {0x64b,0x65f,0},
 {0x670,0x670,0}, {0x6d6,0x6dd,0}, {0x6df,0x6e4,0}, {0x6e7,0x6e8,0},
 {0x6ea,0x6ed,0}, {0x70f,0x70f,0}, {0x711,0x711,0}, {0x730,0x74a,0},
 {0x7a6,0x7b0,0}, {0x7eb,0x7f3,0}, {0x7fd,0x7fd,0}, {0x816,0x819,0},
 {0x81b,0x823,0}, {0x825,0x827,0}, {0x829,0x82d,0}, {0x859,0x85b,0},
 {0x890,0x891,0}, {0x897,0x89f,0}, {0x8ca,0x902,0}, {0x93a,0x93a,0},
 {0x93c,0x93c,0}, {0x941,0x948,0}, {0x94d,0x94d,0}, {0x951,0x957,0},
 {0x962,0x963,0}, {0x981,0x981,0}, {0x9bc,0x9bc,0}, {0x9c1,0x9c4,0},
 {0x9cd,0x9cd,0}, {0x9e2,0x9e3,0}, {0x9fe,0x9fe,0}, {0xa01,0xa02,0},
 {0xa3c,0xa3c,0}, {0xa41,0xa42,0}, {0xa47,0xa48,0}, {0xa4b,0xa4d,0},
 {0xa51,0xa51,0}, {0xa70,0xa71,0}, {0xa75,0xa75,0}, {0xa81,0xa82,0},
 {0xabc,0xabc,0}, {0xac1,0xac5,0}, {0xac7,0xac8,0}, {0xacd,0xacd,0},
 {0xae2,0xae3,0}, {0xafa,0xaff,0}, {0xb01,0xb01,0}, {0xb3c,0xb3c,0},
 {0xb3f,0xb3f,0}, {0xb41,0xb44,0}, {0xb4d,0xb4d,0}, {0xb55,0xb56,0},
 {0xb62,0xb63,0}, {0xb82,0xb82,0}, {0xbc0,0xbc0,0}, {0xbcd,0xbcd,0},
 {0xc00,0xc00,0}, {0xc04,0xc04,0}, {0xc3c,0xc3c,0}, {0xc3e,0xc40,0},
 {0xc46,0xc48,0}, {0xc4a,0xc4d,0}, {0xc55,0xc56,0}, {0xc62,0xc63,0},
 {0xc81,0xc81,0}, {0xcbc,0xcbc,0}, {0xcbf,0xcbf,0}, {0xcc6,0xcc6,0},
 {0xccc,0xccd,0}, {0xce2,0xce3,0}, {0xd00,0xd01,0}, {0xd3b,0xd3c,0},
 {0xd41,0xd44,0}, {0xd4d,0xd4d,0}, {0xd62,0xd63,0}, {0xd81,0xd81,0},
 {0xdca,0xdca,0}, {0xdd2,0xdd4,0}, {0xdd6,0xdd6,0}, {0xe31,0xe31,0},
 {0xe34,0xe3a,0}, {0xe47,0xe4e,0}, {0xeb1,0xeb1,0}, {0xeb4,0xebc,0},
 {0xec8,0xece,0}, {0xf18,0xf19,0}, {0xf35,0xf35,0}, {0xf37,0xf37,0},
 {0xf39,0xf39,0}, {0xf71,0xf7e,0}, {0xf80,0xf84,0}, {0xf86,0xf87,0},
 {0xf8d,0xf97,0}, {0xf99,0xfbc,0}, {0xfc6,0xfc6,0}, {0x102d,0x1030,0},
 {0x1032,0x1037,0}, {0x1039,0x103a,0}, {0x103d,0x103e,0}, {0x1058,0x1059,0},
 {0x105e,0x1060,0}, {0x1071,0x1074,0}, {0x1082,0x1082,0}, {0x1085,0x1086,0},
 {0x108d,0x108d,0}, {0x109d,0x109d,0}, {0x1100,0x115f,2}, {0x135d,0x135f,0},
 {0x1712,0x1714,0}, {0x1732,0x1733,0}, {0x1752,0x1753,0}, {0x1772,0x1773,0},
 {0x17b4,0x17b5,0}, {0x17b7,0x17bd,0}, {0x17c6,0x17c6,0}, {0x17c9,0x17d3,0},
 {0x17dd,0x17dd,0}, {0x180b,0x180f,0}, {0x1885,0x1886,0}, {0x18a9,0x18a9,0},
 {0x1920,0x1922,0}, {0x1927,0x1928,0}, {0x1932,0x1932,0}, {0x1939,0x193b,0},
 {0x1a17,0x1a18,0}, {0x1a1b,0x1a1b,0}, {0x1a56,0x1a56,0}, {0x1a58,0x1a5e,0},
 {0x1a60,0x1a60,0}, {0x1a62,0x1a62,0}, {0x1a65,0x1a6c,0}, {0x1a73,0x1a7c,0},
 {0x1a7f,0x1a7f,0}, {0x1ab0,0x1ace,0}, {0x1b00,0x1b03,0}, {0x1b34,0x1b34,0},
 {0x1b36,0x1b3a,0}, {0x1b3c,0x1b3c,0}, {0x1b42,0x1b42,0}, {0x1b6b,0x1b73,0},
 {0x1b80,0x1b81,0}, {0x1ba2,0x1ba5,0}, {0x1ba8,0x1ba9,0}, {0x1bab,0x1bad,0},
 {0x1be6,0x1be6,0}, {0x1be8,0x1be9,0}, {0x1bed,0x1bed,0}, {0x1bef,0x1bf1,0},
 {0x1c2c,0x1c33,0}, {0x1c36,0x1c37,0}, {0x1cd0,0x1cd2,0}, {0x1cd4,0x1ce0,0},
 {0x1ce2,0x1ce8,0}, {0x1ced,0x1ced,0}, {0x1cf4,0x1cf4,0}, {0x1cf8,0x1cf9,0},
 {0x1dc0,0x1dff,0}, {0x200b,0x200f,0}, {0x202a,0x202e,0}, {0x2060,0x2064,0},
 {0x2066,0x206f,0}, {0x20d0,0x20f0,0}, {0x231a,0x231b,2}, {0x2329,0x232a,2},
 {0x23e9,0x23ec,2}, {0x23f0,0x23f0,2}, {0x23f3,0x23f3,2}, {0x25fd,0x25fe,2},
 {0x2614,0x2615,2}, {0x2630,0x2637,2}, {0x2648,0x2653,2}, {0x267f,0x267f,2},
 {0x268a,0x268f,2}, {0x2693,0x2693,2}, {0x26a1,0x26a1,2}, {0x26aa,0x26ab,2},
 {0x26bd,0x26be,2}, {0x26c4,0x26c5,2}, {0x26ce,0x26ce,2}, {0x26d4,0x26d4,2},
 {0x26ea,0x26ea,2}, {0x26f2,0x26f3,2}, {0x26f5,0x26f5,2}, {0x26fa,0x26fa,2},
 {0x26fd,0x26fd,2}, {0x2705,0x2705,2}, {0x270a,0x270b,2}, {0x2728,0x2728,2},
 {0x274c,0x274c,2}, {0x274e,0x274e,2}, {0x2753,0x2755,2}, {0x2757,0x2757,2},
 {0x2795,0x2797,2}, {0x27b0,0x27b0,2}, {0x27bf,0x27bf,2}, {0x2b1b,0x2b1c,2},
 {0x2b50,0x2b50,2}, {0x2b55,0x2b55,2}, {0x2cef,0x2cf1,0}, {0x2d7f,0x2d7f,0},
 {0x2de0,0x2dff,0}, {0x2e80,0x2e99,2}, {0x2e9b,0x2ef3,2}, {0x2f00,0x2fd5,2},
 {0x2ff0,0x3029,2}, {0x302a,0x302d,0}, {0x302e,0x303e,2}, {0x3041,0x3096,2},
 {0x3099,0x309a,0}, {0x309b,0x30ff,2}, {0x3105,0x312f,2}, {0x3131,0x318e,2},
 {0x3190,0x31e5,2}, {0x31ef,0x321e,2}, {0x3220,0x3247,2}, {0x3250,0xa48c,2},
 {0xa490,0xa4c6,2}, {0xa66f,0xa672,0}, {0xa674,0xa67d,0}, {0xa69e,0xa69f,0},
 {0xa6f0,0xa6f1,0}, {0xa802,0xa802,0}, {0xa806,0xa806,0}, {0xa80b,0xa80b,0},
 {0xa825,0xa826,0}, {0xa82c,0xa82c,0}, {0xa8c4,0xa8c5,0}, {0xa8e0,0xa8f1,0},
 {0xa8ff,0xa8ff,0}, {0xa926,0xa92d,0}, {0xa947,0xa951,0}, {0xa960,0xa97c,2},
 {0xa980,0xa982,0}, {0xa9b3,0xa9b3,0}, {0xa9b6,0xa9b9,0}, {0xa9bc,0xa9bd,0},
 {0xa9e5,0xa9e5,0}, {0xaa29,0xaa2e,0}, {0xaa31,0xaa32,0}, {0xaa35,0xaa36,0},
 {0xaa43,0xaa43,0}, {0xaa4c,0xaa4c,0}, {0xaa7c,0xaa7c,0}, {0xaab0,0xaab0,0},
 {0xaab2,0xaab4,0}, {0xaab7,0xaab8,0}, {0xaabe,0xaabf,0}, {0xaac1,0xaac1,0},
 {0xaaec,0xaaed,0}, {0xaaf6,0xaaf6,0}, {0xabe5,0xabe5,0}, {0xabe8,0xabe8,0},
 {0xabed,0xabed,0}, {0xac00,0xd7a3,2}, {0xf900,0xfa6d,2}, {0xfa70,0xfad9,2},
 {0xfb1e,0xfb1e,0}, {0xfe00,0xfe0f,0}, {0xfe10,0xfe19,2}, {0xfe20,0xfe2f,0},
 {0xfe30,0xfe52,2}, {0xfe54,0xfe66,2}, {0xfe68,0xfe6b,2}, {0xfeff,0xfeff,0},
 {0xff01,0xff60,2}, {0xffe0,0xffe6,2}, {0xfff9,0xfffb,0}, {0x101fd,0x101fd,0},
 {0x102e0,0x102e0,0}, {0x10376,0x1037a,0}, {0x10a01,0x10a03,0}, {0x10a05,0x10a06,0},
 {0x10a0c,0x10a0f,0}, {0x10a38,0x10a3a,0}, {0x10a3f,0x10a3f,0}, {0x10ae5,0x10ae6,0},
 {0x10d24,0x10d27,0}, {0x10d69,0x10d6d,0}, {0x10eab,0x10eac,0}, {0x10efc,0x10eff,0},
 {0x10f46,0x10f50,0}, {0x10f82,0x10f85,0}, {0x11001,0x11001,0}, {0x11038,0x11046,0},
 {0x11070,0x11070,0}, {0x11073,0x11074,0}, {0x1107f,0x11081,0}, {0x110b3,0x110b6,0},
 {0x110b9,0x110ba,0}, {0x110bd,0x110bd,0}, {0x110c2,0x110c2,0}, {0x110cd,0x110cd,0},
 {0x11100,0x11102,0}, {0x11127,0x1112b,0}, {0x1112d,0x11134,0}, {0x11173,0x11173,0},
 {0x11180,0x11181,0}, {0x111b6,0x111be,0}, {0x111c9,0x111cc,0}, {0x111cf,0x111cf,0},
 {0x1122f,0x11231,0}, {0x11234,0x11234,0}, {0x11236,0x11237,0}, {0x1123e,0x1123e,0},
 {0x11241,0x11241,0}, {0x112df,0x112df,0}, {0x112e3,0x112ea,0}, {0x11300,0x11301,0},
 {0x1133b,0x1133c,0}, {0x11340,0x11340,0}, {0x11366,0x1136c,0}, {0x11370,0x11374,0},
 {0x113bb,0x113c0,0}, {0x113ce,0x113ce,0}, {0x113d0,0x113d0,0}, {0x113d2,0x113d2,0},
 {0x113e1,0x113e2,0}, {0x11438,0x1143f,0}, {0x11442,0x11444,0}, {0x11446,0x11446,0},
 {0x1145e,0x1145e,0}, {0x114b3,0x114b8,0}, {0x114ba,0x114ba,0}, {0x114bf,0x114c0,0},
 {0x114c2,0x114c3,0}, {0x115b2,0x115b5,0}, {0x115bc,0x115bd,0}, {0x115bf,0x115c0,0},
 {0x115dc,0x115dd,0}, {0x11633,0x1163a,0}, {0x1163d,0x1163d,0}, {0x1163f,0x11640,0},
 {0x116ab,0x116ab,0}, {0x116ad,0x116ad,0}, {0x116b0,0x116b5,0}, {0x116b7,0x116b7,0},
 {0x1171d,0x1171d,0}, {0x1171f,0x1171f,0}, {0x11722,0x11725,0}, {0x11727,0x1172b,0},
 {0x1182f,0x11837,0}, {0x11839,0x1183a,0}, {0x1193b,0x1193c,0}, {0x1193e,0x1193e,0},
 {0x11943,0x11943,0}, {0x119d4,0x119d7,0}, {0x119da,0x119db,0}, {0x119e0,0x119e0,0},
 {0x11a01,0x11a0a,0}, {0x11a33,0x11a38,0}, {0x11a3b,0x11a3e,0}, {0x11a47,0x11a47,0},
 {0x11a51,0x11a56,0}, {0x11a59,0x11a5b,0}, {0x11a8a,0x11a96,0}, {0x11a98,0x11a99,0},
 {0x11c30,0x11c36,0}, {0x11c38,0x11c3d,0}, {0x11c3f,0x11c3f,0}, {0x11c92,0x11ca7,0},
 {0x11caa,0x11cb0,0}, {0x11cb2,0x11cb3,0}, {0x11cb5,0x11cb6,0}, {0x11d31,0x11d36,0},
 {0x11d3a,0x11d3a,0}, {0x11d3c,0x11d3d,0}, {0x11d3f,0x11d45,0}, {0x11d47,0x11d47,0},
 {0x11d90,0x11d91,0}, {0x11d95,0x11d95,0}, {0x11d97,0x11d97,0}, {0x11ef3,0x11ef4,0},
 {0x11f00,0x11f01,0}, {0x11f36,0x11f3a,0}, {0x11f40,0x11f40,0}, {0x11f42,0x11f42,0},
 {0x11f5a,0x11f5a,0}, {0x13430,0x13440,0}, {0x13447,0x13455,0}, {0x1611e,0x16129,0},
 {0x1612d,0x1612f,0}, {0x16af0,0x16af4,0}, {0x16b30,0x16b36,0}, {0x16f4f,0x16f4f,0},
 {0x16f8f,0x16f92,0}, {0x16fe0,0x16fe3,2}, {0x16fe4,0x16fe4,0}, {0x16ff0,0x16ff1,2},
 {0x17000,0x187f7,2}, {0x18800,0x18cd5,2}, {0x18cff,0x18d08,2}, {0x1aff0,0x1aff3,2},
 {0x1aff5,0x1affb,2}, {0x1affd,0x1affe,2}, {0x1b000,0x1b122,2}, {0x1b132,0x1b132,2},
 {0x1b150,0x1b152,2}, {0x1b155,0x1b155,2}, {0x1b164,0x1b167,2}, {0x1b170,0x1b2fb,2},
 {0x1bc9d,0x1bc9e,0}, {0x1bca0,0x1bca3,0}, {0x1cf00,0x1cf2d,0}, {0x1cf30,0x1cf46,0},
 {0x1d167,0x1d169,0}, {0x1d173,0x1d182,0}, {0x1d185,0x1d18b,0}, {0x1d1aa,0x1d1ad,0},
 {0x1d242,0x1d244,0}, {0x1d300,0x1d356,2}, {0x1d360,0x1d376,2}, {0x1da00,0x1da36,0},
 {0x1da3b,0x1da6c,0}, {0x1da75,0x1da75,0}, {0x1da84,0x1da84,0}, {0x1da9b,0x1da9f,0},
 {0x1daa1,0x1daaf,0}, {0x1e000,0x1e006,0}, {0x1e008,0x1e018,0}, {0x1e01b,0x1e021,0},
 {0x1e023,0x1e024,0}, {0x1e026,0x1e02a,0}, {0x1e08f,0x1e08f,0}, {0x1e130,0x1e136,0},
 {0x1e2ae,0x1e2ae,0}, {0x1e2ec,0x1e2ef,0}, {0x1e4ec,0x1e4ef,0}, {0x1e5ee,0x1e5ef,0},
 {0x1e8d0,0x1e8d6,0}, {0x1e944,0x1e94a,0}, {0x1f004,0x1f004,2}, {0x1f0cf,0x1f0cf,2},
 {0x1f18e,0x1f18e,2}, {0x1f191,0x1f19a,2}, {0x1f200,0x1f202,2}, {0x1f210,0x1f23b,2},
 {0x1f240,0x1f248,2}, {0x1f250,0x1f251,2}, {0x1f260,0x1f265,2}, {0x1f300,0x1f320,2},
 {0x1f32d,0x1f335,2}, {0x1f337,0x1f37c,2}, {0x1f37e,0x1f393,2}, {0x1f3a0,0x1f3ca,2},
 {0x1f3cf,0x1f3d3,2}, {0x1f3e0,0x1f3f0,2}, {0x1f3f4,0x1f3f4,2}, {0x1f3f8,0x1f43e,2},
 {0x1f440,0x1f440,2}, {0x1f442,0x1f4fc,2}, {0x1f4ff,0x1f53d,2}, {0x1f54b,0x1f54e,2},
 {0x1f550,0x1f567,2}, {0x1f57a,0x1f57a,2}, {0x1f595,0x1f596,2}, {0x1f5a4,0x1f5a4,2},
 {0x1f5fb,0x1f64f,2}, {0x1f680,0x1f6c5,2}, {0x1f6cc,0x1f6cc,2}, {0x1f6d0,0x1f6d2,2},
 {0x1f6d5,0x1f6d7,2}, {0x1f6dc,0x1f6df,2}, {0x1f6eb,0x1f6ec,2}, {0x1f6f4,0x1f6fc,2},
 {0x1f7e0,0x1f7eb,2}, {0x1f7f0,0x1f7f0,2}, {0x1f90c,0x1f93a,2}, {0x1f93c,0x1f945,2},
 {0x1f947,0x1f9ff,2}, {0x1fa70,0x1fa7c,2}, {0x1fa80,0x1fa89,2}, {0x1fa8f,0x1fac6,2},
 {0x1face,0x1fadc,2}, {0x1fadf,0x1fae9,2}, {0x1faf0,0x1faf8,2}, {0x20000,0x2a6df,2},
 {0x2a700,0x2b739,2}, {0x2b740,0x2b81d,2}, {0x2b820,0x2cea1,2}, {0x2ceb0,0x2ebe0,2},
 {0x2ebf0,0x2ee5d,2}, {0x2f800,0x2fa1d,2}, {0x30000,0x3134a,2}, {0x31350,0x323af,2},
 {0xe0001,0xe0001,0}, {0xe0020,0xe007f,0}, {0xe0100,0xe01ef,0},
};
/* END GENERATED WIDTH TABLE */
static size_t mulib_pretty_width_(uint32_t cp){
 if(cp<0x80)return 1;
 size_t lo=0,hi=sizeof(mulib_pretty_widths_)/sizeof(*mulib_pretty_widths_);
 while(lo<hi){
  size_t mid=lo+(hi-lo)/2;
  if(cp<mulib_pretty_widths_[mid].first)hi=mid;
  else if(cp>mulib_pretty_widths_[mid].last)lo=mid+1;
  else return mulib_pretty_widths_[mid].width;
 }
 return 1;
}
static int mulib_pretty_space_(unsigned char c){return c==' '||(c>='\t'&&c<='\r');}
static char *mulib_pretty_dup_(const char *s) {
 size_t n; char *p;
 if(!s || (n=strlen(s))==SIZE_MAX)return NULL;
 p=(char*)malloc(n+1);if(p)memcpy(p,s,n+1);return p;
}
mulib_prettydoc_t *mulib_prettydoc_new(void){return (mulib_prettydoc_t*)calloc(1,sizeof(mulib_prettydoc_t));}
void mulib_prettydoc_free(mulib_prettydoc_t *d){
 if(!d)return;
 for(size_t i=0;i<d->count;i++)free(d->nodes[i].text);
 free(d->nodes);free(d);
}
static mulib_pretty_id_t mulib_pretty_add_(mulib_prettydoc_t *d,mulib_pretty_node_ n){
 if(!d || d->count >= UINT32_MAX)return MULIB_PRETTY_NONE;
 if(d->count==d->cap){
  size_t nc=d->cap?d->cap*2:32;
  if(nc<d->cap || nc>SIZE_MAX/sizeof(*d->nodes))return MULIB_PRETTY_NONE;
  mulib_pretty_node_ *p=(mulib_pretty_node_*)realloc(d->nodes,nc*sizeof(*p));
  if(!p)return MULIB_PRETTY_NONE;
  d->nodes=p;d->cap=nc;
 }
 d->nodes[d->count]=n;return (mulib_pretty_id_t)d->count++;
}
static int mulib_pretty_valid_(const mulib_prettydoc_t *d,mulib_pretty_id_t a){return d&&a!=MULIB_PRETTY_NONE&&a<d->count;}
static mulib_pretty_id_t mulib_pretty_string_(mulib_prettydoc_t *d,const char *s,mulib_pretty_kind_t k){
 if(!s || !mulib_pretty_utf8_((const unsigned char*)s))return MULIB_PRETTY_NONE;
 char *p=mulib_pretty_dup_(s);if(!p)return MULIB_PRETTY_NONE;
 mulib_pretty_node_ n={0};n.kind=k;n.text=p;
 mulib_pretty_id_t i=mulib_pretty_add_(d,n);if(i==MULIB_PRETTY_NONE)free(p);return i;
}
mulib_pretty_id_t mulib_pretty_text(mulib_prettydoc_t *d,const char *s){return mulib_pretty_string_(d,s,MULIB_PRETTY_TEXT);}
mulib_pretty_id_t mulib_pretty_fill(mulib_prettydoc_t *d,const char *s){return mulib_pretty_string_(d,s,MULIB_PRETTY_FILL);}
static mulib_pretty_id_t mulib_pretty_node1_(mulib_prettydoc_t *d,mulib_pretty_kind_t k,size_t amt,mulib_pretty_id_t a){
 if(!mulib_pretty_valid_(d,a))return MULIB_PRETTY_NONE;
 mulib_pretty_node_ n={0};n.kind=k;n.a=a;n.amount=amt;return mulib_pretty_add_(d,n);
}
static mulib_pretty_id_t mulib_pretty_node2_(mulib_prettydoc_t *d,mulib_pretty_kind_t k,mulib_pretty_id_t a,mulib_pretty_id_t b){
 if(!mulib_pretty_valid_(d,a)||!mulib_pretty_valid_(d,b))return MULIB_PRETTY_NONE;
 mulib_pretty_node_ n={0};n.kind=k;n.a=a;n.b=b;return mulib_pretty_add_(d,n);
}
mulib_pretty_id_t mulib_pretty_softline(mulib_prettydoc_t *d){mulib_pretty_node_ n={0};n.kind=MULIB_PRETTY_SOFTLINE;return mulib_pretty_add_(d,n);}
mulib_pretty_id_t mulib_pretty_hardline(mulib_prettydoc_t *d){mulib_pretty_node_ n={0};n.kind=MULIB_PRETTY_HARDLINE;return mulib_pretty_add_(d,n);}
mulib_pretty_id_t mulib_pretty_cat(mulib_prettydoc_t *d,mulib_pretty_id_t a,mulib_pretty_id_t b){return mulib_pretty_node2_(d,MULIB_PRETTY_CAT,a,b);}
mulib_pretty_id_t mulib_pretty_group(mulib_prettydoc_t *d,mulib_pretty_id_t a){return mulib_pretty_node1_(d,MULIB_PRETTY_GROUP,0,a);}
mulib_pretty_id_t mulib_pretty_nest(mulib_prettydoc_t *d,size_t n,mulib_pretty_id_t a){return mulib_pretty_node1_(d,MULIB_PRETTY_NEST,n,a);}
mulib_pretty_id_t mulib_pretty_align(mulib_prettydoc_t *d,mulib_pretty_id_t a){return mulib_pretty_node1_(d,MULIB_PRETTY_ALIGN,0,a);}
mulib_pretty_id_t mulib_pretty_choice(mulib_prettydoc_t *d,mulib_pretty_id_t a,mulib_pretty_id_t b){return mulib_pretty_node2_(d,MULIB_PRETTY_CHOICE,a,b);}
mulib_pretty_id_t mulib_pretty_annotate(mulib_prettydoc_t *d,mulib_pretty_style_t style,mulib_pretty_id_t a){
 if(style<0||style>MULIB_PRETTY_MATH_OPERATOR)return MULIB_PRETTY_NONE;
 return mulib_pretty_node1_(d,MULIB_PRETTY_ANNOTATE,(size_t)style,a);
}
mulib_pretty_id_t mulib_pretty_region(mulib_prettydoc_t *d,mulib_pretty_algorithm_t algorithm,mulib_pretty_id_t a){
 if(algorithm<0||algorithm>MULIB_PRETTY_SEARCH)return MULIB_PRETTY_NONE;
 return mulib_pretty_node1_(d,MULIB_PRETTY_REGION,(size_t)algorithm,a);
}
mulib_pretty_id_t mulib_pretty_paragraph_region(mulib_prettydoc_t *d,mulib_pretty_paragraph_t paragraph,mulib_pretty_id_t a){
 if(paragraph<0||paragraph>MULIB_PRETTY_MIN_RAGGEDNESS)return MULIB_PRETTY_NONE;
 return mulib_pretty_node1_(d,MULIB_PRETTY_PARAGRAPH_REGION,(size_t)paragraph,a);
}
void mulib_pretty_options_init(mulib_pretty_options_t *o){
  if(o){o->width=80;o->indent_limit=4096;o->max_output=4*1024*1024;o->algorithm=MULIB_PRETTY_FITS;o->paragraph=MULIB_PRETTY_WORD_GREEDY;o->search_group_limit=9;o->ribbon_width=0;}
}

typedef struct mulib_pretty_renderer_ {
 const mulib_prettydoc_t *doc;mulib_pretty_options_t options;
 size_t *work;
 mulib_pretty_layout_t out;
 size_t capacity,piece_capacity,column,line_indent,indent,breaks;
 double ragged,overflow;
 unsigned group_count,mask,group_cursor;
 int pending_soft;size_t pending_indent;
 int failed;
} mulib_pretty_renderer_;
static int mulib_pretty_visit_(size_t *work){
 if(!*work)return 0;
 --*work;return 1;
}
/* Scan either a Text line or a Fill word. Stop early when a fit probe goes
 * over budget, including for long runs of zero-width scalars. */
static size_t mulib_pretty_measure_(mulib_pretty_renderer_ *r,const unsigned char **s,
                                  size_t cap,int word){
 size_t width=0;
 while(**s && !(word?mulib_pretty_space_(**s):**s=='\n')){
  uint32_t cp;
  if(!mulib_pretty_visit_(r->work)||!mulib_pretty_scalar_(s,&cp)){
   r->failed=1;return 0;
  }
  size_t cells=mulib_pretty_width_(cp);
  if(cells>cap-width)return cap+1;
  width+=cells;
 }
 return width;
}
static int mulib_pretty_skip_space_(mulib_pretty_renderer_ *r,const unsigned char **s){
 while(mulib_pretty_space_(**s)){
  if(!mulib_pretty_visit_(r->work)){r->failed=1;return 0;}
  ++*s;
 }
 return 1;
}
static int mulib_pretty_piece_(mulib_pretty_renderer_ *r,const char *s,size_t n,mulib_pretty_style_t style){
 if(r->failed || n>r->options.max_output-r->out.length){r->failed=1;return 0;}
 size_t len=r->out.length;
 if(len+n+1>r->capacity){
  size_t cap=r->capacity?r->capacity:128;
  while(cap<len+n+1){if(cap>SIZE_MAX/2){cap=len+n+1;break;}cap*=2;}
  char *t=(char*)realloc(r->out.text,cap);if(!t){r->failed=1;return 0;}
  r->out.text=t;r->capacity=cap;
 }
 if(n)memcpy(r->out.text+len,s,n);
 r->out.length+=n;r->out.text[r->out.length]=0;
 if(n){
  if(r->out.count && r->out.pieces[r->out.count-1].style==style &&
     r->out.pieces[r->out.count-1].start+r->out.pieces[r->out.count-1].length==len){
    r->out.pieces[r->out.count-1].length+=n;
  }else{
   if(r->out.count==r->piece_capacity){
    size_t cap=r->piece_capacity?r->piece_capacity*2:16;
    if(cap<r->piece_capacity||cap>SIZE_MAX/sizeof(*r->out.pieces)){r->failed=1;return 0;}
    mulib_pretty_piece_t *p=(mulib_pretty_piece_t*)realloc(r->out.pieces,cap*sizeof(*p));
    if(!p){r->failed=1;return 0;}r->out.pieces=p;r->piece_capacity=cap;
   }
   r->out.pieces[r->out.count++]=(mulib_pretty_piece_t){len,n,style};
  }
 }
 return 1;
}
/* Nesting can change midway through a line. Start the ribbon after the
 * indentation actually emitted for that line, not the current nest level. */
static size_t mulib_pretty_line_limit_(const mulib_pretty_renderer_ *r,size_t line_indent){
  size_t w=r->options.width,ribbon=r->options.ribbon_width;
  if(ribbon && line_indent<w && ribbon<w-line_indent)return line_indent+ribbon;
  return w;
}
static size_t mulib_pretty_room_(const mulib_pretty_renderer_ *r,size_t column,size_t line_indent){
  size_t limit=mulib_pretty_line_limit_(r,line_indent);
  return column<limit?limit-column:0;
}
static int mulib_pretty_break_(mulib_pretty_renderer_ *r,size_t indent){
  size_t w=mulib_pretty_line_limit_(r,r->line_indent);size_t diff=r->column>=w?0:w-r->column;
 if(r->column>w)r->overflow+=r->column-w;
 r->ragged+=diff>4096?4096*4096:diff*diff;
 if(r->column>r->out.max_columns)r->out.max_columns=r->column;
 r->breaks++;r->column=0;r->out.lines++;
  if(!mulib_pretty_piece_(r,"\n",1,MULIB_PRETTY_NORMAL))return 0;
  if(indent>r->options.indent_limit){r->failed=1;return 0;}
  r->line_indent=indent;
 while(indent--){if(!mulib_pretty_piece_(r," ",1,MULIB_PRETTY_NORMAL))return 0;r->column++;}
 return 1;
}
/* Greedy mode defers a softline until the following text fragment is known. */
static int mulib_pretty_pending_(mulib_pretty_renderer_ *r,size_t next_width){
 if(!r->pending_soft)return 1;
 r->pending_soft=0;
  if(next_width<mulib_pretty_room_(r,r->column,r->line_indent)){
  r->column++;
  return mulib_pretty_piece_(r," ",1,MULIB_PRETTY_NORMAL);
 }
 return mulib_pretty_break_(r,r->pending_indent);
}
static int mulib_pretty_put_(mulib_pretty_renderer_ *r,const char *s,mulib_pretty_style_t sty){
 const unsigned char *p=(const unsigned char*)s,*q=p;
 for(;;){
  size_t width=mulib_pretty_measure_(r,&q,SIZE_MAX-1,0);
  if(r->failed || !mulib_pretty_pending_(r,width) ||
     !mulib_pretty_piece_(r,(const char*)p,(size_t)(q-p),sty))return 0;
  r->column+=width;
  if(!*q)return 1;
  if(!mulib_pretty_break_(r,r->indent))return 0;
  p=++q;
 }
}
/* Upper-bound flat width of a subtree, capped at 'remaining'; hardlines do not fit. */
static size_t mulib_pretty_flat_(mulib_pretty_renderer_ *r,mulib_pretty_id_t id,size_t cap,unsigned depth){
 if(!mulib_pretty_visit_(r->work)){r->failed=1;return cap+1;}
 if(depth>256)return cap+1;
 const mulib_pretty_node_ *n=&r->doc->nodes[id];size_t a,b;
 switch(n->kind){
 case MULIB_PRETTY_TEXT: {
  const unsigned char *s=(const unsigned char*)n->text;
  a=mulib_pretty_measure_(r,&s,cap,0);
  return *s=='\n'?cap+1:a;
 }
 case MULIB_PRETTY_FILL: {
  const unsigned char *s=(const unsigned char*)n->text;
  size_t width=0;int first=1;
  while(mulib_pretty_skip_space_(r,&s) && *s){
   if(!first){if(width==cap)return cap+1;width++;}
   a=mulib_pretty_measure_(r,&s,cap-width,1);
   if(r->failed||a>cap-width)return cap+1;
   width+=a;first=0;
  }
  return width;
 }
 case MULIB_PRETTY_SOFTLINE:return cap?1:cap+1;
 case MULIB_PRETTY_HARDLINE:return cap+1;
 case MULIB_PRETTY_CAT:
  a=mulib_pretty_flat_(r,n->a,cap,depth+1);
  if(r->failed||a>cap)return cap+1;
  b=mulib_pretty_flat_(r,n->b,cap-a,depth+1);return b>cap-a?cap+1:a+b;
 case MULIB_PRETTY_CHOICE:return mulib_pretty_flat_(r,n->a,cap,depth+1);
 default:return mulib_pretty_flat_(r,n->a,cap,depth+1);
 }
}
/* Word separator normalization: wrap runs of ASCII whitespace in a Fill node. */
static int mulib_pretty_fill_render_(mulib_pretty_renderer_ *r,const char *s,mulib_pretty_style_t style){
 typedef struct {const unsigned char *begin;size_t bytes,cells;} word_t;
 size_t capacity=16,count=0;word_t *words=(word_t*)malloc(capacity*sizeof(*words));
 if(!words)return 0;
 const unsigned char *p=(const unsigned char*)s;
 while(mulib_pretty_skip_space_(r,&p) && *p){
  if(count==capacity){
   if(capacity>=8192){free(words);return 0;}
   size_t nc=capacity*2;
   word_t *w=(word_t*)realloc(words,nc*sizeof(*w));
   if(!w){free(words);return 0;}words=w;capacity=nc;
  }
  const unsigned char *begin=p;
  size_t cells=mulib_pretty_measure_(r,&p,SIZE_MAX-1,1);
  if(r->failed)break;
  words[count++]=(word_t){begin,(size_t)(p-begin),cells};
 }
 if(r->failed || !mulib_pretty_pending_(r,count?words[0].cells:0)){free(words);return 0;}
 size_t *ends=(size_t*)malloc((count+1)*sizeof(*ends));if(!ends){free(words);return 0;}
 ends[count]=count;
 if(r->options.paragraph==MULIB_PRETTY_MIN_RAGGEDNESS && count && count<=512){
  double *cost=(double*)malloc((count+1)*sizeof(*cost));
  if(!cost){free(words);free(ends);return 0;}
  cost[count]=0;
  for(size_t i=count;i-->0;){
    size_t room=i==0?mulib_pretty_room_(r,r->column,r->line_indent):
                    mulib_pretty_room_(r,r->indent,r->indent);
    size_t width=0;cost[i]=1e300;ends[i]=i+1;
   for(size_t j=i;j<count;j++){
    if(!mulib_pretty_visit_(r->work)){free(cost);free(words);free(ends);return 0;}
    if(j>i)width++;
    if(width>SIZE_MAX-words[j].cells)break;
    width+=words[j].cells;
     if(width>room && j>i)break;
     size_t rest=width>=room?0:room-width;
    double c=(j+1==count?0.0:(double)rest*rest)+cost[j+1];
    if(c<cost[i]){cost[i]=c;ends[i]=j+1;}
   }
  }
  free(cost);
 }else{
  size_t i=0;
   while(i<count){size_t j=i;
    size_t room=i==0?mulib_pretty_room_(r,r->column,r->line_indent):
                     mulib_pretty_room_(r,r->indent,r->indent);
    while(j<count){
     if(j>i){
      if(words[j].cells>=room)break;
      room--;
     }
     room=words[j].cells<room?room-words[j].cells:0;j++;
   }
   ends[i]=j;i=j;
  }
 }
 for(size_t i=0;i<count;){
  size_t j=ends[i];if(i && !mulib_pretty_break_(r,r->indent))break;
  for(size_t k=i;k<j;k++){
   if(k>i){if(!mulib_pretty_piece_(r," ",1,style))break;r->column++;}
   if(!mulib_pretty_piece_(r,(const char*)words[k].begin,words[k].bytes,style))break;
   r->column+=words[k].cells;
  }
  if(r->failed)break;
  i=j;
 }
 free(words);free(ends);return !r->failed;
}
static int mulib_pretty_walk_(mulib_pretty_renderer_ *r,mulib_pretty_id_t id,int flat,
                              mulib_pretty_style_t style,unsigned depth){
 if(depth>256||!mulib_pretty_visit_(r->work)){r->failed=1;return 0;}
 const mulib_pretty_node_ *n=&r->doc->nodes[id];size_t old;
 switch(n->kind){
 case MULIB_PRETTY_TEXT:return mulib_pretty_put_(r,n->text,style);
 case MULIB_PRETTY_FILL:return mulib_pretty_fill_render_(r,n->text,style);
 case MULIB_PRETTY_SOFTLINE:
  if(flat){
   if(!mulib_pretty_pending_(r,0))return 0;
   r->column++;return mulib_pretty_piece_(r," ",1,style);
  }
  if(r->options.algorithm==MULIB_PRETTY_GREEDY){
   if(!mulib_pretty_pending_(r,0))return 0;
   r->pending_soft=1;r->pending_indent=r->indent;return 1;
  }
  return mulib_pretty_break_(r,r->indent);
 case MULIB_PRETTY_HARDLINE:r->pending_soft=0;return mulib_pretty_break_(r,r->indent);
 case MULIB_PRETTY_CAT:
  return mulib_pretty_walk_(r,n->a,flat,style,depth+1) && mulib_pretty_walk_(r,n->b,flat,style,depth+1);
 case MULIB_PRETTY_NEST:
  old=r->indent;
  if(old>r->options.indent_limit || n->amount>r->options.indent_limit-old){r->failed=1;return 0;}
  r->indent+=n->amount;
  {int ok=mulib_pretty_walk_(r,n->a,flat,style,depth+1);r->indent=old;return ok;}
 case MULIB_PRETTY_ALIGN:
  old=r->indent;r->indent=r->column;
  {int ok=mulib_pretty_walk_(r,n->a,flat,style,depth+1);r->indent=old;return ok;}
 case MULIB_PRETTY_ANNOTATE:return mulib_pretty_walk_(r,n->a,flat,(mulib_pretty_style_t)n->amount,depth+1);
 case MULIB_PRETTY_REGION: {
  mulib_pretty_algorithm_t previous=r->options.algorithm;
  r->options.algorithm=(mulib_pretty_algorithm_t)n->amount;
  int ok=mulib_pretty_walk_(r,n->a,flat,style,depth+1);
  r->options.algorithm=previous;return ok;
 }
 case MULIB_PRETTY_PARAGRAPH_REGION: {
  mulib_pretty_paragraph_t previous=r->options.paragraph;
  r->options.paragraph=(mulib_pretty_paragraph_t)n->amount;
  int ok=mulib_pretty_walk_(r,n->a,flat,style,depth+1);
  r->options.paragraph=previous;return ok;
 }
 case MULIB_PRETTY_GROUP: {
  int f=flat;
  if(!flat){
   if(r->options.algorithm==MULIB_PRETTY_GREEDY)f=0;
   else if(r->options.algorithm==MULIB_PRETTY_SEARCH && r->group_cursor<r->group_count)
     f=(r->mask>>(r->group_cursor++))&1;
    else {
     size_t remaining=mulib_pretty_room_(r,r->column,r->line_indent);
     f=mulib_pretty_flat_(r,n->a,remaining,0)<=remaining;
    }
  }
  return !r->failed && mulib_pretty_walk_(r,n->a,f,style,depth+1);
 }
 case MULIB_PRETTY_CHOICE: {
   size_t remaining=mulib_pretty_room_(r,r->column,r->line_indent);
  mulib_pretty_id_t choice=mulib_pretty_flat_(r,n->a,remaining,0)<=remaining?n->a:n->b;
  return !r->failed && mulib_pretty_walk_(r,choice,flat,style,depth+1);
 }
 default:r->failed=1;return 0;
 }
}
static size_t mulib_pretty_search_groups_(const mulib_prettydoc_t *d,
 mulib_pretty_id_t id,mulib_pretty_algorithm_t mode,unsigned depth,size_t *work){
 if(depth>256||!mulib_pretty_visit_(work))return 0;
 const mulib_pretty_node_ *n=&d->nodes[id];
 switch(n->kind){
 case MULIB_PRETTY_REGION:
  return mulib_pretty_search_groups_(d,n->a,(mulib_pretty_algorithm_t)n->amount,depth+1,work);
 case MULIB_PRETTY_GROUP:
  return (mode==MULIB_PRETTY_SEARCH?1:0)+mulib_pretty_search_groups_(d,n->a,mode,depth+1,work);
 case MULIB_PRETTY_CAT:case MULIB_PRETTY_CHOICE:
  return mulib_pretty_search_groups_(d,n->a,mode,depth+1,work)+mulib_pretty_search_groups_(d,n->b,mode,depth+1,work);
 case MULIB_PRETTY_NEST:case MULIB_PRETTY_ALIGN:case MULIB_PRETTY_ANNOTATE:
 case MULIB_PRETTY_PARAGRAPH_REGION:
  return mulib_pretty_search_groups_(d,n->a,mode,depth+1,work);
 default:return 0;
 }
}
static int mulib_pretty_run_(mulib_pretty_renderer_ *r,mulib_pretty_id_t root){
 r->out.lines=1;
 if(!mulib_pretty_walk_(r,root,0,MULIB_PRETTY_NORMAL,0))return 0;
 if(!mulib_pretty_pending_(r,0))return 0;
 if(r->column>r->out.max_columns)r->out.max_columns=r->column;
  size_t limit=mulib_pretty_line_limit_(r,r->line_indent);
  if(r->column>limit)r->overflow+=r->column-limit;
 return !r->failed;
}
void mulib_pretty_layout_free(mulib_pretty_layout_t *o){if(o){free(o->text);free(o->pieces);memset(o,0,sizeof(*o));}}
int mulib_pretty_layout(const mulib_prettydoc_t *doc,mulib_pretty_id_t root,
                       const mulib_pretty_options_t *options,mulib_pretty_layout_t *out){
 if(!mulib_pretty_valid_(doc,root)||!out||!options||!options->width||
    !options->max_output||options->max_output==SIZE_MAX||options->width>65536||options->indent_limit>options->max_output||
    options->algorithm>MULIB_PRETTY_SEARCH||options->paragraph>MULIB_PRETTY_MIN_RAGGEDNESS)return 0;
 unsigned count=0;
 /* Shared across traversal, width scans and paragraph trials. Depth alone
  * does not bound work for reused subtrees in the C builder. */
 size_t work=4u*1024u*1024u;
 size_t total=mulib_pretty_search_groups_(doc,root,options->algorithm,0,&work);
 if(!work)return 0;
 if(total<=options->search_group_limit && total<=12)count=(unsigned)total;
 unsigned permutations=count?1u<<count:1u;
 mulib_pretty_layout_t best={0};double best_cost=1e300;int found=0;
 for(unsigned mask=0;mask<permutations;mask++){
  mulib_pretty_renderer_ r={0};r.doc=doc;r.options=*options;r.work=&work;r.mask=mask;r.group_count=count;
  if(!mulib_pretty_run_(&r,root)){
   mulib_pretty_layout_free(&r.out);mulib_pretty_layout_free(&best);return 0;
  }
  double cost=(double)r.overflow*1000000.0+(double)r.ragged+(double)r.breaks*8.0;
  if(!found||cost<best_cost){
   mulib_pretty_layout_free(&best);best=r.out;best_cost=cost;found=1;
  }else mulib_pretty_layout_free(&r.out);
 }
 if(!found)return 0;
 /* Empty documents still have allocated, NUL-terminated output. */
 if(!best.text){best.text=(char*)malloc(1);if(!best.text)return 0;best.text[0]=0;}
 *out=best;return 1;
}
#ifdef __cplusplus
}
#endif
#endif
