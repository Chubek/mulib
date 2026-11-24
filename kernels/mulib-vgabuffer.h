/**
 * @file mulib-vgabuffer.h
 * @brief Allocation-free VGA text cells, clipped drawing, windows and consoles.
 *
 * Define MULIB_IMPL_VGABUFFER in one translation unit. No dependencies or libc
 * calls. Surfaces borrow mapped VGA text memory or RAM; dimensions and strides
 * are in 16-bit cells. The caller configures the text mode, font, blink mode,
 * mapping and synchronization. See manual/mulib-vgabuffer.md.
 * Functions returning int return 0 on success and -1 on invalid input, except
 * valid which returns 1/0. Invalid drawing calls do not modify storage. No allocation
 * or ownership transfer occurs; the caller keeps borrowed storage alive.
 */
#ifndef MULIB_VGABUFFER_H
#define MULIB_VGABUFFER_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define MULIB_VGABUFFER_COLOR_ADDRESS UINT32_C(0xb8000)
#define MULIB_VGABUFFER_MONO_ADDRESS UINT32_C(0xb0000)
#define MULIB_VGABUFFER_COLOR_CRTC UINT16_C(0x3d4)
#define MULIB_VGABUFFER_MONO_CRTC UINT16_C(0x3b4)
#define MULIB_VGABUFFER_WIDTH 80
#define MULIB_VGABUFFER_HEIGHT 25

/** @brief Standard VGA palette indices (actual colors depend on the palette). */
typedef enum mulib_vgabuffer_color {
    MULIB_VGABUFFER_BLACK, MULIB_VGABUFFER_BLUE, MULIB_VGABUFFER_GREEN,
    MULIB_VGABUFFER_CYAN, MULIB_VGABUFFER_RED, MULIB_VGABUFFER_MAGENTA,
    MULIB_VGABUFFER_BROWN, MULIB_VGABUFFER_LIGHT_GRAY, MULIB_VGABUFFER_DARK_GRAY,
    MULIB_VGABUFFER_LIGHT_BLUE, MULIB_VGABUFFER_LIGHT_GREEN,
    MULIB_VGABUFFER_LIGHT_CYAN, MULIB_VGABUFFER_LIGHT_RED,
    MULIB_VGABUFFER_LIGHT_MAGENTA, MULIB_VGABUFFER_YELLOW, MULIB_VGABUFFER_WHITE
} mulib_vgabuffer_color_t;

/** @brief Raw cell: glyph in bits 0..7, attribute in bits 8..15. */
typedef uint16_t mulib_vgabuffer_cell_t;
/** @brief Borrowed storage; do not change fields except by initialization. */
typedef struct mulib_vgabuffer {
    volatile mulib_vgabuffer_cell_t *cells;
    size_t capacity, width, height, stride;
} mulib_vgabuffer_t;
/** @brief Unsigned rectangle; extents are clipped without overflowing sums. */
typedef struct mulib_vgabuffer_rect {
    size_t x, y, width, height;
} mulib_vgabuffer_rect_t;
/** @brief Border glyphs in the active font (built-ins assume CP437). */
typedef struct mulib_vgabuffer_border {
    uint8_t horizontal, vertical, top_left, top_right, bottom_left, bottom_right;
} mulib_vgabuffer_border_t;
extern const mulib_vgabuffer_border_t mulib_vgabuffer_border_ascii;
extern const mulib_vgabuffer_border_t mulib_vgabuffer_border_single;
extern const mulib_vgabuffer_border_t mulib_vgabuffer_border_double;

/** @brief Borrowed console window, zero-based cursor, delayed line wrap. */
typedef struct mulib_vgabuffer_console {
    mulib_vgabuffer_t surface;
    size_t x, y, tab_width;
    uint8_t attribute;
    unsigned wrap_pending;
} mulib_vgabuffer_console_t;
/** @brief Caller-supplied byte port I/O; callbacks return 0 or -1 on failure. */
typedef struct mulib_vgabuffer_io {
    void *context;
    int (*read8)(void *context, uint16_t port, uint8_t *value);
    int (*write8)(void *context, uint16_t port, uint8_t value);
} mulib_vgabuffer_io_t;

