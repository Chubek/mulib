#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_SYSFONTS
#include "mulib-sysfonts.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    mulib_sysfonts_t *fonts = mulib_sysfonts_open(NULL);
    mulib_sysfonts_font_t match = {0};
    if (!fonts) { puts("Fontconfig runtime unavailable"); return 0; }
    assert(!mulib_sysfonts_match(fonts, "sans:lang=en", &match));
    printf("%s %s: %s (face %d)\n", match.family, match.style, match.path, match.index);
    mulib_sysfonts_font_destroy(&match); mulib_sysfonts_close(fonts); return 0;
}
