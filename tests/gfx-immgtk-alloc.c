/* Real allocation failure injection, scoped to native/IO allocations. */
#include "mulib-gui-core.h"
#include "mulib-input-keyboard.h"
#include "mulib-input-mouse.h"
#include "mulib-textbuffer.h"
#include <assert.h>
#include <stdio.h>
static size_t fail_after = SIZE_MAX;
static void *test_realloc(void *p, size_t n) {
    if (!fail_after) return NULL;
    if (fail_after != SIZE_MAX) --fail_after;
    return realloc(p, n);
}
#define realloc test_realloc
#include "mulib-immgtk-io.h"
#undef realloc
int main(void) {
    mulib_immgtk_native_t g;
    mgx_surface s = {0};
    mgx_rect r = {0, 0, 16, 16};
    assert(mgx_surface_init(&s, 32, 32));
    mulib_immgtk_native_init(&g);
    mulib_immgtk_native_begin(&g, &s, 4, 4, 0, 0);
    fail_after = 0;
    assert(!mig_interact(&g, 1, r, 1) && !g.items && !g.n && !g.cap);
    assert(g.error & MIG_ERROR_MEMORY);
    assert(!mig_state_get(&g, 1) && !g.states && !g.sn && !g.sc);
    mig_rect(&g, r, 0xffffffff);
    assert(!g.draws.n && !g.draws.v);
    mulib_immgtk_native_end(&g);
    fail_after = SIZE_MAX;
    mulib_immgtk_native_begin(&g, &s, 4, 4, 0, 0);
    assert(!mig_interact(&g, 1, r, 1));
    assert(mig_state_get(&g, 1));
    mulib_immgtk_native_end(&g);
    fail_after = 0;
    mgx_event e[2] = {{0}, {0}};
    e[0].type = e[1].type = MGX_EVENT_BUTTON;
    e[0].down = 1;
    assert(!mulib_immgtk_native_begin_events(&g, &s, e, 2));
    assert(!g.an && !g.active && !g.focus && !g.down);
    assert(!mig_interact(&g, 1, r, 1));
    mulib_immgtk_native_end(&g);
    mulib_immgtk_io_t io = {0};
    assert(!mulib_immgtk_io_feed(&io, e));
    assert(io.error && !io.n && !io.events && !io.mouse.buttons);
    mulib_immgtk_io_reset(&io);
    fail_after = SIZE_MAX;
    assert(mulib_immgtk_io_feed(&io, e));
    mulib_immgtk_io_free(&io);
    mulib_immgtk_native_free(&g);
    mgx_surface_free(&s);
    puts("PASS native ImmGTK allocation failure propagation and cleanup");
    return 0;
}
