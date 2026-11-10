/* MuLIB compiler tools: Checked signed 64-bit evaluation of Pratt arithmetic and comparisons.
 * ISO C11 single-header module. Define MULIB_IMPL_CONSTEVAL in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_CONSTEVAL_H
#define MULIB_CONSTEVAL_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-parser-pratt.h"
#include "mulib-symboltable.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-parser-pratt.h"
#include "mulib-symboltable.h"
/* Symbol values are interpreted as two-complement int64 integers. */
int mulib_consteval_eval(const mulib_parser_pratt_t *tree,const mulib_symboltable_t *scope,int64_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CONSTEVAL_H */
#if defined(MULIB_IMPL_CONSTEVAL) && !defined(MULIB_CONSTEVAL_IMPLEMENTED)
#define MULIB_CONSTEVAL_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_consteval_add_(int64_t a,int64_t b,int64_t *o){if((b>0&&a>INT64_MAX-b)||(b<0&&a<INT64_MIN-b))return 0;
*o=a+b;
return 1;
}
static int mulib_consteval_sub_(int64_t a,int64_t b,int64_t *o){if((b<0&&a>INT64_MAX+b)||(b>0&&a<INT64_MIN+b))return 0;
*o=a-b;
return 1;
}
static int mulib_consteval_mul_(int64_t a,int64_t b,int64_t *o){if(a==0||b==0){*o=0;
return 1;
}if(a==-1&&b==INT64_MIN)return 0;
if(b==-1&&a==INT64_MIN)return 0;
 if(a>0){if((b>0&&a>INT64_MAX/b)||(b<0&&b<INT64_MIN/a))return 0;
}
 else {if((b>0&&a<INT64_MIN/b)||(b<0&&a<INT64_MAX/b))return 0;
}*o=a*b;
return 1;
}
static int mulib_consteval_rec_(const mulib_parser_pratt_t *t,const mulib_symboltable_t *scope,size_t id,size_t depth,int64_t *out){
 if(!t||id>=t->count||depth>128)return 0;
const mulib_pratt_node_t *n=t->nodes+id;
int64_t a,b;
 if(n->opcode==MULIB_PRATT_LITERAL){*out=n->literal;
return 1;
}
 if(n->opcode==MULIB_PRATT_NAME){uint64_t u;
if(!scope||!mulib_symboltable_lookup(scope,n->name,&u))return 0;
*out=u<=(uint64_t)INT64_MAX?(int64_t)u:INT64_MIN+(int64_t)(u-((uint64_t)INT64_MAX+1));
return 1;
}
 if(!mulib_consteval_rec_(t,scope,n->left,depth+1,&a))return 0;
 if(n->opcode==MULIB_PRATT_NEG){if(a==INT64_MIN)return 0;
*out=-a;
return 1;
}
 if(!mulib_consteval_rec_(t,scope,n->right,depth+1,&b))return 0;
 switch(n->opcode){case MULIB_PRATT_ADD:return mulib_consteval_add_(a,b,out);
 case MULIB_PRATT_SUB:return mulib_consteval_sub_(a,b,out);
 case MULIB_PRATT_MUL:return mulib_consteval_mul_(a,b,out);
 case MULIB_PRATT_DIV:if(!b||(a==INT64_MIN&&b==-1))return 0;
*out=a/b;
return 1;
 case MULIB_PRATT_MOD:if(!b||(a==INT64_MIN&&b==-1))return 0;
*out=a%b;
return 1;
 case MULIB_PRATT_EQ:*out=a==b;
return 1;
 case MULIB_PRATT_LT:*out=a<b;
return 1;
 default:return 0;
}
}
int mulib_consteval_eval(const mulib_parser_pratt_t *t,const mulib_symboltable_t *scope,int64_t *out){if(!t||!out||t->root>=t->count)return 0;
int64_t value;
if(!mulib_consteval_rec_(t,scope,t->root,0,&value))return 0;
*out=value;
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
