/* MuLIB / Amb3r: pcb-kicad. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_KICAD in exactly one implementation translation unit.
 * Profile: Minimal two-layer track-only KiCad board export; no full board import/export.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_KICAD_H
#define MULIB_PCB_KICAD_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {ma_point a,b;double width;unsigned layer,net;} ma_kicad_track;
int mulib_pcb_kicad_write(FILE *f,const ma_kicad_track *t,size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_KICAD) && !defined(MULIB_PCB_KICAD_H_IMPLEMENTED)
#define MULIB_PCB_KICAD_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_kicad_write(FILE *f,const ma_kicad_track *t,size_t n){if(!f||(!t&&n))return 0;if(fputs("(kicad_pcb (version 20221018) (generator amb3r)\n (general (thickness 1.6))\n (layers (0 \"F.Cu\" signal) (31 \"B.Cu\" signal) (44 \"Edge.Cuts\" user))\n",f)<0)return 0;for(size_t i=0;i<n;i++){if(!ma_point_valid(t[i].a)||!ma_point_valid(t[i].b)||t[i].width<=0||!isfinite(t[i].width)||t[i].layer>1)return 0;if(t[i].net){int seen=0;for(size_t j=0;j<i;j++)if(t[j].net==t[i].net)seen=1;if(!seen&&fprintf(f," (net %u \"NET_%u\")\n",t[i].net,t[i].net)<0)return 0;}}for(size_t i=0;i<n;i++)if(fprintf(f," (segment (start %.9g %.9g) (end %.9g %.9g) (width %.9g) (layer \"%s.Cu\") (net %u))\n",t[i].a.x,t[i].a.y,t[i].b.x,t[i].b.y,t[i].width,t[i].layer?"B":"F",t[i].net)<0)return 0;return fputs(")\n",f)>=0;}
#ifdef __cplusplus
}
#endif
#endif
