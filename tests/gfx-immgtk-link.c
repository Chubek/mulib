#include "mulib-immgtk-native.h"
#include <assert.h>
uint64_t gfx_immgtk_peer_id(const mulib_immgtk_native_t *);
int gfx_immgtk_peer_interact(mulib_immgtk_native_t *);
int main(void) {
    mulib_immgtk_native_t g;
    mgx_surface s = {0};
    assert(mgx_surface_init(&s, 32, 32));
    mulib_immgtk_native_init(&g);
    assert(gfx_immgtk_peer_id(&g) == mig_id(&g, "cross-tu"));
    mulib_immgtk_native_begin(&g, &s, 4, 4, 0, 0);
    assert(!gfx_immgtk_peer_interact(&g));
    mulib_immgtk_native_end(&g);
    mulib_immgtk_native_begin(&g, &s, 4, 4, 1, 0);
    assert(!gfx_immgtk_peer_interact(&g));
    mulib_immgtk_native_end(&g);
    mulib_immgtk_native_begin(&g, &s, 4, 4, 0, 0);
    assert(gfx_immgtk_peer_interact(&g));
    mulib_immgtk_native_end(&g);
    mulib_immgtk_native_free(&g);
    mgx_surface_free(&s);
    return 0;
}
