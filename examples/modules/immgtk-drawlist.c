/* Checked usage example for mulib-immgtk-drawlist.h. */
#define MULIB_IMPL_IMMGTK_DRAWLIST
#include "mulib-immgtk-drawlist.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_immgtk_drawlist_t d = {0};
    mulib_immgtk_drawcmd_t c = {MULIB_IMMGTK_LINE, 0, 0, 1, 1, .1, 0xff0000ff};
    assert(mulib_immgtk_drawlist_push(&d, c) && d.count == 1);
    c.width = -1;
    assert(!mulib_immgtk_drawlist_push(&d, c));
    mulib_immgtk_drawlist_clear(&d);
    assert(d.count == 0);
    mulib_immgtk_drawlist_free(&d);
    puts("PASS mulib-immgtk-drawlist");
    return 0;
}
