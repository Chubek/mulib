/* MuLIB / Amb3r: ecad-port. C11 single-header kernel.
 * Define MULIB_IMPL_ECAD_PORT in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ECAD_PORT_H
#define MULIB_ECAD_PORT_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {const char *name;uint64_t id,net;size_t width;unsigned direction;} ma_port;
int mulib_ecad_port_match(const ma_port *a,const ma_port *b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ECAD_PORT) && !defined(MULIB_ECAD_PORT_H_IMPLEMENTED)
#define MULIB_ECAD_PORT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_port_match(const ma_port *a,const ma_port *b){return a&&b&&a->name&&b->name&&!strcmp(a->name,b->name)&&a->width&&a->width==b->width&&a->direction<=2&&b->direction<=2&&!(a->direction==1&&b->direction==1);}
#ifdef __cplusplus
}
#endif
#endif
