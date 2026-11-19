/* Deliberately create controls in a different translation unit. */
#include "mulib-retgtk-button.h"
#include "mulib-retgtk-entry.h"

mulib_retgtk_widget_t *gfx_retgtk_peer_toggle(uint64_t id)
{
    return mulib_retgtk_toggle(id, "peer", 1);
}

mulib_retgtk_widget_t *gfx_retgtk_peer_entry(uint64_t id)
{
    return mulib_retgtk_entry(id, "edited in peer");
}
