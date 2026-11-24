/** @file mulib-geom2-core.h
 * @brief Double-precision planar points, boxes and polygon predicates.
 * C11 header-only substrate; standard library and libm only.
 * Pointer/count arguments require live arrays of the declared lengths.
 * Stateful objects require caller synchronization.
 */
#ifndef MULIB_GEOM2_CORE_H
#define MULIB_GEOM2_CORE_H
#include "mulib-storage-core.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Coordinates are doubles, with no fixed application unit. Predicates use
 * floating-point arithmetic, not adaptive/exact predicates. Polygon inclusion
 * treats distance <= 1e-12 from an edge as boundary and includes it.
 * Check finite points/ordered boxes explicitly before unchecked operations. */
typedef struct {double x,y;} mulib_geom2_point;
typedef struct {mulib_geom2_point lo,hi;} mulib_geom2_box;
static inline int mulib_geom2_point_valid(mulib_geom2_point p){return isfinite(p.x)&&isfinite(p.y);}
static inline double mulib_geom2_distance(mulib_geom2_point a,mulib_geom2_point b){return hypot(a.x-b.x,a.y-b.y);}
static inline double mulib_geom2_orient(mulib_geom2_point a,mulib_geom2_point b,mulib_geom2_point c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);}
static inline int mulib_geom2_box_valid(mulib_geom2_box b){return mulib_geom2_point_valid(b.lo)&&mulib_geom2_point_valid(b.hi)&&b.lo.x<=b.hi.x&&b.lo.y<=b.hi.y;}
static inline int mulib_geom2_box_overlap(mulib_geom2_box a,mulib_geom2_box b){return a.lo.x<=b.hi.x&&a.hi.x>=b.lo.x&&a.lo.y<=b.hi.y&&a.hi.y>=b.lo.y;}
static inline double mulib_geom2_segment_distance(mulib_geom2_point p,mulib_geom2_point a,mulib_geom2_point b){double dx=b.x-a.x,dy=b.y-a.y,d=dx*dx+dy*dy;double t=d?((p.x-a.x)*dx+(p.y-a.y)*dy)/d:0;t=MULIB_STORAGE_MAX(0,MULIB_STORAGE_MIN(1,t));return hypot(p.x-a.x-t*dx,p.y-a.y-t*dy);}
static inline int mulib_geom2_polygon_contains(const mulib_geom2_point *p,size_t n,mulib_geom2_point q){int in=0;if(!p||n<3)return 0;for(size_t i=0,j=n-1;i<n;j=i++){if(mulib_geom2_segment_distance(q,p[j],p[i])<=1e-12)return 1;if((p[i].y>q.y)!=(p[j].y>q.y)&&q.x<(p[j].x-p[i].x)*(q.y-p[i].y)/(p[j].y-p[i].y)+p[i].x)in=!in;}return in;}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_GEOM2_CORE_H */
