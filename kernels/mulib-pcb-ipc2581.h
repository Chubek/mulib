/* MuLIB: pcb-ipc2581. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_IPC2581 in exactly one implementation translation unit.
 * Profile: Layer-stack XML fragment only; not a conforming full IPC-2581 exporter.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_IPC2581_H
#define MULIB_PCB_IPC2581_H
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
#include "mulib-pcb-stackup.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_pcb_ipc2581_stackup(FILE *f,const mulib_pcb_layer *layers,size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_IPC2581) && !defined(MULIB_PCB_IPC2581_H_IMPLEMENTED)
#define MULIB_PCB_IPC2581_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_ipc2581_stackup(FILE *f,const mulib_pcb_layer *layers,size_t n){double total;if(!f||!mulib_pcb_stackup_thickness(layers,n,&total))return 0;if(fprintf(f,"<Stackup thickness=\"%.9g\">\n",total)<0)return 0;for(size_t i=0;i<n;i++)if(fprintf(f,"  <StackupLayer layerOrGroupRef=\"L%zu\" thickness=\"%.9g\"/>\n",i+1,layers[i].thickness)<0)return 0;return fputs("</Stackup>\n",f)>=0;}
#ifdef __cplusplus
}
#endif
#endif
