/* Hosted TUI rendering example. Replace front_cells with a mapped VGA text
 * aperture in a kernel. Only MULIB_IMPL_VGABUFFER is required. */
#define MULIB_IMPL_VGABUFFER
#include "mulib-vgabuffer.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    uint16_t back_cells[80 * 25], front_cells[80 * 25];
    mulib_vgabuffer_t back, front, log;
    mulib_vgabuffer_console_t console;
    mulib_vgabuffer_rect_t screen = {0, 0, 80, 25};
    mulib_vgabuffer_rect_t panel = {2, 2, 76, 21};
    mulib_vgabuffer_rect_t title = {4, 2, 30, 1};
    mulib_vgabuffer_rect_t menu = {4, 4, 18, 6};
    mulib_vgabuffer_rect_t selected = {4, 5, 18, 1};
    mulib_vgabuffer_rect_t log_rect = {25, 4, 50, 16};
    uint8_t normal = mulib_vgabuffer_attribute(MULIB_VGABUFFER_LIGHT_GRAY, MULIB_VGABUFFER_BLUE);
    uint8_t highlight = mulib_vgabuffer_attribute(MULIB_VGABUFFER_BLACK, MULIB_VGABUFFER_LIGHT_GRAY);
    const char title_text[] = " MuLIB VGA dashboard ";
    const char menu_text[] = "Overview\nSystem log\nDevices\nSettings";
    size_t x, y;
    assert(mulib_vgabuffer_init(&back, back_cells, 80 * 25, 80, 25, 80) == 0);
    assert(mulib_vgabuffer_init(&front, front_cells, 80 * 25, 80, 25, 80) == 0);
    assert(mulib_vgabuffer_clear(&back, mulib_vgabuffer_cell(' ', normal)) == 0);
    assert(mulib_vgabuffer_box(&back, panel, &mulib_vgabuffer_border_double, normal) == 0);
    assert(mulib_vgabuffer_text(&back, title, title_text, sizeof title_text - 1, normal) == 0);
    assert(mulib_vgabuffer_text(&back, menu, menu_text, sizeof menu_text - 1, normal) == 0);
    assert(mulib_vgabuffer_restyle(&back, selected, highlight) == 0);
    assert(mulib_vgabuffer_vline(&back, 23, 3, 19, mulib_vgabuffer_cell(179, normal)) == 0);
    assert(mulib_vgabuffer_view(&log, &back, log_rect) == 0);
    assert(mulib_vgabuffer_console_init(&console, &log, normal) == 0);
    for (y = 0; y < 20; ++y) {
        const char message[] = "Device initialized successfully.\n";
        assert(mulib_vgabuffer_console_write(&console, message, sizeof message - 1) == 0);
    }
    /* A progress bar and a complete back-buffer presentation. */
    assert(mulib_vgabuffer_hline(&back, 25, 21, 50, mulib_vgabuffer_cell(176, normal)) == 0);
    assert(mulib_vgabuffer_hline(&back, 25, 21, 35, mulib_vgabuffer_cell(219, normal)) == 0);
    assert(mulib_vgabuffer_blit(&front, 0, 0, &back, screen) == 0);
    assert(front_cells[5 * 80 + 4] == mulib_vgabuffer_cell('S', highlight));
    /* ASCII preview for an ordinary host terminal; actual VGA uses CP437. */
    for (y = 0; y < 25; ++y) {
        for (x = 0; x < 80; ++x) {
            uint8_t ch = (uint8_t)front_cells[y * 80 + x];
            putchar(ch >= 32 && ch < 127 ? ch : '#');
        }
        putchar('\n');
    }
    return 0;
}
