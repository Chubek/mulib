#define MULIB_IMPL_FRAMEBUFFER
#include "mulib-framebuffer.h"
#include <assert.h>
int main(void) {
    uint32_t pixels[64]; mulib_framebuffer_t surface = {pixels, 64, 8, 8, 8};
    assert(!mulib_framebuffer_pattern(&surface, 0xff0000, 0x00ff00, 2));
    assert(!mulib_framebuffer_verify(&surface, 0xff0000, 0x00ff00, 2));
    return 0;
}
