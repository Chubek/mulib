/* MuLIB: hdl-symboltable. C11 single-header kernel.
 * Define MULIB_IMPL_HDL_SYMBOLTABLE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HDL_SYMBOLTABLE_H
#define MULIB_HDL_SYMBOLTABLE_H
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
typedef struct {char *name;size_t value,depth;} mulib_hdl_symbol;
typedef struct {mulib_hdl_symbol *v;size_t n,cap,depth;} mulib_hdl_symbols;
int mulib_hdl_symboltable_enter(mulib_hdl_symbols *s);
int mulib_hdl_symboltable_insert(mulib_hdl_symbols *s,const char *name,size_t value);
int mulib_hdl_symboltable_lookup(const mulib_hdl_symbols *s,const char *name,size_t *out);
int mulib_hdl_symboltable_leave(mulib_hdl_symbols *s);
void mulib_hdl_symboltable_free(mulib_hdl_symbols *s);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HDL_SYMBOLTABLE) && !defined(MULIB_HDL_SYMBOLTABLE_H_IMPLEMENTED)
#define MULIB_HDL_SYMBOLTABLE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hdl_symboltable_enter(mulib_hdl_symbols *s){if(!s||s->depth==SIZE_MAX)return 0;s->depth++;return 1;}
int mulib_hdl_symboltable_insert(mulib_hdl_symbols *s,const char *name,size_t value){if(!s||!mulib_storage_identifier_valid(name))return 0;for(size_t i=s->n;i-->0;){if(s->v[i].depth!=s->depth)break;if(!strcmp(s->v[i].name,name))return 0;}mulib_hdl_symbol e={mulib_storage_string_dup(name),value,s->depth};if(!e.name||!mulib_storage_push((void**)&s->v,&s->n,&s->cap,sizeof e,&e)){free(e.name);return 0;}return 1;}
int mulib_hdl_symboltable_lookup(const mulib_hdl_symbols *s,const char *name,size_t *out){if(!s||!name||!out)return 0;for(size_t i=s->n;i-->0;)if(!strcmp(s->v[i].name,name)){*out=s->v[i].value;return 1;}return 0;}
int mulib_hdl_symboltable_leave(mulib_hdl_symbols *s){if(!s||!s->depth)return 0;while(s->n&&s->v[s->n-1].depth==s->depth)free(s->v[--s->n].name);s->depth--;return 1;}
void mulib_hdl_symboltable_free(mulib_hdl_symbols *s){if(s){for(size_t i=0;i<s->n;i++)free(s->v[i].name);free(s->v);memset(s,0,sizeof *s);}}
#ifdef __cplusplus
}
#endif
#endif
