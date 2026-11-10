/* MuLIB compiler tools: Deterministic byte-offset lexer for identifiers, integers, strings, comments and punctuation.
 * ISO C11 single-header module. Define MULIB_IMPL_LEXER in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_LEXER_H
#define MULIB_LEXER_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-lexical-token.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-lexical-token.h"
typedef struct {const char *text;
size_t length,offset;
} mulib_lexer_t;
void mulib_lexer_init(mulib_lexer_t *l,const char *source,size_t length);
int mulib_lexer_next(mulib_lexer_t *l,mulib_lexical_token_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_LEXER_H */
#if defined(MULIB_IMPL_LEXER) && !defined(MULIB_LEXER_IMPLEMENTED)
#define MULIB_LEXER_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_lexer_alpha_(unsigned c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_';
}
static int mulib_lexer_digit_(unsigned c){return c>='0'&&c<='9';
}
void mulib_lexer_init(mulib_lexer_t *l,const char *s,size_t n){if(l){l->text=s;
l->length=s?n:0;
l->offset=0;
}}
int mulib_lexer_next(mulib_lexer_t *l,mulib_lexical_token_t *out){
 if(!l||!out||(!l->text&&l->length)||l->offset>l->length)return -1;
 const char *s=l->text;
size_t n=l->length,p=l->offset;
 for(;;){while(p<n&&(s[p]==' '||s[p]=='\t'||s[p]=='\n'||s[p]=='\r'))p++;
  if(p+1<n&&s[p]=='/'&&s[p+1]=='/'){p+=2;
while(p<n&&s[p]!='\n')p++;
continue;
}
  if(p+1<n&&s[p]=='/'&&s[p+1]=='*'){size_t a=p;
p+=2;
while(p+1<n&&!(s[p]=='*'&&s[p+1]=='/'))p++;
if(p+1>=n){l->offset=n;
*out=(mulib_lexical_token_t){MULIB_TOKEN_ERROR,s+a,n-a,a};
return 1;
}p+=2;
continue;
}break;
}
 if(p==n){l->offset=n;
*out=(mulib_lexical_token_t){MULIB_TOKEN_EOF,NULL,0,n};
return 0;
}
 size_t b=p;
unsigned c=(unsigned char)s[p++];
mulib_token_kind_t kind=MULIB_TOKEN_OPERATOR;
 if(mulib_lexer_alpha_(c)){kind=MULIB_TOKEN_IDENTIFIER;
while(p<n&&(mulib_lexer_alpha_((unsigned char)s[p])||mulib_lexer_digit_((unsigned char)s[p])))p++;
}
 else if(mulib_lexer_digit_(c)){kind=MULIB_TOKEN_INTEGER;
while(p<n&&mulib_lexer_digit_((unsigned char)s[p]))p++;
}
 else if(c=='"'||c=='\''){kind=MULIB_TOKEN_STRING;
int escaped=0,closed=0;
while(p<n){unsigned d=(unsigned char)s[p++];
if(escaped){escaped=0;
continue;
}if(d=='\\'){escaped=1;
continue;
}if(d==c){closed=1;
break;
}}if(!closed)kind=MULIB_TOKEN_ERROR;
}
 else if(p<n&&((c=='='&&s[p]=='=')||(c=='!'&&s[p]=='=')||(c=='<'&&s[p]=='=')||(c=='>'&&s[p]=='=')||(c=='&'&&s[p]=='&')||(c=='|'&&s[p]=='|')))p++;
 l->offset=p;
*out=(mulib_lexical_token_t){kind,s+b,p-b,b};
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
