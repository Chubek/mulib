/* Standalone API contract test for mulib-musicnotation. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_MUSICNOTATION
#include "mulib-musicnotation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_musicnotation_list_t l={0};assert(mulib_musicnotation_push(&l,(mulib_musicnotation_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_musicnotation_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_musicnotation_remove(&l,0));mulib_musicnotation_destroy(&l);return 0;}
