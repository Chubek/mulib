/* MuLIB / Amb3r: ecad-instance. C11 single-header kernel.
 * Define MULIB_IMPL_ECAD_INSTANCE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ECAD_INSTANCE_H
#define MULIB_ECAD_INSTANCE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

ma_point mulib_ecad_instance_transform(ma_point p,ma_point offset,unsigned rotation,int mirror);
ma_point mulib_ecad_instance_inverse(ma_point p,ma_point offset,unsigned rotation,int mirror);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ECAD_INSTANCE) && !defined(MULIB_ECAD_INSTANCE_H_IMPLEMENTED)
#define MULIB_ECAD_INSTANCE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
ma_point mulib_ecad_instance_transform(ma_point p,ma_point offset,unsigned rotation,int mirror){if(mirror)p.x=-p.x;for(unsigned i=0;i<rotation%4;i++){double t=p.x;p.x=-p.y;p.y=t;}p.x+=offset.x;p.y+=offset.y;return p;}
ma_point mulib_ecad_instance_inverse(ma_point p,ma_point offset,unsigned rotation,int mirror){p.x-=offset.x;p.y-=offset.y;for(unsigned i=0;i<(4-rotation%4)%4;i++){double t=p.x;p.x=-p.y;p.y=t;}if(mirror)p.x=-p.x;return p;}
#ifdef __cplusplus
}
#endif
#endif
