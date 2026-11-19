/* MuLIB / Amb3r: physical-lef. C11 single-header kernel.
 * Define MULIB_IMPL_PHYSICAL_LEF in exactly one implementation translation unit.
 * Profile: Single-layer rectangular-block macro fragment writer; not full LEF interchange.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PHYSICAL_LEF_H
#define MULIB_PHYSICAL_LEF_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_physical_lef_macro(FILE *f,const char *name,double width,double height,const char *layer,const ma_box *obstructions,size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PHYSICAL_LEF) && !defined(MULIB_PHYSICAL_LEF_H_IMPLEMENTED)
#define MULIB_PHYSICAL_LEF_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_physical_lef_macro(FILE *f,const char *name,double width,double height,const char *layer,const ma_box *obstructions,size_t n){if(!f||!ma_word(name)||!ma_word(layer)||width<=0||height<=0||!isfinite(width+height)||(!obstructions&&n))return 0;for(size_t i=0;i<n;i++)if(!ma_box_valid(obstructions[i]))return 0;if(fprintf(f,"MACRO %s\n CLASS BLOCK ;\n ORIGIN 0 0 ;\n SIZE %.9g BY %.9g ;\n OBS\n LAYER %s ;\n",name,width,height,layer)<0)return 0;for(size_t i=0;i<n;i++)if(fprintf(f," RECT %.9g %.9g %.9g %.9g ;\n",obstructions[i].lo.x,obstructions[i].lo.y,obstructions[i].hi.x,obstructions[i].hi.y)<0)return 0;return fprintf(f," END\nEND %s\n",name)>0;}
#ifdef __cplusplus
}
#endif
#endif
