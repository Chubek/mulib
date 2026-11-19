/* Private headless Wayland example, not an installed MuLIB interface. */
#ifndef MULIB_EXAMPLE_WL_HEADLESS_PRIVATE_H
#define MULIB_EXAMPLE_WL_HEADLESS_PRIVATE_H
#include "mulib-glx-frameclock.h"
#include "mulib-wl-server.h"
#include "mulib-wl-xdgshell.h"

enum {
    WL_HEADLESS_CLIENTS=4,
    WL_HEADLESS_SURFACES=16,
    WL_HEADLESS_OBJECTS=128,
    WL_HEADLESS_BUDGET=8,
    WL_HEADLESS_W=320,
    WL_HEADLESS_H=200
};
#define WL_HEADLESS_BG UINT32_C(0xff202630)

typedef struct {
    mulib_wl_server server;
    mulib_wl_compositor compositor;
    mulib_wl_xdg_server xdg;
    mulib_wl_client clients[WL_HEADLESS_CLIENTS];
    mulib_wl_quota quota;
    mulib_glx_frameclock clock;
    mgx_surface output;
    int pending_dispatch[WL_HEADLESS_CLIENTS];
    size_t turn, mapped;
    uint64_t ids[WL_HEADLESS_SURFACES], revisions[WL_HEADLESS_SURFACES];
    uint64_t frames;
    char runtime[sizeof(((struct sockaddr_un*)0)->sun_path)];
} wl_headless_private;

/* Single owner. Initialize into an empty object, then close once finished. */
int wl_headless_private_open(wl_headless_private *host,const char *temp_parent);
int wl_headless_private_step(wl_headless_private *host,int timeout_ms);
void wl_headless_private_close(wl_headless_private *host);
#endif
