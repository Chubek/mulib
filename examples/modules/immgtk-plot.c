/* Checked usage example for mulib-immgtk-plot.h. */
#define MULIB_IMPL_IMMGTK_PLOT
#include "mulib-immgtk-plot.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    double a[] = {1, 2, 3}, b[] = {-1, 9, 4}, x0, x1, y0, y1, z;
    assert(mulib_immgtk_plot_bounds(a, b, 3, &x0, &x1, &y0, &y1) && x0 == 1 && x1 == 3 &&
           y0 == -1 && y1 == 9);
    assert(mulib_immgtk_plot_log_transform(100, 10, &z) && fabs(z - 2) < 1e-10);
    assert(!mulib_immgtk_plot_log_transform(0, 10, &z));
    puts("PASS mulib-immgtk-plot");
    return 0;
}
