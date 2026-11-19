#include "mulib-immgtk-native.h"
uint64_t gfx_immgtk_peer_id(const mulib_immgtk_native_t *g) {
    return mig_id(g, "cross-tu");
}
int gfx_immgtk_peer_interact(mulib_immgtk_native_t *g) {
    return mig_interact(g, mig_id(g, "cross-tu"), (mgx_rect){0, 0, 16, 16}, 1);
}
