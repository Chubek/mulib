/** @file mulib-texmouth.h
 * @brief TeX-like borrowed token scanning: control words, braces, spaces and comments.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_TEXMOUTH once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-texmouth.md for scope and limitations.
 */
#ifndef MULIB_TEXMOUTH_H
#define MULIB_TEXMOUTH_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_texmouth_kind { mulib_texmouth_END_OF_INPUT=0,mulib_texmouth_TEXT=1,mulib_texmouth_CONTROL=2,mulib_texmouth_OPEN=3,mulib_texmouth_CLOSE=4,mulib_texmouth_WHITESPACE=5,mulib_texmouth_COMMENT=6 } mulib_texmouth_kind_t;
typedef struct mulib_texmouth_token { mulib_texmouth_kind_t kind;size_t offset,length; } mulib_texmouth_token_t;
typedef struct mulib_texmouth_scanner { const char *source;size_t size,position; } mulib_texmouth_scanner_t;
int mulib_texmouth_init(mulib_texmouth_scanner_t *s,const char *input,size_t length);
int mulib_texmouth_next(mulib_texmouth_scanner_t *s,mulib_texmouth_token_t *token);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TEXMOUTH_H */
#if defined(MULIB_IMPL_TEXMOUTH) && !defined(MULIB_TEXMOUTH_H_IMPLEMENTED)
#define MULIB_TEXMOUTH_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_texmouth_init(mulib_texmouth_scanner_t *s,const char *p,size_t n){if(!s||(!p&&n))return 0;s->source=p;s->size=n;s->position=0;return 1;}
int mulib_texmouth_next(mulib_texmouth_scanner_t *s,mulib_texmouth_token_t *t){if(!s||!t)return -1;if(s->position>=s->size){*t=(mulib_texmouth_token_t){mulib_texmouth_END_OF_INPUT,s->position,0};return 0;}size_t i=s->position,j=i+1;unsigned char c=(unsigned char)s->source[i];mulib_texmouth_kind_t k=mulib_texmouth_TEXT;
 if(c=='\\'){if(j>=s->size)return -1;k=mulib_texmouth_CONTROL;unsigned char d=(unsigned char)s->source[j];if(isalpha(d)){while(j<s->size&&isalpha((unsigned char)s->source[j]))j++;}else j++;}
 else if(c=='{')k=mulib_texmouth_OPEN;else if(c=='}')k=mulib_texmouth_CLOSE;
 else if(c=='%'){k=mulib_texmouth_COMMENT;while(j<s->size&&s->source[j]!='\n')j++;}
 else if(isspace(c)){k=mulib_texmouth_WHITESPACE;while(j<s->size&&isspace((unsigned char)s->source[j]))j++;}
 else {while(j<s->size){unsigned char d=(unsigned char)s->source[j];if(d=='\\'||d=='{'||d=='}'||d=='%'||isspace(d))break;j++;}}
 *t=(mulib_texmouth_token_t){k,i,j-i};s->position=j;return 1;}
#ifdef __cplusplus
}
#endif
#endif
