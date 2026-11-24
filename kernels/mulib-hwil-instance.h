/* MuLIB: hwil-instance. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_INSTANCE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_INSTANCE_H
#define MULIB_HWIL_INSTANCE_H
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
#include "mulib-hwil-port.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_hwil_instance_bind(const mulib_hwil_hw_port *ports,size_t n,const mulib_hwil_hw_port *actual,size_t m,size_t *mapping);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_INSTANCE) && !defined(MULIB_HWIL_INSTANCE_H_IMPLEMENTED)
#define MULIB_HWIL_INSTANCE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_instance_bind(const mulib_hwil_hw_port *ports,size_t n,const mulib_hwil_hw_port *actual,size_t m,size_t *mapping){if((!ports&&n)||(!actual&&m)||(!mapping&&n)||n!=m)return 0;for(size_t i=0;i<n;i++){size_t j=0;if(!ports[i].name)return 0;for(;j<m;j++)if(actual[j].name&&!strcmp(ports[i].name,actual[j].name))break;if(j==m||ports[i].width!=actual[j].width)return 0;for(size_t k=0;k<i;k++)if(mapping[k]==actual[j].node)return 0;mapping[i]=actual[j].node;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
