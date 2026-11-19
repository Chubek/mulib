/* MuLIB / Amb3r: synth-cellmap. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_CELLMAP in exactly one implementation translation unit.
 * Profile: Select exact truth-table cell by weighted area and delay; no global cover or timing optimization.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_CELLMAP_H
#define MULIB_SYNTH_CELLMAP_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {const char *name;unsigned inputs;uint64_t truth;double area,delay;} ma_logic_cell;
int mulib_synth_cellmap_choose(const ma_logic_cell *cells,size_t n,unsigned inputs,uint64_t truth,double area_weight,double delay_weight,size_t *choice);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_CELLMAP) && !defined(MULIB_SYNTH_CELLMAP_H_IMPLEMENTED)
#define MULIB_SYNTH_CELLMAP_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_cellmap_choose(const ma_logic_cell *cells,size_t n,unsigned inputs,uint64_t truth,double area_weight,double delay_weight,size_t *choice){if((!cells&&n)||!choice||inputs>6||area_weight<0||delay_weight<0)return 0;double best=INFINITY;size_t id=MA_NONE;for(size_t i=0;i<n;i++)if(cells[i].inputs==inputs&&cells[i].truth==truth&&cells[i].area>=0&&cells[i].delay>=0){double c=area_weight*cells[i].area+delay_weight*cells[i].delay;if(c<best){best=c;id=i;}}if(id==MA_NONE)return 0;*choice=id;return 1;}
#ifdef __cplusplus
}
#endif
#endif
