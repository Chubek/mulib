/* MuLIB compiler tools: Length-aware token kinds and borrowed lexeme comparisons.
 * ISO C11 single-header module. Define MULIB_IMPL_LEXICAL_TOKEN in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_LEXICAL_TOKEN_H
#define MULIB_LEXICAL_TOKEN_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum {MULIB_TOKEN_EOF,MULIB_TOKEN_IDENTIFIER,MULIB_TOKEN_INTEGER,MULIB_TOKEN_STRING,MULIB_TOKEN_OPERATOR,MULIB_TOKEN_ERROR} mulib_token_kind_t;
typedef struct {mulib_token_kind_t kind;
const char *start;
size_t length,offset;
} mulib_lexical_token_t;
int mulib_lexical_token_is(const mulib_lexical_token_t *t,const char *literal);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_LEXICAL_TOKEN_H */
#if defined(MULIB_IMPL_LEXICAL_TOKEN) && !defined(MULIB_LEXICAL_TOKEN_IMPLEMENTED)
#define MULIB_LEXICAL_TOKEN_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_lexical_token_is(const mulib_lexical_token_t *t,const char *s){if(!t||!s)return 0;
size_t n=strlen(s);
return n==t->length&&(!n||(t->start&&memcmp(t->start,s,n)==0));
}
#ifdef __cplusplus
}
#endif
#endif
