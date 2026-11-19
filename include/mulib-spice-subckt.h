/* MuLIB / Amb3r: spice-subckt. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_SUBCKT in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_SUBCKT_H
#define MULIB_SPICE_SUBCKT_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_spice_subckt_map(const size_t *local_ports,const size_t *actual,size_t ports,size_t local_nodes,size_t first_internal,size_t *mapping,size_t *next);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_SUBCKT) && !defined(MULIB_SPICE_SUBCKT_H_IMPLEMENTED)
#define MULIB_SPICE_SUBCKT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_subckt_map(const size_t *local_ports,const size_t *actual,size_t ports,size_t local_nodes,size_t first_internal,size_t *mapping,size_t *next){if((!local_ports&&ports)||(!actual&&ports)||(!mapping&&local_nodes)||!next)return 0;unsigned char *assigned=(unsigned char*)ma_alloc(local_nodes,1);if(!assigned)return 0;for(size_t i=0;i<ports;i++){if(local_ports[i]>=local_nodes||assigned[local_ports[i]]){free(assigned);return 0;}assigned[local_ports[i]]=1;mapping[local_ports[i]]=actual[i];}for(size_t i=0;i<local_nodes;i++)if(!assigned[i]){if(first_internal==SIZE_MAX){free(assigned);return 0;}mapping[i]=first_internal++;}*next=first_internal;free(assigned);return 1;}
#ifdef __cplusplus
}
#endif
#endif
