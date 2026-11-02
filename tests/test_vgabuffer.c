#include "mulib-vgabuffer.h"
#define MULIB_IMPL_VGABUFFER
#include "mulib-vgabuffer.h"
#include "mulib-vgabuffer.h"
#include <assert.h>
#include <string.h>

static mulib_vgabuffer_rect_t rect(size_t x, size_t y, size_t w, size_t h) {
    mulib_vgabuffer_rect_t r = {x, y, w, h};
    return r;
}
static void bounds_and_drawing(void) {
    uint16_t storage[32], before[32], cell = 0x1234;
    mulib_vgabuffer_t s, saved, view;
    size_t i, x, y;
    for (i = 0; i < 32; ++i) storage[i] = 0xdead;
    assert(mulib_vgabuffer_init(&s, storage + 1, 22, 4, 4, 6) == 0);
    saved = s;
    assert(!mulib_vgabuffer_valid(NULL));
    assert(mulib_vgabuffer_init(NULL, storage, 32, 4, 4, 6) == -1);
    assert(mulib_vgabuffer_init(&s, NULL, 32, 4, 4, 6) == -1);
    assert(mulib_vgabuffer_init(&s, storage, 21, 4, 4, 6) == -1);
    assert(mulib_vgabuffer_init(&s, storage, 32, 0, 4, 6) == -1);
    assert(mulib_vgabuffer_init(&s, storage, 32, 4, 0, 6) == -1);
    assert(mulib_vgabuffer_init(&s, storage, 32, 4, 4, 3) == -1);
    assert(mulib_vgabuffer_init(&s, storage, SIZE_MAX, 4, 4, 6) == -1);
    assert(mulib_vgabuffer_init(&s, storage, 32, 4, SIZE_MAX, SIZE_MAX) == -1);
    assert(s.cells == saved.cells && s.capacity == saved.capacity && s.width == saved.width);
    assert(mulib_vgabuffer_attribute(15, 1) == 0x1f);
    assert(mulib_vgabuffer_attribute(0x12, 0x18) == 0x82);
    assert(mulib_vgabuffer_cell(0xdb, 0x9e) == 0x9edb);
    assert(mulib_vgabuffer_clear(&s, 0x0720) == 0);
    for (i = 0; i < 32; ++i) {
        int visible = i >= 1 && i < 23 && (i - 1) % 6 < 4;
        assert(storage[i] == (visible ? 0x0720 : 0xdead));
    }
    memcpy(before, storage, sizeof storage);
    assert(mulib_vgabuffer_get(&s, SIZE_MAX, 0, &cell) == -1 && cell == 0x1234);
    assert(mulib_vgabuffer_get(&s, 0, 0, NULL) == -1);
    assert(mulib_vgabuffer_put(&s, 0, 4, 1) == -1);
    assert(mulib_vgabuffer_fill(&s, rect(SIZE_MAX, 0, SIZE_MAX, SIZE_MAX), 0) == 0);
    assert(mulib_vgabuffer_fill(&s, rect(0, 0, 0, SIZE_MAX), 0) == 0);
    assert(mulib_vgabuffer_box(&s, rect(0, 0, 1, 4), &mulib_vgabuffer_border_single, 7) == -1);
    assert(mulib_vgabuffer_text(&s, rect(0, 0, 4, 4), NULL, 1, 7) == -1);
    assert(mulib_vgabuffer_text(&s, rect(0, 0, 4, 4), NULL, 0, 7) == 0);
    assert(memcmp(before, storage, sizeof storage) == 0);
    assert(mulib_vgabuffer_fill(&s, rect(2, 2, SIZE_MAX, SIZE_MAX), 0x1258) == 0);
    assert(mulib_vgabuffer_restyle(&s, rect(3, 3, SIZE_MAX, SIZE_MAX), 0x4f) == 0);
    assert(mulib_vgabuffer_get(&s, 3, 3, &cell) == 0 && cell == 0x4f58);
    assert(mulib_vgabuffer_view(&view, &s, rect(1, 1, 2, 2)) == 0);
    assert(mulib_vgabuffer_clear(&view, 0x0341) == 0);
    assert(mulib_vgabuffer_view(&view, &s, rect(3, 0, 2, 2)) == -1);
    assert(mulib_vgabuffer_view(&view, &view, rect(1, 1, 1, 1)) == 0);
    assert(mulib_vgabuffer_put(&view, 0, 0, 0x425a) == 0);
    assert(mulib_vgabuffer_get(&s, 2, 2, &cell) == 0 && cell == 0x425a);
    assert(mulib_vgabuffer_clear(&s, 0) == 0);
    assert(mulib_vgabuffer_box(&s, rect(0, 0, 4, 4), &mulib_vgabuffer_border_single, 7) == 0);
    assert(s.cells[0] == 0x07da && s.cells[3] == 0x07bf);
    assert(s.cells[18] == 0x07c0 && s.cells[21] == 0x07d9);
    assert(s.cells[1] == 0x07c4 && s.cells[6] == 0x07b3 && s.cells[7] == 0);
    assert(mulib_vgabuffer_clear(&s, 0) == 0);
    assert(mulib_vgabuffer_box(&s, rect(1, 1, SIZE_MAX, SIZE_MAX), &mulib_vgabuffer_border_double, 3) == 0);
    assert(s.cells[7] == 0x03c9 && s.cells[9] == 0x03cd);
    assert(s.cells[19] == 0x03ba && s.cells[21] == 0); /* No artificial clipped edges. */
    assert(mulib_vgabuffer_hline(&s, 2, 0, SIZE_MAX, 0x11) == 0);
    assert(mulib_vgabuffer_vline(&s, 0, 2, SIZE_MAX, 0x22) == 0);
    for (y = 0; y < 4; ++y) for (x = 4; x < 6; ++x)
        assert(storage[1 + y * 6 + x] == 0xdead);
    assert(storage[0] == 0xdead && storage[25] == 0xdead);
}

