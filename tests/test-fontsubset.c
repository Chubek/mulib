/* Standalone API contract test for mulib-fontsubset. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_FONTSUBSET
#include "mulib-fontsubset.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_fontsubset_list_t l={0};assert(mulib_fontsubset_push(&l,(mulib_fontsubset_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_fontsubset_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_fontsubset_remove(&l,0));mulib_fontsubset_destroy(&l);return 0;}
