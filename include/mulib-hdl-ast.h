/* MuLIB: hdl-ast. C11 single-header kernel.
 * Define MULIB_IMPL_HDL_AST in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HDL_AST_H
#define MULIB_HDL_AST_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {unsigned kind;size_t start,end,n,children[4];} mulib_hdl_ast_node;
typedef struct {mulib_hdl_ast_node *nodes;size_t n,cap;} mulib_hdl_ast;
int mulib_hdl_ast_add(mulib_hdl_ast *a,unsigned kind,size_t start,size_t end,const size_t *children,size_t n,size_t *id);
void mulib_hdl_ast_free(mulib_hdl_ast *a);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HDL_AST) && !defined(MULIB_HDL_AST_H_IMPLEMENTED)
#define MULIB_HDL_AST_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hdl_ast_add(mulib_hdl_ast *a,unsigned kind,size_t start,size_t end,const size_t *children,size_t n,size_t *id){if(!a||!kind||end<start||n>4||(!children&&n)||!id)return 0;mulib_hdl_ast_node t={kind,start,end,n,{0}};for(size_t i=0;i<n;i++){if(children[i]>=a->n)return 0;t.children[i]=children[i];}*id=a->n;return mulib_storage_push((void**)&a->nodes,&a->n,&a->cap,sizeof t,&t);}
void mulib_hdl_ast_free(mulib_hdl_ast *a){if(a){free(a->nodes);memset(a,0,sizeof *a);}}
#ifdef __cplusplus
}
#endif
#endif
