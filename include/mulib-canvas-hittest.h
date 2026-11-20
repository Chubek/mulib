/* MuLIB: canvas-hittest. C11 single-header kernel.
 * Define MULIB_IMPL_CANVAS_HITTEST in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_CANVAS_HITTEST_H
#define MULIB_CANVAS_HITTEST_H
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
#include "mulib-geom2-core.h"
#include "mulib-graph-core.h"
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_canvas_hittest(const mulib_geom2_box *objects,size_t count,mulib_geom2_point point,double tolerance);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CANVAS_HITTEST) && !defined(MULIB_CANVAS_HITTEST_H_IMPLEMENTED)
#define MULIB_CANVAS_HITTEST_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_canvas_hittest(const mulib_geom2_box *objects,size_t count,mulib_geom2_point point,double tolerance){if((!objects&&count)||!mulib_geom2_point_valid(point)||!isfinite(tolerance)||tolerance<0)return MULIB_STORAGE_NONE;for(size_t i=count;i-->0;)if(mulib_geom2_box_valid(objects[i])&&point.x>=objects[i].lo.x-tolerance&&point.x<=objects[i].hi.x+tolerance&&point.y>=objects[i].lo.y-tolerance&&point.y<=objects[i].hi.y+tolerance)return i;return MULIB_STORAGE_NONE;}
#ifdef __cplusplus
}
#endif
#endif
