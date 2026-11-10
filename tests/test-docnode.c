/* Standalone API contract test for mulib-docnode. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCNODE
#include "mulib-docnode.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_docnode_list_t l={0};assert(mulib_docnode_push(&l,(mulib_docnode_item_t){2,3,10,20,0}));double w=0,h=0;assert(mulib_docnode_bounds(&l,&w,&h));assert(w==12&&h==23);assert(mulib_docnode_remove(&l,0));mulib_docnode_destroy(&l);return 0;}
