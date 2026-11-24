/* MuLIB compiler tools: Ordered longest-match literal and byte-class lexer rules.
 * ISO C11 single-header module. Define MULIB_IMPL_LEXGEN in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_LEXGEN_H
#define MULIB_LEXGEN_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum {MULIB_LEXGEN_LITERAL,MULIB_LEXGEN_DIGITS,MULIB_LEXGEN_IDENT,MULIB_LEXGEN_WHITESPACE} mulib_lexgen_rule_kind_t;
typedef struct {mulib_lexgen_rule_kind_t kind;
const char *literal;
int token;
} mulib_lexgen_rule_t;
/* 1=match; 0=no match; -1=invalid. Tie selects first rule. */
int mulib_lexgen_match(const mulib_lexgen_rule_t *rules,size_t count,const char *text,size_t length,size_t *matched,int *token);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_LEXGEN_H */
#if defined(MULIB_IMPL_LEXGEN) && !defined(MULIB_LEXGEN_IMPLEMENTED)
#define MULIB_LEXGEN_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_lexgen_alpha_(unsigned c){return(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_';
}
int mulib_lexgen_match(const mulib_lexgen_rule_t *r,size_t count,const char *s,size_t n,size_t *matched,int *token){
 if((!r&&count)||(!s&&n)||!matched||!token)return -1;
size_t best=0;
int id=0;
 for(size_t i=0;i<count;i++){size_t k=0;
switch(r[i].kind){
 case MULIB_LEXGEN_LITERAL:if(!r[i].literal)return -1;
k=strlen(r[i].literal);
if(k>n||!k||memcmp(s,r[i].literal,k))k=0;
break;
 case MULIB_LEXGEN_DIGITS:while(k<n&&s[k]>='0'&&s[k]<='9')k++;
break;
 case MULIB_LEXGEN_IDENT:if(n&&mulib_lexgen_alpha_((unsigned char)s[0])){k=1;
while(k<n&&(mulib_lexgen_alpha_((unsigned char)s[k])||(s[k]>='0'&&s[k]<='9')))k++;
}break;
 case MULIB_LEXGEN_WHITESPACE:while(k<n&&(s[k]==' '||s[k]=='\t'||s[k]=='\n'||s[k]=='\r'))k++;
break;
 default:return -1;
}
 if(k>best){best=k;
id=r[i].token;
}}
 if(!best)return 0;
*matched=best;
*token=id;
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
