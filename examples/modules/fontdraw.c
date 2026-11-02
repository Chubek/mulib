#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_SYSFONTS
#define MULIB_IMPL_FONTDRAW
#include "mulib-sysfonts.h"
#include "mulib-fontdraw.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    mulib_sysfonts_t *fonts = mulib_sysfonts_open(NULL);
    mulib_sysfonts_font_t match = {0};
    mulib_fontdraw_t *font;
    mulib_fontdraw_bitmap_t bitmap = {0};
    if (!fonts) { puts("Fontconfig runtime unavailable"); return 0; }
    assert(!mulib_sysfonts_match(fonts, "sans", &match));
    font = mulib_fontdraw_open(match.path, match.index, 32, NULL);
    if (font) {
        assert(!mulib_fontdraw_render(font, mulib_fontdraw_glyph(font, 'A'), 0, 0, &bitmap));
        printf("A: %zux%zu, bearing (%d,%d)\n", bitmap.width, bitmap.height, bitmap.left, bitmap.top);
        mulib_fontdraw_bitmap_destroy(&bitmap); mulib_fontdraw_close(font);
    } else puts("FreeType runtime/font unavailable");
    mulib_sysfonts_font_destroy(&match); mulib_sysfonts_close(fonts); return 0;
}
