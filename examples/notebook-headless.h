/* Private example interface for the independent headless regression TU. */
#ifndef MULIB_EXAMPLE_NOTEBOOK_HEADLESS_H
#define MULIB_EXAMPLE_NOTEBOOK_HEADLESS_H

#include "mulib-glx-hittest.h"
#include "mulib-glx-window.h"
#include "mulib-immgtk-codeeditor.h"
#include "mulib-nb-virtualize.h"
#include "mulib-nb-window.h"
#include "mulib-retgtk-label.h"

enum { MNB_DEMO_CELLS = 64, MNB_DEMO_WIDTH = 480, MNB_DEMO_HEIGHT = 320 };

typedef struct {
    mulib_retgtk_widget_t *heading;
    mulib_texteditor_t editor;
    uint64_t cell, revision;
} mnb_demo_chrome;

typedef struct {
    uint64_t cell, window, node;
    mgx_surface pixels;
    mulib_glx_clientbuffer buffer;
} mnb_demo_client;

typedef struct {
    uint64_t cell;             /* Logical document cell, or zero. */
    mulib_glx_hit native;      /* Scene node and buffer-local coordinates, if hit. */
} mnb_demo_hit;

/* Single owner thread; zero-initialize and do not move/copy after init. */
typedef struct {
    mulib_nb doc;
    mulib_wm_core wm;
    mulib_glx_scene scene;
    mulib_nb_layout layout;
    mulib_nb_viewport viewport;
    mulib_nb_range visible;
    mgx_rect clip;
    uint64_t clip_node, canvas_node, selected_cell;
    mnb_demo_client clients[2];
    size_t client_count;

    mulib_glx_platform_t platform;
    mulib_glx_window_t output;
    mulib_glx_font_t font;
    mulib_retgtk_widget_t *title, *status;
    mulib_immgtk_native_t immediate;
    mulib_immgtk_io_t io;
    mnb_demo_chrome chrome[MNB_DEMO_CELLS];
    size_t pending_edit; /* One-based index, zero when there is no edit. */
    int ready;
} mnb_demo;

/* NULL path selects the built-in fixture and its fake clients. Loading MNB1
 * creates no clients and never interprets the source. */
int mnb_demo_init(mnb_demo *demo, const char *path);
void mnb_demo_free(mnb_demo *demo);
int mnb_demo_view(mnb_demo *demo, double scroll, double zoom, int w, int h);
int mnb_demo_render(mnb_demo *demo);
/* Input uses the last presented geometry. After a click, edit or view change,
 * render again before accepting another input event. */
int mnb_demo_pick(mnb_demo *demo, double x, double y, mnb_demo_hit *hit);
int mnb_demo_click(mnb_demo *demo, double x, double y);
int mnb_demo_text(mnb_demo *demo, const char *utf8);
const mgx_surface *mnb_demo_frame(const mnb_demo *demo);

#endif