/* Compare every overlapping move in a small pitched surface against an
 * immutable snapshot, including moves that clip at either set of bounds. */
static void copies_and_scroll(void) {
    uint16_t data[30], original[30], expected[30], other[21];
    mulib_vgabuffer_t s, d, view;
    size_t sx, sy, dx, dy, w, h, x, y, i, rows;
    int down;
    assert(mulib_vgabuffer_init(&s, data, 30, 4, 4, 6) == 0);
    for (i = 0; i < 30; ++i) original[i] = (uint16_t)(0x2000 + i);
    for (sy = 0; sy < 5; ++sy) for (sx = 0; sx < 5; ++sx)
    for (dy = 0; dy < 5; ++dy) for (dx = 0; dx < 5; ++dx)
    for (h = 0; h < 6; ++h) for (w = 0; w < 6; ++w) {
        memcpy(data, original, sizeof data); memcpy(expected, original, sizeof data);
        for (y = 0; y < h; ++y) for (x = 0; x < w; ++x)
            if (sx + x < 4 && sy + y < 4 && dx + x < 4 && dy + y < 4)
                expected[(dy + y) * 6 + dx + x] = original[(sy + y) * 6 + sx + x];
        assert(mulib_vgabuffer_blit(&s, dx, dy, &s, rect(sx, sy, w, h)) == 0);
        assert(memcmp(data, expected, sizeof data) == 0);
    }
    memcpy(data, original, sizeof data);
    for (i = 0; i < 21; ++i) other[i] = 0xbeef;
    assert(mulib_vgabuffer_init(&d, other, 21, 3, 4, 5) == 0);
    assert(mulib_vgabuffer_blit(&d, 1, 1, &s, rect(1, 0, SIZE_MAX, SIZE_MAX)) == 0);
    for (y = 0; y < 4; ++y) for (x = 0; x < 5; ++x)
        assert(other[y * 5 + x] == (y >= 1 && x >= 1 && x < 3 ? original[(y - 1) * 6 + x] : 0xbeef));
    assert(mulib_vgabuffer_view(&view, &s, rect(1, 0, 3, 4)) == 0);
    assert(mulib_vgabuffer_blit(&view, 0, 0, &s, rect(0, 0, 2, 2)) == -1);
    assert(memcmp(data, original, sizeof data) == 0);
    for (down = 0; down <= 1; ++down) for (rows = 0; rows < 5; ++rows) {
        memcpy(data, original, sizeof data); memcpy(expected, original, sizeof data);
        for (y = 1; y < 4; ++y) for (x = 1; x < 3; ++x) {
            size_t from = down ? y - rows : y + rows;
            expected[y * 6 + x] = rows >= 3 || (down ? y < 1 + rows : from >= 4) ?
                                  0x0720 : original[from * 6 + x];
        }
        assert(mulib_vgabuffer_scroll(&s, rect(1, 1, 2, SIZE_MAX), rows, down, 0x0720) == 0);
        assert(memcmp(data, expected, sizeof data) == 0);
    }
    assert(mulib_vgabuffer_scroll(&s, rect(0, 0, 4, 4), SIZE_MAX, 0, 0) == 0);
    for (y = 0; y < 4; ++y) for (x = 0; x < 4; ++x) assert(data[y * 6 + x] == 0);
    assert(mulib_vgabuffer_scroll(&s, rect(0, 0, 4, 4), 1, 2, 0) == -1);
}

