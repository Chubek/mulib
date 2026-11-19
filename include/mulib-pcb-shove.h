/* MuLIB / Amb3r: pcb-shove. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_SHOVE in exactly one implementation translation unit.
 * Profile: Axis-aligned obstacle translation reference profile; not production interactive trace pushing.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_SHOVE_H
#define MULIB_PCB_SHOVE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_pcb_shove(ma_box *b,const unsigned char *fixed,size_t n,size_t moved,ma_point delta,double clearance);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_SHOVE) && !defined(MULIB_PCB_SHOVE_H_IMPLEMENTED)
#define MULIB_PCB_SHOVE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_shove(ma_box *b,const unsigned char *fixed,size_t n,size_t moved,ma_point delta,double clearance){if(!b||!fixed||moved>=n||fixed[moved]||clearance<0||(delta.x!=0&&delta.y!=0)||(delta.x==0&&delta.y==0))return 0;ma_box *t=(ma_box*)ma_alloc(n,sizeof *t);if(!t)return 0;memcpy(t,b,n*sizeof *t);t[moved].lo.x+=delta.x;t[moved].hi.x+=delta.x;t[moved].lo.y+=delta.y;t[moved].hi.y+=delta.y;unsigned char *dirty=(unsigned char*)ma_alloc(n,1);if(!dirty){free(t);return 0;}dirty[moved]=1;int ok=0;for(size_t k=0;k<=n;k++){int change=0;for(size_t i=0;i<n;i++)if(dirty[i]){dirty[i]=0;for(size_t j=0;j<n;j++)if(i!=j){ma_box x=t[i];x.lo.x-=clearance;x.lo.y-=clearance;x.hi.x+=clearance;x.hi.y+=clearance;if(!ma_box_hit(x,t[j]))continue;double dx=0,dy=0;if(delta.x>0&&t[j].hi.x>t[i].hi.x)dx=x.hi.x-t[j].lo.x;else if(delta.x<0&&t[j].lo.x<t[i].lo.x)dx=x.lo.x-t[j].hi.x;else if(delta.y>0&&t[j].hi.y>t[i].hi.y)dy=x.hi.y-t[j].lo.y;else if(delta.y<0&&t[j].lo.y<t[i].lo.y)dy=x.lo.y-t[j].hi.y;else continue;if(!dx&&!dy)continue;if(fixed[j]||j==moved)goto end;t[j].lo.x+=dx;t[j].hi.x+=dx;t[j].lo.y+=dy;t[j].hi.y+=dy;dirty[j]=1;change=1;}}if(!change){ok=1;break;}}end:if(ok)memcpy(b,t,n*sizeof *t);free(dirty);free(t);return ok;}
#ifdef __cplusplus
}
#endif
#endif
