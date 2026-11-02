#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_SYSFONTS
#define MULIB_IMPL_FONTSHAPE
#include "mulib-sysfonts.h"
#include "mulib-fontshape.h"
#include <assert.h>
int main(void) {
    mulib_sysfonts_t *fonts = mulib_sysfonts_open(NULL);
    mulib_sysfonts_font_t match = {0};
    mulib_fontshape_t *font;
    mulib_fontshape_run_t run = {0};
    size_t i;
    if (!fonts) { puts("Fontconfig runtime unavailable"); return 0; }
    assert(!mulib_sysfonts_match(fonts, "sans", &match));
    font = mulib_fontshape_open(match.path, (unsigned)match.index, 16*64, NULL);
    if (font) {
        assert(!mulib_fontshape_shape(font, "office", 6, NULL, &run));
        for (i = 0; i < run.count; ++i)
            printf("glyph %u cluster %u advance %.2fpx\n", (unsigned)run.glyphs[i].glyph,
                   (unsigned)run.glyphs[i].cluster, run.glyphs[i].x_advance/64.0);
        mulib_fontshape_run_destroy(&run); mulib_fontshape_close(font);
    } else puts("HarfBuzz runtime/font unavailable");
    mulib_sysfonts_font_destroy(&match); mulib_sysfonts_close(fonts); return 0;
}