/** @brief Pack the low four bits of each color. Bit 7 means blink when enabled. */
uint8_t mulib_vgabuffer_attribute(unsigned foreground, unsigned background);
/** @brief Pack a glyph byte and a raw attribute byte without interpretation. */
mulib_vgabuffer_cell_t mulib_vgabuffer_cell(uint8_t glyph, uint8_t attribute);
/** @brief Validate nonempty dimensions, alignment, capacity and arithmetic. */
int mulib_vgabuffer_valid(const mulib_vgabuffer_t *surface);
/** @brief Borrow storage without clearing it; return 0/-1, unchanged on failure. */
int mulib_vgabuffer_init(mulib_vgabuffer_t *surface,
                         volatile mulib_vgabuffer_cell_t *cells, size_t capacity,
                         size_t width, size_t height, size_t stride);
/** @brief Borrow a nonempty, fully contained window; output may equal parent. */
int mulib_vgabuffer_view(mulib_vgabuffer_t *view,
                         const mulib_vgabuffer_t *parent, mulib_vgabuffer_rect_t rect);
/** @brief Read one cell, returning 0/-1; output is unchanged on failure. */
int mulib_vgabuffer_get(const mulib_vgabuffer_t *surface, size_t x, size_t y,
                        mulib_vgabuffer_cell_t *cell);
/** @brief Write one cell; out-of-bounds coordinates return -1. */
int mulib_vgabuffer_put(mulib_vgabuffer_t *surface, size_t x, size_t y,
                        mulib_vgabuffer_cell_t cell);
/** @brief Fill a clipped rectangle; empty/offscreen rectangles succeed. */
int mulib_vgabuffer_fill(mulib_vgabuffer_t *surface, mulib_vgabuffer_rect_t rect,
                         mulib_vgabuffer_cell_t cell);
/** @brief Fill all visible cells, leaving row padding untouched. */
int mulib_vgabuffer_clear(mulib_vgabuffer_t *surface, mulib_vgabuffer_cell_t cell);
/** @brief Replace attributes in a clipped rectangle while preserving glyphs. */
int mulib_vgabuffer_restyle(mulib_vgabuffer_t *surface, mulib_vgabuffer_rect_t rect,
                            uint8_t attribute);
/** @brief Draw a clipped horizontal line. */
int mulib_vgabuffer_hline(mulib_vgabuffer_t *surface, size_t x, size_t y,
                          size_t length, mulib_vgabuffer_cell_t cell);
/** @brief Draw a clipped vertical line. */
int mulib_vgabuffer_vline(mulib_vgabuffer_t *surface, size_t x, size_t y,
                          size_t length, mulib_vgabuffer_cell_t cell);
/** @brief Outline a rectangle at its original edges; width/height must be >=2. */
int mulib_vgabuffer_box(mulib_vgabuffer_t *surface, mulib_vgabuffer_rect_t rect,
                        const mulib_vgabuffer_border_t *border, uint8_t attribute);
/**
 * @brief Draw length bytes in a clipped text area, with wrapping but no scrolling.
 * Layout uses the clipped dimensions. LF starts a row, CR returns to column zero,
 * TAB inserts spaces to an eight-column stop; other bytes are literal glyphs.
 * Filling a row delays wrap until the next glyph; excess text is discarded.
 * NULL text is valid only with zero length. Text must not alias destination.
 */
int mulib_vgabuffer_text(mulib_vgabuffer_t *surface, mulib_vgabuffer_rect_t rect,
                         const char *text, size_t length, uint8_t attribute);
/**
 * @brief Copy a clipped source rectangle to destination x/y, preserving raw cells.
 * Same-base, same-stride overlap is memmove-safe. Other overlapping storage
 * spans are rejected before writing. Nonoverlapping buffers may have any stride.
 * Source/destination clipping reduces width/height without moving either origin.
 */
int mulib_vgabuffer_blit(mulib_vgabuffer_t *destination, size_t x, size_t y,
                         const mulib_vgabuffer_t *source, mulib_vgabuffer_rect_t rect);
/** @brief Scroll a clipped region up (down=0) or down (down=1); fill exposed rows. */
int mulib_vgabuffer_scroll(mulib_vgabuffer_t *surface, mulib_vgabuffer_rect_t rect,
                           size_t rows, int down, mulib_vgabuffer_cell_t fill);
/** @brief Initialize at (0,0), tab width min(8, width); does not clear storage. */
int mulib_vgabuffer_console_init(mulib_vgabuffer_console_t *console,
                                 const mulib_vgabuffer_t *surface, uint8_t attribute);
