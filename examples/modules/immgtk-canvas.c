/* Checked usage example for mulib-immgtk-canvas.h. */
#define MULIB_IMPL_IMMGTK_CANVAS
#include "mulib-immgtk-canvas.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_immgtk_canvas_t c = {0, 0, 2, 10, 20};
    double sx, sy, wx, wy;
    assert(mulib_immgtk_canvas_world_to_screen(&c, 2, 3, &sx, &sy) && sx == 14 && sy == 26);
    assert(mulib_immgtk_canvas_zoom_at(&c, 14, 26, 2));
    assert(mulib_immgtk_canvas_screen_to_world(&c, 14, 26, &wx, &wy) && fabs(wx - 2) < 1e-12 &&
           fabs(wy - 3) < 1e-12);
    assert(!mulib_immgtk_canvas_zoom_at(&c, 0, 0, -2));
    puts("PASS mulib-immgtk-canvas");
    return 0;
}
