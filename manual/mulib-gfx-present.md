# Graphics presentation and buffer ownership

Include `mulib-glx-window.h`. These additive C11 APIs are `static inline`;
no implementation switch or third-party runtime is required.

Initialize owning objects empty, do not copy live objects, and keep their
borrowed platform alive until teardown. All calls belong to the rendering
thread. Pixel storage is straight native `0xAARRGGBB`.

## Synchronous and asynchronous platforms

`mulib_glx_platform_headless(&platform)` explicitly selects synchronous,
no-output presentation. A platform without a present callback is otherwise
unsupported: it does not silently report success.

A synchronous callback must finish using source pixels before returning.
A callback on a platform with `MGX_CAP_ASYNC_PRESENT` may retain the submitted
surface until completion. Only one submission per `mulib_glx_present_t` may be
outstanding. `serial` counts accepted submissions, while `presented` counts
acknowledged completion. Failed submissions retain serial and damage for retry.
Damage arguments are borrowed only during the callback.

After a real backend completion event, call
`mulib_glx_window_complete(&window, serial)`, or
`mulib_glx_present_complete(&present, serial)` when managing buffers separately.
Queueing a frame is not completion. Incorrect or duplicate serials fail.
Backends remain responsible for their own protocol buffers and event decoding.

The optional `mulib_glx_backend_wayland_client_t` dispatches an actual
`wl_surface.frame` callback and exposes its host token through
`mulib_glx_backend_wayland_client_take_frame`. A host adapter can pass the next
presentation serial to `..._client_begin` and complete that serial when taking
the callback. This is pacing/CPU-source completion, **not physical presentation
feedback**. SHM buffers remain independently pinned until `wl_buffer.release`.
See [the wire client lifecycle](mulib-gfx-wire.md#optional-wayland-toplevel-client).

Swapchains acquired through `mulib_glx_swapchain_acquire` never return a busy
buffer. `mulib_glx_swapchain_submit_mode(&swap, 1)` pins an asynchronous buffer;
release it with `mulib_glx_swapchain_complete(&swap, index)` after completion.
`mulib_glx_swapchain_cancel` abandons an acquired, unsubmitted frame.
The legacy `mulib_glx_swapchain_submit` remains synchronous.

## Teardown and validation

Window resize and the `*_try_free` APIs fail while submitted buffers are busy.
Retry after draining completion events. Legacy void `*_free` calls leave busy
objects intact and set `errno` to `EBUSY`; use `*_try_free` to inspect success.
Neither teardown nor resize waits for events or accesses a device.

After a backend has been **disconnected or otherwise proven quiescent**,
`mulib_glx_present_abort(p, serial)` and `mulib_glx_window_abort(w, serial)` may
abandon its accepted asynchronous submission. They validate the exact pending
serial and release source ownership without advancing `presented`. The serial
remains consumed; damage accumulated after submission is retained. Repaint the
full target when switching to a replacement backend. These APIs do not themselves
disconnect, stop a device or prove quiescence, and must never be used as a timeout
shortcut while a backend can still read the pixels.

Regression: `tests/test-gfx-present.c` exercises rejection/retry, explicit
headless operation, serial overflow, asynchronous pinning, duplicate completion,
and busy resize/teardown using a mock backend. This does not validate a live
DRM, X11, or Wayland presentation loop.
`tests/test-gfx-backend-followup.c` covers the private mock-compositor lifecycle,
frame/release separation, and backend-loss abort without claiming a live-server
or physical-presentation result.
