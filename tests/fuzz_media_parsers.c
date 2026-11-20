/* Optional Clang libFuzzer entry point. No filesystem or unbounded recursion.
 * clang -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude -g -O1 \
 *   -fsanitize=fuzzer,address,undefined tests/fuzz_media_parsers.c -lm -o fuzz
 * ./fuzz -runs=5000 -max_len=4096 -seed=42 -timeout=2 -rss_limit_mb=256
 */
#include "mulib-media-isobmff.h"
#include "mulib-media-av1.h"
#include "mulib-media-matroska.h"
#include "mulib-media-mpegts.h"
#include "mulib-media-h264.h"
#include "mulib-media-h265.h"
#include "mulib-media-vp9.h"
#include "mulib-mesh-gltf.h"
#include "mulib-project-journal.h"
#include <assert.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > 4096)
        return 0;
    mulib_media_bytes input = {data, size};
    mulib_media_isobmff_box box;
    mulib_media_av1_obu obu;
    mulib_media_matroska_element element;
    mulib_media_mpegts_packet packet;
    mulib_media_vp9_header vp9;
    mulib_mesh_gltf_glb glb;
    mulib_media_bytes nal, journal;
    size_t offset = 0;
    for (unsigned budget = 0; budget < 32; ++budget) {
        size_t previous = offset;
        if (!mulib_media_isobmff_next(input, &offset, &box))
            break;
        assert(offset > previous && offset <= size);
    }
    offset = 0;
    for (unsigned budget = 0; budget < 32; ++budget) {
        size_t previous = offset;
        if (!mulib_media_av1_next(input, &offset, &obu))
            break;
        assert(offset > previous && offset <= size);
    }
    offset = 0;
    for (unsigned budget = 0; budget < 32; ++budget) {
        size_t previous = offset;
        if (!mulib_media_matroska_next(input, &offset, &element))
            break;
        assert(offset > previous && offset <= size);
    }
    (void)mulib_media_mpegts_parse(data, size, &packet);
    unsigned type, layer, temporal;
    offset = 0;
    (void)mulib_media_h264_nal(input, &offset, &nal, &type);
    assert(offset <= size);
    offset = 0;
    (void)mulib_media_h265_nal(input, &offset, &nal, &type, &layer, &temporal);
    assert(offset <= size);
    (void)mulib_media_vp9_parse(input, &vp9);
    (void)mulib_mesh_gltf_parse_glb(input, &glb);
    offset = 0;
    for (unsigned budget = 0; budget < 32; ++budget) {
        size_t previous = offset;
        if (!mulib_project_journal_next(input, &offset, &journal))
            break;
        assert(offset > previous && offset <= size);
    }
    return 0;
}