static void text_and_console(void) {
    uint16_t data[18], second[18];
    mulib_vgabuffer_t s, t;
    mulib_vgabuffer_console_t c, d;
    const char stream[] = "12345\nabc\rZ\tQ\b!\n0123456789\nlast";
    const char raw[] = {(char)0xdb, 0};
    size_t i, split;
    assert(mulib_vgabuffer_init(&s, data, 18, 5, 3, 6) == 0);
    assert(mulib_vgabuffer_init(&t, second, 18, 5, 3, 6) == 0);
    for (i = 0; i < 18; ++i) data[i] = second[i] = 0xffff;
    assert(mulib_vgabuffer_clear(&s, 0x0720) == 0);
    assert(mulib_vgabuffer_text(&s, rect(0, 0, 5, 3), "ABCDE\nF\rG\tZ", 11, 7) == 0);
    assert(data[4] == 0x0745 && data[6] == 0x0747 && data[15] == 0x075a);
    assert(data[5] == 0xffff && data[11] == 0xffff && data[17] == 0xffff);
    assert(mulib_vgabuffer_text(&s, rect(4, 2, SIZE_MAX, SIZE_MAX), raw, 2, 0x81) == 0);
    assert(data[16] == 0x81db); /* Second byte clipped; no out-of-bounds access. */
    assert(mulib_vgabuffer_text(&s, rect(0, 0, 2, 1), raw, 2, 0x81) == 0);
    assert(data[0] == 0x81db && data[1] == 0x8100);
    assert(mulib_vgabuffer_clear(&s, 0x0720) == 0);
    assert(mulib_vgabuffer_console_init(&c, &s, 7) == 0);
    assert(mulib_vgabuffer_console_move(&c, 0, 2) == 0);
    assert(mulib_vgabuffer_console_write(&c, "12345", 5) == 0);
    assert(c.wrap_pending && c.x == 4 && c.y == 2 && data[12] == 0x0731);
    assert(mulib_vgabuffer_console_write(&c, "\bX", 2) == 0 && data[16] == 0x0758);
    assert(mulib_vgabuffer_console_write(&c, "Y", 1) == 0);
    assert(data[6] == 0x0731 && data[10] == 0x0758 && data[12] == 0x0759 && data[13] == 0x0720);
    assert(mulib_vgabuffer_console_write(&c, NULL, 0) == 0);
    assert(mulib_vgabuffer_console_write(&c, NULL, 1) == -1);
    assert(mulib_vgabuffer_console_move(&c, 5, 0) == -1);
    c.tab_width = 0;
    assert(mulib_vgabuffer_console_write(&c, "X", 1) == -1);
    /* Every chunk boundary must produce identical screen and cursor state. */
    for (split = 0; split < sizeof stream; ++split) {
        assert(mulib_vgabuffer_clear(&s, 0x0720) == 0);
        assert(mulib_vgabuffer_clear(&t, 0x0720) == 0);
        assert(mulib_vgabuffer_console_init(&c, &s, 7) == 0);
        assert(mulib_vgabuffer_console_init(&d, &t, 7) == 0);
        assert(mulib_vgabuffer_console_write(&c, stream, sizeof stream - 1) == 0);
        assert(mulib_vgabuffer_console_write(&d, stream, split) == 0);
        assert(mulib_vgabuffer_console_write(&d, stream + split, sizeof stream - 1 - split) == 0);
        assert(memcmp(data, second, sizeof data) == 0);
        assert(c.x == d.x && c.y == d.y && c.wrap_pending == d.wrap_pending);
    }
    assert(mulib_vgabuffer_view(&t, &s, rect(2, 1, 1, 1)) == 0);
    assert(mulib_vgabuffer_console_init(&c, &t, 0x1f) == 0);
    assert(mulib_vgabuffer_console_write(&c, "AB", 2) == 0 && data[8] == 0x1f42);
    assert(mulib_vgabuffer_console_write(&c, "\n", 1) == 0 && data[8] == 0x1f20);
}