/** @brief Move the software cursor in bounds and cancel delayed wrap. */
int mulib_vgabuffer_console_move(mulib_vgabuffer_console_t *console, size_t x, size_t y);
/**
 * @brief Stream CP437 bytes, scrolling at the bottom; returns 0/-1.
 * LF/CR/TAB behave as in text; BS cancels pending wrap or moves left without
 * erasing or crossing rows. Other bytes are literal. State survives split writes.
 * Caller may set attribute and nonzero tab_width (at most surface.width).
 * NULL text requires zero length; text must not alias destination storage.
 */
int mulib_vgabuffer_console_write(mulib_vgabuffer_console_t *console,
                                  const char *text, size_t length);
/**
 * @brief Program CRTC cursor location and shape through callbacks; returns 0/-1.
 * crtc is 0x3d4 or 0x3b4; position is an absolute 16-bit text-memory cell offset
 * including display-page origin. Scanlines satisfy start <= end <= 31. visible
 * is 0/1. Preserves unrelated shape bits. I/O failure can leave partial hardware
 * changes; caller serializes index/data access. No privileged instructions here.
 */
int mulib_vgabuffer_cursor(const mulib_vgabuffer_io_t *io, uint16_t crtc,
                           size_t position, unsigned start, unsigned end, int visible);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_VGABUFFER_H */

#if defined(MULIB_IMPL_VGABUFFER) && !defined(MULIB_VGABUFFER_IMPLEMENTED)
#define MULIB_VGABUFFER_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif

const mulib_vgabuffer_border_t mulib_vgabuffer_border_ascii = {'-', '|', '+', '+', '+', '+'};
const mulib_vgabuffer_border_t mulib_vgabuffer_border_single = {196, 179, 218, 191, 192, 217};
const mulib_vgabuffer_border_t mulib_vgabuffer_border_double = {205, 186, 201, 187, 200, 188};

