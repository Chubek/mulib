/* Standalone API contract test for mulib-texbox. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_TEXBOX
#include "mulib-texbox.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_texbox_box_t b[]={{10,8,2},{20,10,3}},o;assert(mulib_texbox_hpack(b,2,&o));assert(o.width==30&&o.height==10&&o.depth==3);assert(mulib_texbox_vpack(b,2,&o)&&o.height==23);return 0;}
