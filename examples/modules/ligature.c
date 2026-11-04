#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_FONTSHAPE
#define MULIB_IMPL_LIGATURE
#include "mulib-ligature.h"
#include <assert.h>
int main(void) {
    mulib_fontshape_glyph_t glyphs[] = {{42, 0, 640, 0, 0, 0}};
    mulib_fontshape_run_t run = {glyphs, 1};
    mulib_ligature_cluster_t *clusters = NULL;
    size_t count;
    assert(!mulib_ligature_clusters("ffi", 3, &run, &clusters, &count));
    assert(count == 1 && clusters[0].length == 3 && clusters[0].scalar_count == 3);
    free(clusters); return 0;
}
