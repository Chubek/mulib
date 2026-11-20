/* Independent small cross-family CPU oracles; not an editor/ABI acceptance test. */
#include "mulib-media-timebase.h"
#include "mulib-media-proxy.h"
#include "mulib-nle-edit.h"
#include "mulib-nle-ripple.h"
#include "mulib-img-blur.h"
#include "mulib-img-blend.h"
#include "mulib-color-transfer.h"
#include "mulib-color-lut3d.h"
#include "mulib-nodegraph-compile.h"
#include "mulib-compute-cpu.h"
#include "mulib-audio-delay.h"
#include "mulib-kernel-validate.h"
#include "mulib-project-journal.h"
#include <assert.h>

static bool count_work(void *context, uint32_t x, uint32_t y, uint32_t z)
{
    (void)x;
    (void)y;
    (void)z;
    (*(unsigned *)context)++;
    return true;
}

static void test_timing(void)
{
    int64_t out = 77;
    assert(mulib_media_timebase_rescale(30000, (mulib_media_rational){1001, 30000},
                                        (mulib_media_rational){1, 48000}, &out));
    assert(out == 48048000);
    assert(mulib_media_timebase_rescale(INT64_MIN, (mulib_media_rational){1, 1},
                                        (mulib_media_rational){1, 1}, &out));
    assert(out == INT64_MIN);
    assert(mulib_media_timebase_rescale(-1, (mulib_media_rational){1, 3},
                                        (mulib_media_rational){1, 2}, &out));
    assert(out == 0); /* Documented truncation toward zero, not floor. */
    out = 77;
    assert(!mulib_media_timebase_rescale(1, (mulib_media_rational){0, 1},
                                         (mulib_media_rational){1, 1}, &out));
    assert(out == 77);
    assert(!mulib_media_timebase_rescale(INT64_MAX, (mulib_media_rational){2, 1},
                                         (mulib_media_rational){1, 1}, &out));
    assert(out == 77);
}

static void test_frames_and_edits(void)
{
    mulib_media_frame frame = {0}, proxy = {0};
    assert(mulib_media_frame_alloc(&frame, 4, 2, MULIB_MEDIA_RGBA8));
    memset(frame.data[0], 255, frame.capacity[0]);
    assert(mulib_media_proxy_make(&proxy, &frame, 2, 2));
    assert(proxy.width == 2 && proxy.height == 1 && proxy.data[0][0] == 255);
    mulib_media_frame_release(&proxy);
    mulib_media_frame_release(&frame);
    mulib_nle_item items[4];
    mulib_nle_timeline timeline = {items, 0, 4, {1, 30}};
    assert(mulib_nle_timeline_insert(&timeline, (mulib_nle_item){1, 2, 3, 10, 20, 5}));
    assert(mulib_nle_edit_split(&timeline, 1, 20, 4));
    assert(timeline.count == 2 && items[0].length == 10 && items[1].source == 15);
    assert(!mulib_nle_ripple_shift(&timeline, 3, 0, -30));
    assert(items[0].start == 10 && items[1].start == 20);
    assert(mulib_nle_ripple_shift(&timeline, 3, 20, 2));
    assert(items[1].start == 22);
}

static void test_image_color(void)
{
    float pixels[16] = {2, 0, 0, 1, 2, 0, 0, 1,
                        2, 0, 0, 1, 2, 0, 0, 1};
    float scratch[16], destination[16];
    mulib_img_view source = {pixels, 2, 2, 8, 16};
    mulib_img_view temporary = {scratch, 2, 2, 8, 16};
    mulib_img_view output = {destination, 2, 2, 8, 16};
    assert(mulib_img_blur_gaussian(output, temporary, source, 2, 1));
    for (size_t i = 0; i < 16; ++i)
        assert(fabs(destination[i] - pixels[i]) < 1e-6);
    mulib_img_rgba c = mulib_img_blend_pixel((mulib_img_rgba){1, 0, 0, 1},
                                            (mulib_img_rgba){0, 0, 1, 0.5f},
                                            MULIB_IMG_OVER);
    assert(fabs(c.r - 0.5) < 1e-6 && fabs(c.b - 0.5) < 1e-6 && c.a == 1);
    for (unsigned i = 0; i <= 100; ++i) {
        double x = i / 100.0;
        double encoded = mulib_color_transfer_encode(MULIB_COLOR_TF_SRGB, x);
        assert(fabs(mulib_color_transfer_decode(MULIB_COLOR_TF_SRGB, encoded) - x) < 1e-12);
    }
    float lut[24];
    for (unsigned i = 0; i < 8; ++i) {
        lut[3 * i] = (i & 1) ? 1 : 0;
        lut[3 * i + 1] = (i & 2) ? 1 : 0;
        lut[3 * i + 2] = (i & 4) ? 1 : 0;
    }
    for (unsigned tetra = 0; tetra < 2; ++tetra)
        for (unsigned i = 0; i < 24; ++i) {
            mulib_img_rgba input = {i / 23.0f, ((i * 7) % 24) / 23.0f,
                                    ((i * 13) % 24) / 23.0f, 0.25f};
            c = mulib_color_lut3d_eval((mulib_color_lut3d){lut, 2, 24}, input, tetra != 0);
            assert(fabs(c.r - input.r) < 1e-6 && fabs(c.g - input.g) < 1e-6 &&
                   fabs(c.b - input.b) < 1e-6 && c.a == input.a);
        }
}

static void test_graph_compute_audio_ir(void)
{
    uint32_t type = 1;
    mulib_nodegraph_node nodes[] = {{1, 1, 1, 0, NULL}, {2, 1, 1, 1, &type}};
    mulib_nodegraph_edge edges[] = {{0, 1, 0}};
    mulib_nodegraph graph = {nodes, 2, edges, 1};
    size_t order[2], degree[2];
    assert(mulib_nodegraph_compile(graph, order, 2, degree, 2));
    assert(order[0] == 0 && order[1] == 1);
    unsigned count = 0;
    assert(mulib_compute_cpu_dispatch((mulib_compute_grid){2, 3, 4}, count_work,
                                      &count, 24) == MULIB_MEDIA_OK && count == 24);
    float ring[2];
    mulib_audio_delay delay;
    assert(mulib_audio_delay_init(&delay, ring, 2, 0, 1));
    assert(mulib_audio_delay_tick(&delay, 1) == 0);
    assert(mulib_audio_delay_tick(&delay, 0) == 0);
    assert(mulib_audio_delay_tick(&delay, 0) == 1);
    mulib_kernel_inst code[] = {
        {MULIB_KERNEL_PARAM, MULIB_KERNEL_F32, 0, 0, 0, 0},
        {MULIB_KERNEL_CONST, MULIB_KERNEL_F32, 0, 0, 0, 2},
        {MULIB_KERNEL_MUL, MULIB_KERNEL_F32, 0, 1, 0, 0}
    };
    mulib_kernel_ir ir = {code, 3, 2, 1};
    float param = 3, scratch[3], result;
    assert(mulib_kernel_eval(ir, &param, 1, scratch, 3, &result) && result == 6);
    assert(!mulib_kernel_eval(ir, &param, 1, scratch, 2, &result));
}

int main(void)
{
    test_timing();
    test_frames_and_edits();
    test_image_color();
    test_graph_compute_audio_ir();
    puts("JoltFX CPU integration oracles passed");
    return 0;
}
