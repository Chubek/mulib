/* Focused executable regression for mulib-ecad-pcb-sync.h. */
#define MULIB_IMPL_ECAD_PCB_SYNC
#include "mulib-ecad-pcb-sync.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_ecad_pcb_sync_t s={0};assert(mulib_ecad_pcb_sync_add(&s,5,7));assert(mulib_ecad_pcb_sync_lookup(&s,5)==7);assert(!mulib_ecad_pcb_sync_add(&s,6,7));assert(!mulib_ecad_pcb_sync_lookup(&s,6));
    puts("PASS mulib-ecad-pcb-sync");
    return 0;
}
