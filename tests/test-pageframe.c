/* Standalone API contract test for mulib-pageframe. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_PAGEFRAME
#include "mulib-pageframe.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){mulib_pageframe_rect_t r;assert(mulib_pageframe_inner(612,792,72,72,72,72,&r));assert(r.width==468&&r.height==648);return 0;}
