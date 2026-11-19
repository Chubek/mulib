/* MuLIB / Amb3r: hwil-wire. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_WIRE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_WIRE_H
#define MULIB_HWIL_WIRE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t id;size_t driver;unsigned width,driven;} ma_hw_wire;
int mulib_hwil_wire_connect(ma_hw_wire *w,size_t driver,unsigned width);
int mulib_hwil_wire_disconnect(ma_hw_wire *w,size_t driver);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_WIRE) && !defined(MULIB_HWIL_WIRE_H_IMPLEMENTED)
#define MULIB_HWIL_WIRE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_wire_connect(ma_hw_wire *w,size_t driver,unsigned width){if(!w||!width||width>64||w->width!=width||driver==MA_NONE||w->driven)return 0;w->driver=driver;w->driven=1;return 1;}
int mulib_hwil_wire_disconnect(ma_hw_wire *w,size_t driver){if(!w||!w->driven||w->driver!=driver)return 0;w->driver=MA_NONE;w->driven=0;return 1;}
#ifdef __cplusplus
}
#endif
#endif
