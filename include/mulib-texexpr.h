/** @file mulib-texexpr.h
 * @brief Bounded recursive-descent arithmetic expressions with +,-,*,/,unary signs, parentheses.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_TEXEXPR once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-texexpr.md for scope and limitations.
 */
#ifndef MULIB_TEXEXPR_H
#define MULIB_TEXEXPR_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_texexpr_evaluate(const char *expression,double *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TEXEXPR_H */
#if defined(MULIB_IMPL_TEXEXPR) && !defined(MULIB_TEXEXPR_H_IMPLEMENTED)
#define MULIB_TEXEXPR_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_texexpr_parser { const char *p;size_t steps;int ok; } mulib_texexpr_parser_t;
static void mulib_texexpr_skip_(mulib_texexpr_parser_t *s){while(isspace((unsigned char)*s->p))s->p++;}
static double mulib_texexpr_expr_(mulib_texexpr_parser_t *s,unsigned depth);
static double mulib_texexpr_factor_(mulib_texexpr_parser_t *s,unsigned depth){if(depth>64||++s->steps>4096){s->ok=0;return 0;}mulib_texexpr_skip_(s);int neg=0;while(*s->p=='+'||*s->p=='-'){neg^=(*s->p=='-');s->p++;mulib_texexpr_skip_(s);}double x=0;if(*s->p=='('){s->p++;x=mulib_texexpr_expr_(s,depth+1);mulib_texexpr_skip_(s);if(*s->p!=')')s->ok=0;else s->p++;}else{char *end=NULL;x=strtod(s->p,&end);if(end==s->p)s->ok=0;else s->p=end;}return neg?-x:x;}
static double mulib_texexpr_term_(mulib_texexpr_parser_t *s,unsigned depth){double x=mulib_texexpr_factor_(s,depth);while(s->ok){mulib_texexpr_skip_(s);char op=*s->p;if(op!='*'&&op!='/')break;s->p++;double y=mulib_texexpr_factor_(s,depth);if(op=='/'&&y==0){s->ok=0;break;}x=op=='*'?x*y:x/y;if(!isfinite(x)){s->ok=0;break;}}return x;}
static double mulib_texexpr_expr_(mulib_texexpr_parser_t *s,unsigned depth){double x=mulib_texexpr_term_(s,depth);while(s->ok){mulib_texexpr_skip_(s);char op=*s->p;if(op!='+'&&op!='-')break;s->p++;double y=mulib_texexpr_term_(s,depth);x=op=='+'?x+y:x-y;if(!isfinite(x)){s->ok=0;break;}}return x;}
int mulib_texexpr_evaluate(const char *str,double *out){if(!str||!out)return 0;mulib_texexpr_parser_t p={str,0,1};double v=mulib_texexpr_expr_(&p,0);mulib_texexpr_skip_(&p);if(!p.ok||*p.p||!isfinite(v))return 0;*out=v;return 1;}
#ifdef __cplusplus
}
#endif
#endif