typedef struct mock_io {
    uint16_t port;
    uint8_t index, registers[256];
    size_t calls, fail_at;
} mock_io_t;
static int write8(void *context, uint16_t port, uint8_t value) {
    mock_io_t *m = (mock_io_t *)context;
    if (++m->calls == m->fail_at) return -1;
    assert(port == m->port || port == m->port + 1);
    if (port == m->port) m->index = value;
    else m->registers[m->index] = value;
    return 0;
}
static int read8(void *context, uint16_t port, uint8_t *value) {
    mock_io_t *m = (mock_io_t *)context;
    if (++m->calls == m->fail_at) return -1;
    assert(port == m->port + 1);
    *value = m->registers[m->index];
    return 0;
}
static void cursor_io(void) {
    mock_io_t m;
    mulib_vgabuffer_io_t io = {&m, read8, write8};
    size_t fail;
    memset(&m, 0, sizeof m); m.port = MULIB_VGABUFFER_COLOR_CRTC;
    m.registers[0x0a] = 0xe1; m.registers[0x0b] = 0xa3;
    assert(mulib_vgabuffer_cursor(&io, m.port, 0x1234, 6, 15, 1) == 0);
    assert(m.calls == 12 && m.registers[0x0e] == 0x12 && m.registers[0x0f] == 0x34);
    assert(m.registers[0x0a] == 0xc6 && m.registers[0x0b] == 0xaf);
    m.port = MULIB_VGABUFFER_MONO_CRTC;
    assert(mulib_vgabuffer_cursor(&io, m.port, 65535, 0, 31, 0) == 0);
    assert(m.registers[0x0a] == 0xe0 && m.registers[0x0b] == 0xbf);
    m.calls = 0;
    assert(mulib_vgabuffer_cursor(&io, m.port, SIZE_MAX, 0, 1, 1) == -1);
    assert(mulib_vgabuffer_cursor(&io, 0, 0, 0, 1, 1) == -1);
    assert(mulib_vgabuffer_cursor(&io, m.port, 0, 2, 1, 1) == -1);
    assert(mulib_vgabuffer_cursor(&io, m.port, 0, 0, 32, 1) == -1);
    assert(mulib_vgabuffer_cursor(&io, m.port, 0, 0, 1, 2) == -1);
    assert(mulib_vgabuffer_cursor(NULL, m.port, 0, 0, 1, 1) == -1 && m.calls == 0);
    for (fail = 1; fail <= 12; ++fail) {
        m.calls = 0; m.fail_at = fail;
        assert(mulib_vgabuffer_cursor(&io, m.port, 10, 0, 15, 1) == -1);
        assert(m.calls == fail);
    }
}
int main(void) {
    bounds_and_drawing(); copies_and_scroll(); text_and_console(); cursor_io();
    return 0;
}
