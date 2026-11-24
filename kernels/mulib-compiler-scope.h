/* MuLIB compiler tools: Scope-oriented facade for MuLIB lexical symbol tables.
 * ISO C11 single-header module. Define MULIB_IMPL_COMPILER_SCOPE in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_COMPILER_SCOPE_H
#define MULIB_COMPILER_SCOPE_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-symboltable.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-symboltable.h"
typedef mulib_symboltable_t mulib_compiler_scope_t;
int mulib_compiler_scope_bind(mulib_compiler_scope_t *s,const char *name,uint64_t symbol);
int mulib_compiler_scope_resolve(const mulib_compiler_scope_t *s,const char *name,uint64_t *symbol);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_COMPILER_SCOPE_H */
#if defined(MULIB_IMPL_COMPILER_SCOPE) && !defined(MULIB_COMPILER_SCOPE_IMPLEMENTED)
#define MULIB_COMPILER_SCOPE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_compiler_scope_bind(mulib_compiler_scope_t *s,const char *name,uint64_t symbol){return mulib_symboltable_define(s,name,symbol);
}
int mulib_compiler_scope_resolve(const mulib_compiler_scope_t *s,const char *name,uint64_t *symbol){return mulib_symboltable_lookup(s,name,symbol);
}
#ifdef __cplusplus
}
#endif
#endif
