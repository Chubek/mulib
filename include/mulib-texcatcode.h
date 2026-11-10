/** @file mulib-texcatcode.h
 * @brief Configurable ASCII TeX-like category codes (initialization and assignment).
 * ISO C11, no third-party libraries. Define MULIB_IMPL_TEXCATCODE once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-texcatcode.md for scope and limitations.
 */
#ifndef MULIB_TEXCATCODE_H
#define MULIB_TEXCATCODE_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { mulib_texcatcode_ESCAPE=0,mulib_texcatcode_BEGIN=1,mulib_texcatcode_END=2,mulib_texcatcode_SPACE=10,mulib_texcatcode_LETTER=11,mulib_texcatcode_OTHER=12,mulib_texcatcode_COMMENT=14 };
typedef struct mulib_texcatcode_table { unsigned char category[256]; } mulib_texcatcode_table_t;
void mulib_texcatcode_init(mulib_texcatcode_table_t *table);
void mulib_texcatcode_set(mulib_texcatcode_table_t *table,unsigned byte,unsigned category);
unsigned mulib_texcatcode_get(const mulib_texcatcode_table_t *table,unsigned byte);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TEXCATCODE_H */
#if defined(MULIB_IMPL_TEXCATCODE) && !defined(MULIB_TEXCATCODE_H_IMPLEMENTED)
#define MULIB_TEXCATCODE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
void mulib_texcatcode_init(mulib_texcatcode_table_t *t){if(!t)return;for(unsigned i=0;i<256;i++)t->category[i]=mulib_texcatcode_OTHER;for(unsigned c='A';c<='Z';c++)t->category[c]=mulib_texcatcode_LETTER;for(unsigned c='a';c<='z';c++)t->category[c]=mulib_texcatcode_LETTER;t->category['\\']=mulib_texcatcode_ESCAPE;t->category['{']=mulib_texcatcode_BEGIN;t->category['}']=mulib_texcatcode_END;t->category[' ']=t->category['\t']=t->category['\n']=mulib_texcatcode_SPACE;t->category['%']=mulib_texcatcode_COMMENT;}
void mulib_texcatcode_set(mulib_texcatcode_table_t *t,unsigned byte,unsigned cat){if(t&&byte<256&&cat<=15)t->category[byte]=(unsigned char)cat;}
unsigned mulib_texcatcode_get(const mulib_texcatcode_table_t *t,unsigned byte){return t&&byte<256?t->category[byte]:mulib_texcatcode_OTHER;}
#ifdef __cplusplus
}
#endif
#endif