uint8_t mulib_vgabuffer_attribute(unsigned fg, unsigned bg) {
    return (uint8_t)((fg & 15u) | ((bg & 15u) << 4));
}
mulib_vgabuffer_cell_t mulib_vgabuffer_cell(uint8_t glyph, uint8_t attr) {
    return (mulib_vgabuffer_cell_t)((uint16_t)glyph | ((uint16_t)attr << 8));
}
int mulib_vgabuffer_valid(const mulib_vgabuffer_t *s) {
    if (!s || !s->cells || (uintptr_t)s->cells % sizeof(*s->cells) ||
        !s->width || !s->height || s->stride < s->width ||
        s->capacity > SIZE_MAX / sizeof(*s->cells) || s->width > s->capacity)
        return 0;
    if (s->height - 1 > (s->capacity - s->width) / s->stride) return 0;
    return s->capacity <= (UINTPTR_MAX - (uintptr_t)s->cells) / sizeof(*s->cells);
}
int mulib_vgabuffer_init(mulib_vgabuffer_t *s, volatile mulib_vgabuffer_cell_t *cells,
                         size_t capacity, size_t width, size_t height, size_t stride) {
    mulib_vgabuffer_t next;
    if (!s) return -1;
    next.cells = cells; next.capacity = capacity;
    next.width = width; next.height = height; next.stride = stride;
    if (!mulib_vgabuffer_valid(&next)) return -1;
    *s = next;
    return 0;
}
int mulib_vgabuffer_view(mulib_vgabuffer_t *view, const mulib_vgabuffer_t *s,
                         mulib_vgabuffer_rect_t r) {
    size_t offset;
    if (!view || !mulib_vgabuffer_valid(s) || !r.width || !r.height ||
        r.x >= s->width || r.y >= s->height ||
        r.width > s->width - r.x || r.height > s->height - r.y) return -1;
    offset = r.y * s->stride + r.x;
    return mulib_vgabuffer_init(view, s->cells + offset, s->capacity - offset,
                                r.width, r.height, s->stride);
}
int mulib_vgabuffer_get(const mulib_vgabuffer_t *s, size_t x, size_t y,
                        mulib_vgabuffer_cell_t *cell) {
    if (!cell || !mulib_vgabuffer_valid(s) || x >= s->width || y >= s->height) return -1;
    *cell = s->cells[y * s->stride + x];
    return 0;
}
int mulib_vgabuffer_put(mulib_vgabuffer_t *s, size_t x, size_t y, mulib_vgabuffer_cell_t cell) {
    if (!mulib_vgabuffer_valid(s) || x >= s->width || y >= s->height) return -1;
    s->cells[y * s->stride + x] = cell;
    return 0;
}
static mulib_vgabuffer_rect_t mulib_vgabuffer_clip_(const mulib_vgabuffer_t *s,
                                                  mulib_vgabuffer_rect_t r) {
    if (r.x >= s->width || r.y >= s->height) { r.width = r.height = 0; return r; }
    if (r.width > s->width - r.x) r.width = s->width - r.x;
    if (r.height > s->height - r.y) r.height = s->height - r.y;
    return r;
}
static void mulib_vgabuffer_fill_(mulib_vgabuffer_t *s, mulib_vgabuffer_rect_t r,
                                  mulib_vgabuffer_cell_t cell) {
    size_t x, y;
    for (y = 0; y < r.height; ++y)
        for (x = 0; x < r.width; ++x)
            s->cells[(r.y + y) * s->stride + r.x + x] = cell;
}
int mulib_vgabuffer_fill(mulib_vgabuffer_t *s, mulib_vgabuffer_rect_t r,
                         mulib_vgabuffer_cell_t cell) {
    if (!mulib_vgabuffer_valid(s)) return -1;
    mulib_vgabuffer_fill_(s, mulib_vgabuffer_clip_(s, r), cell);
    return 0;
}
int mulib_vgabuffer_clear(mulib_vgabuffer_t *s, mulib_vgabuffer_cell_t cell) {
    mulib_vgabuffer_rect_t r;
    if (!mulib_vgabuffer_valid(s)) return -1;
    r.x = r.y = 0; r.width = s->width; r.height = s->height;
    mulib_vgabuffer_fill_(s, r, cell);
    return 0;
}
int mulib_vgabuffer_restyle(mulib_vgabuffer_t *s, mulib_vgabuffer_rect_t r, uint8_t attr) {
    size_t x, y, i;
    if (!mulib_vgabuffer_valid(s)) return -1;
    r = mulib_vgabuffer_clip_(s, r);
    for (y = 0; y < r.height; ++y) for (x = 0; x < r.width; ++x) {
        i = (r.y + y) * s->stride + r.x + x;
        s->cells[i] = mulib_vgabuffer_cell((uint8_t)s->cells[i], attr);
    }
    return 0;
}
int mulib_vgabuffer_hline(mulib_vgabuffer_t *s, size_t x, size_t y, size_t n,
                          mulib_vgabuffer_cell_t cell) {
    mulib_vgabuffer_rect_t r = {x, y, n, 1};
    return mulib_vgabuffer_fill(s, r, cell);
}
int mulib_vgabuffer_vline(mulib_vgabuffer_t *s, size_t x, size_t y, size_t n,
                          mulib_vgabuffer_cell_t cell) {
    mulib_vgabuffer_rect_t r = {x, y, 1, n};
    return mulib_vgabuffer_fill(s, r, cell);
}
int mulib_vgabuffer_box(mulib_vgabuffer_t *s, mulib_vgabuffer_rect_t r,
                        const mulib_vgabuffer_border_t *b, uint8_t attr) {
    mulib_vgabuffer_rect_t clipped;
    size_t x, y;
    uint8_t glyph;
    int right, bottom;
    if (!b || !mulib_vgabuffer_valid(s) || r.width < 2 || r.height < 2) return -1;
    clipped = mulib_vgabuffer_clip_(s, r);
    /* Iterate only visible edges, even for SIZE_MAX extents. */
    for (y = 0; y < clipped.height; ++y) {
        bottom = y == r.height - 1;
        if (y == 0 || bottom) {
            for (x = 0; x < clipped.width; ++x) {
                right = x == r.width - 1;
                glyph = x == 0 ? (bottom ? b->bottom_left : b->top_left) :
                        right ? (bottom ? b->bottom_right : b->top_right) : b->horizontal;
                s->cells[(r.y + y) * s->stride + r.x + x] = mulib_vgabuffer_cell(glyph, attr);
            }
        } else if (clipped.width) {
            s->cells[(r.y + y) * s->stride + r.x] = mulib_vgabuffer_cell(b->vertical, attr);
            if (clipped.width == r.width)
                s->cells[(r.y + y) * s->stride + r.x + r.width - 1] =
                    mulib_vgabuffer_cell(b->vertical, attr);
        }
    }
    return 0;
}
int mulib_vgabuffer_text(mulib_vgabuffer_t *s, mulib_vgabuffer_rect_t r,
                         const char *text, size_t length, uint8_t attr) {
    size_t i, x = 0, y = 0, count;
    uint8_t ch;
    if ((!text && length) || !mulib_vgabuffer_valid(s)) return -1;
    r = mulib_vgabuffer_clip_(s, r);
    if (!r.width || !r.height) return 0;
    for (i = 0; i < length && y < r.height; ++i) {
        ch = (uint8_t)text[i];
        if (ch == '\r') { x = 0; continue; }
        if (ch == '\n') { x = 0; ++y; continue; }
        if (x == r.width) { x = 0; if (++y == r.height) break; }
        count = ch == '\t' ? 8 - x % 8 : 1;
        if (ch == '\t') ch = ' ';
        while (count--) {
            if (x == r.width) { x = 0; if (++y == r.height) break; }
            s->cells[(r.y + y) * s->stride + r.x + x++] = mulib_vgabuffer_cell(ch, attr);
        }
    }
    return 0;
}
int mulib_vgabuffer_blit(mulib_vgabuffer_t *dst, size_t dx, size_t dy,
                         const mulib_vgabuffer_t *src, mulib_vgabuffer_rect_t r) {
    size_t x, y, from, to, width, height;
    uintptr_t a, b, ae, be;
    int reverse = 0;
    if (!mulib_vgabuffer_valid(dst) || !mulib_vgabuffer_valid(src)) return -1;
    r = mulib_vgabuffer_clip_(src, r);
    if (!r.width || !r.height || dx >= dst->width || dy >= dst->height) return 0;
    width = r.width < dst->width - dx ? r.width : dst->width - dx;
    height = r.height < dst->height - dy ? r.height : dst->height - dy;
    from = r.y * src->stride + r.x; to = dy * dst->stride + dx;
    if (dst->cells == src->cells && dst->stride == src->stride) {
        reverse = to > from;
    } else {
        a = (uintptr_t)src->cells; b = (uintptr_t)dst->cells;
        ae = a + src->capacity * sizeof(*src->cells);
        be = b + dst->capacity * sizeof(*dst->cells);
        if (a < be && b < ae) return -1;
    }
    for (y = 0; y < height; ++y) for (x = 0; x < width; ++x) {
        size_t row = reverse ? height - 1 - y : y;
        size_t col = reverse ? width - 1 - x : x;
        dst->cells[to + row * dst->stride + col] = src->cells[from + row * src->stride + col];
    }
    return 0;
}
int mulib_vgabuffer_scroll(mulib_vgabuffer_t *s, mulib_vgabuffer_rect_t r,
                           size_t rows, int down, mulib_vgabuffer_cell_t fill) {
    mulib_vgabuffer_rect_t source, exposed;
    if (!mulib_vgabuffer_valid(s) || (down != 0 && down != 1)) return -1;
    r = mulib_vgabuffer_clip_(s, r);
    if (!rows || !r.width || !r.height) return 0;
    if (rows >= r.height) { mulib_vgabuffer_fill_(s, r, fill); return 0; }
    source = exposed = r;
    source.height -= rows; exposed.height = rows;
    if (down) {
        (void)mulib_vgabuffer_blit(s, r.x, r.y + rows, s, source);
    } else {
        source.y += rows; exposed.y += r.height - rows;
        (void)mulib_vgabuffer_blit(s, r.x, r.y, s, source);
    }
    mulib_vgabuffer_fill_(s, exposed, fill);
    return 0;
}
static int mulib_vgabuffer_console_valid_(const mulib_vgabuffer_console_t *c) {
    return c && mulib_vgabuffer_valid(&c->surface) && c->x < c->surface.width &&
        c->y < c->surface.height && c->tab_width && c->tab_width <= c->surface.width &&
        c->wrap_pending <= 1 && (!c->wrap_pending || c->x == c->surface.width - 1);
}
int mulib_vgabuffer_console_init(mulib_vgabuffer_console_t *c,
                                 const mulib_vgabuffer_t *s, uint8_t attr) {
    mulib_vgabuffer_console_t next;
    if (!c || !mulib_vgabuffer_valid(s)) return -1;
    next.surface = *s; next.x = next.y = 0; next.attribute = attr; next.wrap_pending = 0;
    next.tab_width = s->width < 8 ? s->width : 8;
    *c = next;
    return 0;
}
int mulib_vgabuffer_console_move(mulib_vgabuffer_console_t *c, size_t x, size_t y) {
    if (!mulib_vgabuffer_console_valid_(c) || x >= c->surface.width || y >= c->surface.height)
        return -1;
    c->x = x; c->y = y; c->wrap_pending = 0;
    return 0;
}
static void mulib_vgabuffer_console_newline_(mulib_vgabuffer_console_t *c) {
    c->x = 0; c->wrap_pending = 0;
    if (c->y == c->surface.height - 1) {
        mulib_vgabuffer_rect_t r = {0, 0, c->surface.width, c->surface.height};
        (void)mulib_vgabuffer_scroll(&c->surface, r, 1, 0, mulib_vgabuffer_cell(' ', c->attribute));
    } else ++c->y;
}
static void mulib_vgabuffer_console_glyph_(mulib_vgabuffer_console_t *c, uint8_t ch) {
    if (c->wrap_pending) mulib_vgabuffer_console_newline_(c);
    c->surface.cells[c->y * c->surface.stride + c->x] = mulib_vgabuffer_cell(ch, c->attribute);
    if (c->x == c->surface.width - 1) c->wrap_pending = 1;
    else ++c->x;
}
int mulib_vgabuffer_console_write(mulib_vgabuffer_console_t *c, const char *text, size_t length) {
    size_t i, spaces;
    uint8_t ch;
    if ((!text && length) || !mulib_vgabuffer_console_valid_(c)) return -1;
    for (i = 0; i < length; ++i) {
        ch = (uint8_t)text[i];
        if (ch == '\n') mulib_vgabuffer_console_newline_(c);
        else if (ch == '\r') { c->x = 0; c->wrap_pending = 0; }
        else if (ch == '\b') {
            if (c->wrap_pending) c->wrap_pending = 0;
            else if (c->x) --c->x;
        } else if (ch == '\t') {
            if (c->wrap_pending) mulib_vgabuffer_console_newline_(c);
            spaces = c->tab_width - c->x % c->tab_width;
            while (spaces--) mulib_vgabuffer_console_glyph_(c, ' ');
        } else mulib_vgabuffer_console_glyph_(c, ch);
    }
    return 0;
}
static int mulib_vgabuffer_crtc_write_(const mulib_vgabuffer_io_t *io, uint16_t port,
                                      uint8_t index, uint8_t value) {
    if (io->write8(io->context, port, index) != 0) return -1;
    return io->write8(io->context, (uint16_t)(port + 1), value) == 0 ? 0 : -1;
}
int mulib_vgabuffer_cursor(const mulib_vgabuffer_io_t *io, uint16_t crtc,
                           size_t position, unsigned start, unsigned end, int visible) {
    uint8_t old_start, old_end;
    if (!io || !io->read8 || !io->write8 ||
        (crtc != MULIB_VGABUFFER_COLOR_CRTC && crtc != MULIB_VGABUFFER_MONO_CRTC) ||
        position > UINT16_MAX || start > end || end > 31 || (visible != 0 && visible != 1))
        return -1;
    if (io->write8(io->context, crtc, 0x0a) != 0 ||
        io->read8(io->context, (uint16_t)(crtc + 1), &old_start) != 0 ||
        io->write8(io->context, crtc, 0x0b) != 0 ||
        io->read8(io->context, (uint16_t)(crtc + 1), &old_end) != 0) return -1;
    if (mulib_vgabuffer_crtc_write_(io, crtc, 0x0e, (uint8_t)(position >> 8)) ||
        mulib_vgabuffer_crtc_write_(io, crtc, 0x0f, (uint8_t)position) ||
        mulib_vgabuffer_crtc_write_(io, crtc, 0x0a,
            (uint8_t)((old_start & 0xc0u) | start | (visible ? 0 : 0x20u))) ||
        mulib_vgabuffer_crtc_write_(io, crtc, 0x0b, (uint8_t)((old_end & 0xe0u) | end)))
        return -1;
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_VGABUFFER_IMPLEMENTED */
