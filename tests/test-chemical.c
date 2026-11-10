/* Standalone API contract test for mulib-chemical. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_CHEMICAL
#include "mulib-chemical.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_chemical_list_t l={0};assert(mulib_chemical_push(&l,(mulib_chemical_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_chemical_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_chemical_remove(&l,0));mulib_chemical_destroy(&l);return 0;}
