#include "link.h"
#include <cassert>

int
main()
{
 uint32_t pixels[64*64]={0};
 assert(!mulib_gfx_link_render(pixels,1));
 assert(mulib_gfx_link_render(pixels,64*64));
 assert(pixels[0]==UINT32_C(0xff202630));
 assert(pixels[3*64+3]!=pixels[0]);
 assert(pixels[35*64+3]!=pixels[0]);
 return 0;
}
