/* Focused executable regression for mulib-pcb-board.h. */
#define MULIB_IMPL_PCB_GEOMETRY
#define MULIB_IMPL_PCB_BOARD
#include "mulib-pcb-board.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_pcb_board_t b={0};mulib_pcb_segment_t t={{0,0},{10,0},.3,1};assert(mulib_pcb_board_init(&b,4));assert(mulib_pcb_board_add_trace(&b,t)&&b.count==1);t.width=-1;assert(!mulib_pcb_board_add_trace(&b,t));mulib_pcb_board_free(&b);
    puts("PASS mulib-pcb-board");
    return 0;
}
