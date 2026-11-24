/* MuLIB: hwil-port. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_PORT in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_PORT_H
#define MULIB_HWIL_PORT_H
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
typedef struct {const char *name;size_t node;unsigned width,direction;} mulib_hwil_hw_port;
int mulib_hwil_port_validate(const mulib_hwil_hw_port *p,size_t n,const mulib_hwil_hw_context *c);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_PORT) && !defined(MULIB_HWIL_PORT_H_IMPLEMENTED)
#define MULIB_HWIL_PORT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_port_validate(const mulib_hwil_hw_port *p,size_t n,const mulib_hwil_hw_context *c){if((!p&&n)||!c)return 0;for(size_t i=0;i<n;i++){if(!mulib_storage_identifier_valid(p[i].name)||p[i].direction>2||p[i].node>=c->n||p[i].width!=c->nodes[p[i].node].width)return 0;for(size_t j=0;j<i;j++)if(!strcmp(p[i].name,p[j].name))return 0;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
