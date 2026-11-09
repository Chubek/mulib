/* Checked usage example for mulib-fpga-route.h. */
#define MULIB_IMPL_FPGA_ROUTE
#include "mulib-fpga-route.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    uint8_t grid[25] = {0};
    grid[2] = grid[7] = grid[12] = 1;
    uint16_t path[25];
    size_t len = 0;
    mulib_fpga_grid_t g = {5, 5, grid};
    assert(mulib_fpga_route_bfs(g, 0, 0, 4, 4, path, 25, &len) && len >= 9 && path[0] == 0 &&
           path[len - 1] == 24);
    grid[0] = 1;
    assert(!mulib_fpga_route_bfs(g, 0, 0, 4, 4, path, 25, &len));
    puts("PASS mulib-fpga-route");
    return 0;
}
