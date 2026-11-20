/* Deterministic bounded parser probes, not a replacement for corpus fuzzing. */
#include "mulib-media-isobmff.h"
#include "mulib-media-av1.h"
#include "mulib-media-matroska.h"
#include "mulib-media-mpegts.h"
#include "mulib-media-h264.h"
#include "mulib-media-h265.h"
#include "mulib-media-vp9.h"
#include "mulib-mesh-gltf.h"
#include "mulib-project-journal.h"
#include "mulib-project-package.h"
#include <assert.h>

static uint32_t random_word(uint32_t *state)
{
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

static void test_valid_and_truncated(void)
{
    const uint8_t box[] = {0, 0, 0, 8, 'f', 'r', 'e', 'e'};
    const uint8_t obu[] = {0x0a, 1, 42};
    mulib_media_isobmff_box parsed;
    mulib_media_av1_obu av1;
    for (size_t n = 0; n < sizeof box; ++n) {
        size_t offset = 0;
        assert(!mulib_media_isobmff_next((mulib_media_bytes){box, n}, &offset, &parsed));
        assert(offset == 0);
    }
    size_t offset = 0;
    assert(mulib_media_isobmff_next((mulib_media_bytes){box, sizeof box}, &offset, &parsed));
    assert(offset == sizeof box && parsed.payload.size == 0);
    for (size_t n = 0; n < sizeof obu; ++n) {
        offset = 0;
        assert(!mulib_media_av1_next((mulib_media_bytes){obu, n}, &offset, &av1));
    }
    offset = 0;
    assert(mulib_media_av1_next((mulib_media_bytes){obu, sizeof obu}, &offset, &av1));
    assert(offset == sizeof obu && av1.type == 1 && av1.payload.size == 1);
}

static void test_journal(void)
{
    uint8_t storage[32];
    memset(storage, 0xa5, sizeof storage);
    const uint8_t payload[] = {1, 2, 3};
    size_t used = 0;
    assert(mulib_project_journal_append((mulib_media_mutbytes){storage, sizeof storage},
                                        &used, (mulib_media_bytes){payload, sizeof payload}));
    assert(used == 11);
    for (size_t n = 0; n < used; ++n) {
        size_t offset = 0;
        mulib_media_bytes out;
        assert(!mulib_project_journal_next((mulib_media_bytes){storage, n}, &offset, &out));
        assert(offset == 0);
    }
    size_t offset = 0;
    mulib_media_bytes out;
    assert(mulib_project_journal_next((mulib_media_bytes){storage, used}, &offset, &out));
    assert(offset == used && out.size == sizeof payload &&
           memcmp(out.data, payload, sizeof payload) == 0);
    storage[9] ^= 1;
    offset = 0;
    assert(!mulib_project_journal_next((mulib_media_bytes){storage, used}, &offset, &out));
    assert(offset == 0);
    uint8_t before[32];
    memcpy(before, storage, sizeof before);
    size_t unchanged = used;
    assert(!mulib_project_journal_append((mulib_media_mutbytes){storage, used}, &used,
                                         (mulib_media_bytes){payload, sizeof payload}));
    assert(used == unchanged && memcmp(before, storage, sizeof storage) == 0);
    assert(mulib_project_package_path("assets/a.png"));
    const char *bad[] = {"", "../a", "/a", "assets\\a", "a//b", "a/./b", "a/../b"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i)
        assert(!mulib_project_package_path(bad[i]));
}

static void test_bounded_parser_probes(void)
{
    uint8_t bytes[64];
    uint32_t state = 42;
    for (unsigned iteration = 0; iteration < 2048; ++iteration) {
        for (size_t i = 0; i < sizeof bytes; ++i)
            bytes[i] = (uint8_t)(random_word(&state) >> 24);
        size_t n = iteration % (sizeof bytes + 1);
        mulib_media_bytes input = {bytes, n};
        mulib_media_isobmff_box box;
        mulib_media_av1_obu obu;
        mulib_media_matroska_element element;
        mulib_media_mpegts_packet packet;
        mulib_media_vp9_header vp9;
        mulib_mesh_gltf_glb glb;
        mulib_media_bytes nal, journal;
        unsigned type, layer, temporal;
        size_t offset = 0;
        (void)mulib_media_isobmff_next(input, &offset, &box);
        assert(offset <= n);
        offset = 0;
        (void)mulib_media_av1_next(input, &offset, &obu);
        assert(offset <= n);
        offset = 0;
        (void)mulib_media_matroska_next(input, &offset, &element);
        assert(offset <= n);
        (void)mulib_media_mpegts_parse(bytes, n, &packet);
        offset = 0;
        (void)mulib_media_h264_nal(input, &offset, &nal, &type);
        assert(offset <= n);
        offset = 0;
        (void)mulib_media_h265_nal(input, &offset, &nal, &type, &layer, &temporal);
        assert(offset <= n);
        (void)mulib_media_vp9_parse(input, &vp9);
        (void)mulib_mesh_gltf_parse_glb(input, &glb);
        offset = 0;
        (void)mulib_project_journal_next(input, &offset, &journal);
        assert(offset <= n);
    }
}

int main(void)
{
    test_valid_and_truncated();
    test_journal();
    test_bounded_parser_probes();
    puts("Media parser boundaries and 2048 deterministic probes passed");
    return 0;
}
