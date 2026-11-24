/* MuLIB compiler tools: Checked Pratt-expression typing with numeric and Boolean rules.
 * ISO C11 single-header module. Define MULIB_IMPL_TYPECHECK in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_TYPECHECK_H
#define MULIB_TYPECHECK_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-parser-pratt.h"
#include "mulib-typesystem.h"
#include "mulib-symboltable.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-parser-pratt.h"
#include "mulib-typesystem.h"
#include "mulib-symboltable.h"
/* scope maps identifiers to interned type IDs; fails on missing symbols. */
int mulib_typecheck_expr(const mulib_parser_pratt_t *tree,const mulib_typesystem_t *types,const mulib_symboltable_t *scope,size_t *type_id);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TYPECHECK_H */
#if defined(MULIB_IMPL_TYPECHECK) && !defined(MULIB_TYPECHECK_IMPLEMENTED)
#define MULIB_TYPECHECK_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_typecheck_rec_(const mulib_parser_pratt_t *tree,const mulib_typesystem_t *types,const mulib_symboltable_t *scope,size_t id,size_t depth,size_t *out){
 if(id>=tree->count||depth>128)return 0;
const mulib_pratt_node_t *n=tree->nodes+id;
size_t a=SIZE_MAX,b=SIZE_MAX;
 if(n->opcode==MULIB_PRATT_LITERAL){
 for(size_t i=0;i<types->count;i++){
 if(types->types[i].kind==MULIB_TYPE_I64){*out=i;return 1;}
 }
 return 0;
}
 if(n->opcode==MULIB_PRATT_NAME){uint64_t sym;
if(!scope||!mulib_symboltable_lookup(scope,n->name,&sym)||sym>=types->count)return 0;
*out=(size_t)sym;
return 1;
}
 if(!mulib_typecheck_rec_(tree,types,scope,n->left,depth+1,&a))return 0;
 if(n->opcode==MULIB_PRATT_NEG){if(types->types[a].kind!=MULIB_TYPE_I64)return 0;
*out=a;
return 1;
}
 if(!mulib_typecheck_rec_(tree,types,scope,n->right,depth+1,&b)||a!=b||types->types[a].kind!=MULIB_TYPE_I64)return 0;
 if(n->opcode==MULIB_PRATT_EQ||n->opcode==MULIB_PRATT_LT){
 for(size_t i=0;i<types->count;i++){
 if(types->types[i].kind==MULIB_TYPE_BOOL){*out=i;return 1;}
 }
 return 0;
}
 *out=a;
return n->opcode>=MULIB_PRATT_ADD&&n->opcode<=MULIB_PRATT_MOD;
}
int mulib_typecheck_expr(const mulib_parser_pratt_t *tree,const mulib_typesystem_t *types,const mulib_symboltable_t *scope,size_t *out){if(!tree||!types||!out||tree->root>=tree->count)return 0;
size_t id;
if(!mulib_typecheck_rec_(tree,types,scope,tree->root,0,&id))return 0;
*out=id;
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
