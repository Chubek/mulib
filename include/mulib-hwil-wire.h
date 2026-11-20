/* MuLIB: hwil-wire. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_WIRE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_WIRE_H
#define MULIB_HWIL_WIRE_H
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
#include "mulib-graph-core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t id;size_t driver;unsigned width,driven;} mulib_hwil_hw_wire;
int mulib_hwil_wire_connect(mulib_hwil_hw_wire *w,size_t driver,unsigned width);
int mulib_hwil_wire_disconnect(mulib_hwil_hw_wire *w,size_t driver);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_WIRE) && !defined(MULIB_HWIL_WIRE_H_IMPLEMENTED)
#define MULIB_HWIL_WIRE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_wire_connect(mulib_hwil_hw_wire *w,size_t driver,unsigned width){if(!w||!width||width>64||w->width!=width||driver==MULIB_STORAGE_NONE||w->driven)return 0;w->driver=driver;w->driven=1;return 1;}
int mulib_hwil_wire_disconnect(mulib_hwil_hw_wire *w,size_t driver){if(!w||!w->driven||w->driver!=driver)return 0;w->driver=MULIB_STORAGE_NONE;w->driven=0;return 1;}
#ifdef __cplusplus
}
#endif
#endif
