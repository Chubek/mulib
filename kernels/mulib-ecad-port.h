/* MuLIB: ecad-port. C11 single-header kernel.
 * Define MULIB_IMPL_ECAD_PORT in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ECAD_PORT_H
#define MULIB_ECAD_PORT_H
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
typedef struct {const char *name;uint64_t id,net;size_t width;unsigned direction;} mulib_ecad_port;
int mulib_ecad_port_match(const mulib_ecad_port *a,const mulib_ecad_port *b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ECAD_PORT) && !defined(MULIB_ECAD_PORT_H_IMPLEMENTED)
#define MULIB_ECAD_PORT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_port_match(const mulib_ecad_port *a,const mulib_ecad_port *b){return a&&b&&a->name&&b->name&&!strcmp(a->name,b->name)&&a->width&&a->width==b->width&&a->direction<=2&&b->direction<=2&&!(a->direction==1&&b->direction==1);}
#ifdef __cplusplus
}
#endif
#endif
