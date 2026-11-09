/* Focused executable regression for mulib-pcb-geometry.h. */
#define MULIB_IMPL_PCB_GEOMETRY
#include "mulib-pcb-geometry.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_pcb_point_t a={0,0},b={10,0},c={5,-5},d={5,5};assert(mulib_pcb_segment_distance(a,b,c,d)==0);assert(fabs(mulib_pcb_point_segment_distance((mulib_pcb_point_t){5,2},a,b)-2)<1e-12);mulib_pcb_segment_t s1={a,b,1,1},s2={c,d,1,2};assert(mulib_pcb_segments_violate(s1,s2,.2));s2.net=1;assert(!mulib_pcb_segments_violate(s1,s2,.2));
    puts("PASS mulib-pcb-geometry");
    return 0;
}
