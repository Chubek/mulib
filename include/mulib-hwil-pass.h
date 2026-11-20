/* MuLIB: hwil-pass. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_PASS in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_PASS_H
#define MULIB_HWIL_PASS_H
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
#include "mulib-hwil-context.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {int (*run)(void *,mulib_hwil_hw_context *);void *ctx;} mulib_hwil_hw_pass;
int mulib_hwil_pass_run(mulib_hwil_hw_context *c,const mulib_hwil_hw_pass *passes,size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_PASS) && !defined(MULIB_HWIL_PASS_H_IMPLEMENTED)
#define MULIB_HWIL_PASS_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_pass_run(mulib_hwil_hw_context *c,const mulib_hwil_hw_pass *passes,size_t n){if(!c||(!passes&&n))return 0;for(size_t j=0;j<n;j++){mulib_hwil_hw_context t={0};if(!mulib_storage_reserve((void**)&t.nodes,&t.cap,c->n,sizeof *t.nodes))return 0;t.n=c->n;if(t.n)memcpy(t.nodes,c->nodes,t.n*sizeof *t.nodes);int ok=passes[j].run&&passes[j].run(passes[j].ctx,&t);for(size_t i=0;ok&&i<t.n;i++)ok=mulib_hwil_hw_node_valid(&t,i);if(!ok){mulib_hwil_context_free(&t);return 0;}mulib_hwil_context_free(c);*c=t;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
