/* MuLIB compiler tools: Bounded Pratt expression parser with unary operators, precedence, parenthesized expressions and symbols.
 * ISO C11 single-header module. Define MULIB_IMPL_PARSER_PRATT in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_PARSER_PRATT_H
#define MULIB_PARSER_PRATT_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-lexer.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-lexer.h"
typedef enum {MULIB_PRATT_LITERAL,MULIB_PRATT_NAME,MULIB_PRATT_NEG,MULIB_PRATT_ADD,MULIB_PRATT_SUB,MULIB_PRATT_MUL,MULIB_PRATT_DIV,MULIB_PRATT_MOD,MULIB_PRATT_EQ,MULIB_PRATT_LT} mulib_pratt_op_t;
typedef struct {mulib_pratt_op_t opcode;
int64_t literal;
size_t left,right,start,length;
char name[64];
} mulib_pratt_node_t;
typedef struct {mulib_pratt_node_t *nodes;
size_t count,capacity,root;
} mulib_parser_pratt_t;
/* 1 success; 0 malformed/overflow/OOM; success replaces only empty output. */
int mulib_parser_pratt_parse(const char *source,size_t length,mulib_parser_pratt_t *out,size_t *error_offset);
void mulib_parser_pratt_free(mulib_parser_pratt_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PARSER_PRATT_H */
#if defined(MULIB_IMPL_PARSER_PRATT) && !defined(MULIB_PARSER_PRATT_IMPLEMENTED)
#define MULIB_PARSER_PRATT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {mulib_lexer_t lex;
mulib_lexical_token_t token;
mulib_parser_pratt_t tree;
size_t error;
} mulib_pratt_state_;
static int mulib_pratt_next_(mulib_pratt_state_ *p){int r=mulib_lexer_next(&p->lex,&p->token);
if(r<0||p->token.kind==MULIB_TOKEN_ERROR){p->error=p->token.offset;
return 0;
}return 1;
}
static int mulib_pratt_node_(mulib_pratt_state_ *p,mulib_pratt_node_t n,size_t *index){
 if(p->tree.count>=65536)return 0;
 if(p->tree.count==p->tree.capacity){size_t cap=p->tree.capacity?p->tree.capacity*2:16;
if(cap>65536)cap=65536;
void *x=realloc(p->tree.nodes,cap*sizeof(*p->tree.nodes));
if(!x)return 0;
p->tree.nodes=(mulib_pratt_node_t*)x;
p->tree.capacity=cap;
}
 *index=p->tree.count;
p->tree.nodes[p->tree.count++]=n;
return 1;
}
static int mulib_pratt_number_(const mulib_lexical_token_t *t,int64_t *out){uint64_t v=0;
for(size_t j=0;j<t->length;j++){unsigned d=(unsigned)(t->start[j]-'0');
if(d>9||v>((uint64_t)INT64_MAX-d)/10)return 0;
v=v*10+d;
}*out=(int64_t)v;
return 1;
}
static int mulib_pratt_prec_(const mulib_lexical_token_t *t,mulib_pratt_op_t *o){if(t->kind!=MULIB_TOKEN_OPERATOR)return 0;
 if(mulib_lexical_token_is(t,"==")){*o=MULIB_PRATT_EQ;
return 1;
}
 if(mulib_lexical_token_is(t,"<")){*o=MULIB_PRATT_LT;
return 2;
}
 if(mulib_lexical_token_is(t,"+")){*o=MULIB_PRATT_ADD;
return 3;
}
 if(mulib_lexical_token_is(t,"-")){*o=MULIB_PRATT_SUB;
return 3;
}
 if(mulib_lexical_token_is(t,"*")){*o=MULIB_PRATT_MUL;
return 4;
}
 if(mulib_lexical_token_is(t,"/")){*o=MULIB_PRATT_DIV;
return 4;
}
 if(mulib_lexical_token_is(t,"%")){*o=MULIB_PRATT_MOD;
return 4;
}return 0;
}
static int mulib_pratt_expr_(mulib_pratt_state_ *p,int minp,size_t depth,size_t *id){
 if(depth>=128)return 0;
size_t left=SIZE_MAX;
mulib_pratt_node_t node;
memset(&node,0,sizeof(node));
node.left=node.right=SIZE_MAX;
node.start=p->token.offset;
node.length=p->token.length;
 if(p->token.kind==MULIB_TOKEN_INTEGER){node.opcode=MULIB_PRATT_LITERAL;
if(!mulib_pratt_number_(&p->token,&node.literal)||!mulib_pratt_node_(p,node,&left)||!mulib_pratt_next_(p))return 0;
}
 else if(p->token.kind==MULIB_TOKEN_IDENTIFIER){node.opcode=MULIB_PRATT_NAME;
if(p->token.length>=sizeof(node.name))return 0;
memcpy(node.name,p->token.start,p->token.length);
node.name[p->token.length]=0;
if(!mulib_pratt_node_(p,node,&left)||!mulib_pratt_next_(p))return 0;
}
 else if(mulib_lexical_token_is(&p->token,"+")){
if(!mulib_pratt_next_(p)||!mulib_pratt_expr_(p,5,depth+1,&left))return 0;
}
 else if(mulib_lexical_token_is(&p->token,"-")){node.opcode=MULIB_PRATT_NEG;
if(!mulib_pratt_next_(p)||!mulib_pratt_expr_(p,5,depth+1,&node.left)||!mulib_pratt_node_(p,node,&left))return 0;
}
 else if(mulib_lexical_token_is(&p->token,"(")){if(!mulib_pratt_next_(p)||!mulib_pratt_expr_(p,1,depth+1,&left)||!mulib_lexical_token_is(&p->token,")")||!mulib_pratt_next_(p))return 0;
}
 else return 0;
 for(;;){mulib_pratt_op_t op;
int prec=mulib_pratt_prec_(&p->token,&op);
if(!prec||prec<minp)return (*id=left),1;
 node=(mulib_pratt_node_t){0};
node.opcode=op;
node.left=left;
node.right=SIZE_MAX;
node.start=p->token.offset;
node.length=p->token.length;
 if(!mulib_pratt_next_(p)||!mulib_pratt_expr_(p,prec+1,depth+1,&node.right)||!mulib_pratt_node_(p,node,&left))return 0;
}
}
void mulib_parser_pratt_free(mulib_parser_pratt_t *t){if(t){free(t->nodes);
memset(t,0,sizeof(*t));
}}
int mulib_parser_pratt_parse(const char *source,size_t length,mulib_parser_pratt_t *out,size_t *error_offset){
 if(!out||out->nodes||out->count||(!source&&length))return 0;
 mulib_pratt_state_ p;
memset(&p,0,sizeof(p));
mulib_lexer_init(&p.lex,source,length);
 if(!mulib_pratt_next_(&p)||!mulib_pratt_expr_(&p,1,0,&p.tree.root)||p.token.kind!=MULIB_TOKEN_EOF){if(error_offset)*error_offset=p.token.offset;
mulib_parser_pratt_free(&p.tree);
return 0;
}
 *out=p.tree;
if(error_offset)*error_offset=length;
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
