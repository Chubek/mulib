/* The unchanged legacy data/API remain usable from a C++ implementation TU.
 * This is NOT a C++ certification for the C11 native overlay. */
#define MULIB_IMPL_IMMGTK_DRAWLIST
#include "mulib-immgtk-drawlist.h"
#include <cassert>
int main() {
    mulib_immgtk_drawlist_t d{};
    mulib_immgtk_drawcmd_t c{MULIB_IMMGTK_LINE, 0, 0, 1, 1, 1, 0xffffffff};
    assert(mulib_immgtk_drawlist_push(&d, c));
    mulib_immgtk_drawlist_free(&d);
}
