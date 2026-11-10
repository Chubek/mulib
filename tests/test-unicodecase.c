/* Standalone API contract test for mulib-unicodecase. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_UNICODECASE
#include "mulib-unicodecase.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_unicodecase_list_t l={0};assert(mulib_unicodecase_push(&l,(mulib_unicodecase_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_unicodecase_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_unicodecase_remove(&l,0));mulib_unicodecase_destroy(&l);return 0;}
