/** @file mulib-ezndk.h
 * @brief Owned Android assets/windows, looper helpers and safe logcat messages.
 * Define MULIB_IMPL_EZNDK and BUFFER once. MULIB_EZNDK_NATIVE opts into NDK
 * headers/backend (API 9+, link -landroid -llog). SDK-free by default. */
#ifndef MULIB_EZNDK_H
#define MULIB_EZNDK_H
#include "mulib-buffer.h"
#include <stdint.h>
#ifdef MULIB_EZNDK_NATIVE
#include <android/asset_manager.h>
#include <android/native_window.h>
#include <android/looper.h>
#include <android/log.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_ezndk_status {
  MULIB_EZNDK_OK = 0, MULIB_EZNDK_INVALID = -1, MULIB_EZNDK_UNSUPPORTED = -2,
  MULIB_EZNDK_NOMEM = -3, MULIB_EZNDK_IO = -4, MULIB_EZNDK_LIMIT = -5,
  MULIB_EZNDK_STATE = -6, MULIB_EZNDK_NOT_FOUND = -7
} mulib_ezndk_status_t;
/** Whether the native NDK backend was compiled in. No runtime availability or
 * hardware support is implied for unrelated NDK APIs. */
int mulib_ezndk_available (void);
/** Read a borrowed AAssetManager's asset into an owned MuLIB buffer. Append is
 * atomic, binary/empty assets supported, max_bytes bounds bytes read. Never
 * retains the manager; no filesystem-path or compressed-asset assumptions. */
mulib_ezndk_status_t mulib_ezndk_asset_load (void *manager, const char *path,
    size_t max_bytes, mulib_buffer_t *out);
typedef struct mulib_ezndk_window { void *handle; int locked; } mulib_ezndk_window_t;
/** Pixels borrowed only until unlock/post. stride is pixels, row_bytes includes
 * padding; format uses WINDOW_FORMAT_* values (RGBA8888=1, RGBX8888=2, RGB565=4). */
typedef struct mulib_ezndk_pixels {
  void *data;
  size_t width, height, stride, row_bytes;
  int format;
} mulib_ezndk_pixels_t;
/** Acquire one reference to a borrowed ANativeWindow; output starts zero. */
mulib_ezndk_status_t mulib_ezndk_window_open (void *window, mulib_ezndk_window_t *out);
/** Set geometry while unlocked. Width/height both zero select base dimensions.
 * format=0 keeps base format. Negative/mixed-zero dimensions rejected. */
mulib_ezndk_status_t mulib_ezndk_window_geometry (mulib_ezndk_window_t *window,
    int width, int height, int format);
/** Lock a complete buffer and validate its stride/size/format. No nested locks. */
mulib_ezndk_status_t mulib_ezndk_window_lock (mulib_ezndk_window_t *window,
    mulib_ezndk_pixels_t *out);
/** Consume the lock and post. Lock is consumed even if native posting fails. */
mulib_ezndk_status_t mulib_ezndk_window_post (mulib_ezndk_window_t *window);
/** Lock/fill/post supported formats. rgba is 0xRRGGBBAA, independent of host
 * endianness. Padding pixels are not written; RGBX gets opaque X. */
mulib_ezndk_status_t mulib_ezndk_window_fill (mulib_ezndk_window_t *window, uint32_t rgba);
/** Release our reference and zero the owner; locked windows reject release. */
mulib_ezndk_status_t mulib_ezndk_window_close (mulib_ezndk_window_t *window);
/** Prepare this thread's looper, allowing non-callback FDs. Borrowed result;
 * caller must not release it without separately acquiring a reference. */
mulib_ezndk_status_t mulib_ezndk_looper_prepare (void **out);
/** Register/remove an FD on a borrowed ALooper, with application identifier>=0.
 * Neither helper owns/closes the FD or the user pointer. */
mulib_ezndk_status_t mulib_ezndk_looper_add (void *looper, int fd, int identifier,
    int events, void *user);
mulib_ezndk_status_t mulib_ezndk_looper_remove (void *looper, int fd);
typedef struct mulib_ezndk_event { int identifier, fd, events; void *user; }
  mulib_ezndk_event_t;
/** PollOnce on this thread. identifier preserves native wake/callback/timeout
 * values (-1/-2/-3); native poll error (-4) maps to IO. Timeout -1 blocks. */
mulib_ezndk_status_t mulib_ezndk_looper_poll (int timeout_ms, mulib_ezndk_event_t *out);
/** Write literal text to logcat, without interpreting '%' as a format string.
 * priority is Android log priority 2..7. No fallback to stderr is implicit. */
mulib_ezndk_status_t mulib_ezndk_log (int priority, const char *tag, const char *text);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EZNDK) && !defined(MULIB_EZNDK_IMPLEMENTED)
#define MULIB_EZNDK_IMPLEMENTED
#include <limits.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ezndk_available (void)
{
#ifdef MULIB_EZNDK_NATIVE
  return 1;
#else
  return 0;
#endif
}
mulib_ezndk_status_t mulib_ezndk_asset_load (void *manager, const char *path, size_t max, mulib_buffer_t *out)
{
  if (!manager || !path || !*path || !out) return MULIB_EZNDK_INVALID;
#ifdef MULIB_EZNDK_NATIVE
  {
    AAsset *asset = AAssetManager_open ((AAssetManager *)manager, path, AASSET_MODE_STREAMING);
    mulib_buffer_t b = {0}; unsigned char bytes[4096]; mulib_ezndk_status_t s = MULIB_EZNDK_OK;
    if (!asset) return MULIB_EZNDK_NOT_FOUND;
    for (;;) {
      size_t request = max - b.length;
      int n;
      if (request > sizeof (bytes)) request = sizeof (bytes);
      /* One-byte probe distinguishes exact-limit/empty success from overflow. */
      n = AAsset_read (asset, bytes, request ? request : 1);
      if (n < 0) { s = MULIB_EZNDK_IO; break; }
      if (!n) break;
      if ((size_t)n > request) { s = MULIB_EZNDK_LIMIT; break; }
      if (!mulib_buffer_append (&b, bytes, (size_t)n)) { s = MULIB_EZNDK_NOMEM; break; }
    }
    AAsset_close (asset);
    if (s == MULIB_EZNDK_OK && !mulib_buffer_append (out, b.data, b.length)) s = MULIB_EZNDK_NOMEM;
    mulib_buffer_free (&b); return s;
  }
#else
  (void)max; return MULIB_EZNDK_UNSUPPORTED;
#endif
}
mulib_ezndk_status_t mulib_ezndk_window_open (void *raw, mulib_ezndk_window_t *out)
{
  if (!raw || !out || out->handle || out->locked) return MULIB_EZNDK_INVALID;
#ifdef MULIB_EZNDK_NATIVE
  ANativeWindow_acquire ((ANativeWindow *)raw); out->handle = raw; return MULIB_EZNDK_OK;
#else
  return MULIB_EZNDK_UNSUPPORTED;
#endif
}
mulib_ezndk_status_t mulib_ezndk_window_geometry (mulib_ezndk_window_t *w, int width, int height, int format)
{
  if (!w || !w->handle || width < 0 || height < 0 || (!width != !height)
      || (format != 0 && format != 1 && format != 2 && format != 4)) return MULIB_EZNDK_INVALID;
  if (w->locked) return MULIB_EZNDK_STATE;
#ifdef MULIB_EZNDK_NATIVE
  return ANativeWindow_setBuffersGeometry ((ANativeWindow *)w->handle, width, height, format) < 0 ? MULIB_EZNDK_IO : MULIB_EZNDK_OK;
#else
  return MULIB_EZNDK_UNSUPPORTED;
#endif
}
mulib_ezndk_status_t mulib_ezndk_window_lock (mulib_ezndk_window_t *w, mulib_ezndk_pixels_t *out)
{
  if (!w || !w->handle || !out) return MULIB_EZNDK_INVALID;
  if (w->locked) return MULIB_EZNDK_STATE;
#ifdef MULIB_EZNDK_NATIVE
  {
    ANativeWindow_Buffer b; mulib_ezndk_pixels_t p; size_t bpp;
    memset (&b, 0, sizeof (b));
    if (ANativeWindow_lock ((ANativeWindow *)w->handle, &b, NULL) < 0) return MULIB_EZNDK_IO;
    bpp = b.format == 1 || b.format == 2 ? 4 : b.format == 4 ? 2 : 0;
    if (!bpp || !b.bits || b.width <= 0 || b.height <= 0 || b.stride < b.width
        || (size_t)b.stride > SIZE_MAX / bpp || (size_t)b.height > SIZE_MAX / ((size_t)b.stride * bpp))
      { (void)ANativeWindow_unlockAndPost ((ANativeWindow *)w->handle); return bpp ? MULIB_EZNDK_INVALID : MULIB_EZNDK_UNSUPPORTED; }
    p.data = b.bits; p.width = (size_t)b.width; p.height = (size_t)b.height;
    p.stride = (size_t)b.stride; p.row_bytes = p.stride * bpp; p.format = b.format;
    w->locked = 1; *out = p; return MULIB_EZNDK_OK;
  }
#else
  return MULIB_EZNDK_UNSUPPORTED;
#endif
}
mulib_ezndk_status_t mulib_ezndk_window_post (mulib_ezndk_window_t *w)
{
  if (!w || !w->handle) return MULIB_EZNDK_INVALID;
  if (!w->locked) return MULIB_EZNDK_STATE;
#ifdef MULIB_EZNDK_NATIVE
  { int s = ANativeWindow_unlockAndPost ((ANativeWindow *)w->handle); w->locked = 0; return s < 0 ? MULIB_EZNDK_IO : MULIB_EZNDK_OK; }
#else
  return MULIB_EZNDK_UNSUPPORTED;
#endif
}
mulib_ezndk_status_t mulib_ezndk_window_fill (mulib_ezndk_window_t *w, uint32_t rgba)
{
  mulib_ezndk_pixels_t p; size_t x, y;
  mulib_ezndk_status_t s = mulib_ezndk_window_lock (w, &p);
  if (s != MULIB_EZNDK_OK) return s;
  for (y = 0; y < p.height; ++y) {
    unsigned char *row = (unsigned char *)p.data + y * p.row_bytes;
    for (x = 0; x < p.width; ++x) {
      if (p.format == 4) {
        uint16_t v = (uint16_t)(((rgba >> 27) << 11) | (((rgba >> 18) & 63) << 5) | ((rgba >> 11) & 31));
        row[x * 2] = (unsigned char)v; row[x * 2 + 1] = (unsigned char)(v >> 8);
      } else {
        row[x * 4] = (unsigned char)(rgba >> 24); row[x * 4 + 1] = (unsigned char)(rgba >> 16);
        row[x * 4 + 2] = (unsigned char)(rgba >> 8); row[x * 4 + 3] = p.format == 2 ? 255 : (unsigned char)rgba;
      }
    }
  }
  return mulib_ezndk_window_post (w);
}
mulib_ezndk_status_t mulib_ezndk_window_close (mulib_ezndk_window_t *w)
{
  if (!w) return MULIB_EZNDK_INVALID;
  if (w->locked) return MULIB_EZNDK_STATE;
  if (!w->handle) return MULIB_EZNDK_OK;
#ifdef MULIB_EZNDK_NATIVE
  ANativeWindow_release ((ANativeWindow *)w->handle); memset (w, 0, sizeof (*w)); return MULIB_EZNDK_OK;
#else
  return MULIB_EZNDK_UNSUPPORTED;
#endif
}
mulib_ezndk_status_t mulib_ezndk_looper_prepare (void **out)
{
  if (!out) return MULIB_EZNDK_INVALID;
#ifdef MULIB_EZNDK_NATIVE
  { ALooper *p = ALooper_prepare (ALOOPER_PREPARE_ALLOW_NON_CALLBACKS); if (!p) return MULIB_EZNDK_NOMEM; *out = p; return MULIB_EZNDK_OK; }
#else
  return MULIB_EZNDK_UNSUPPORTED;
#endif
}
mulib_ezndk_status_t mulib_ezndk_looper_add (void *looper, int fd, int id, int events, void *user)
{
  if (!looper || fd < 0 || id < 0 || events <= 0 || (events & ~3)) return MULIB_EZNDK_INVALID;
#ifdef MULIB_EZNDK_NATIVE
  return ALooper_addFd ((ALooper *)looper, fd, id, events, NULL, user) == 1 ? MULIB_EZNDK_OK : MULIB_EZNDK_IO;
#else
  (void)user; return MULIB_EZNDK_UNSUPPORTED;
#endif
}
mulib_ezndk_status_t mulib_ezndk_looper_remove (void *looper, int fd)
{
  if (!looper || fd < 0) return MULIB_EZNDK_INVALID;
#ifdef MULIB_EZNDK_NATIVE
  { int s = ALooper_removeFd ((ALooper *)looper, fd); return s < 0 ? MULIB_EZNDK_IO : s ? MULIB_EZNDK_OK : MULIB_EZNDK_NOT_FOUND; }
#else
  return MULIB_EZNDK_UNSUPPORTED;
#endif
}
mulib_ezndk_status_t mulib_ezndk_looper_poll (int timeout, mulib_ezndk_event_t *out)
{
  if (!out || timeout < -1) return MULIB_EZNDK_INVALID;
#ifdef MULIB_EZNDK_NATIVE
  { mulib_ezndk_event_t e; memset (&e, 0, sizeof (e)); e.fd = -1;
    e.identifier = ALooper_pollOnce (timeout, &e.fd, &e.events, &e.user);
    if (e.identifier == ALOOPER_POLL_ERROR) return MULIB_EZNDK_IO;
    *out = e; return MULIB_EZNDK_OK; }
#else
  return MULIB_EZNDK_UNSUPPORTED;
#endif
}
mulib_ezndk_status_t mulib_ezndk_log (int priority, const char *tag, const char *text)
{
  if (!tag || !*tag || !text || priority < 2 || priority > 7) return MULIB_EZNDK_INVALID;
#ifdef MULIB_EZNDK_NATIVE
  return __android_log_write (priority, tag, text) < 0 ? MULIB_EZNDK_IO : MULIB_EZNDK_OK;
#else
  return MULIB_EZNDK_UNSUPPORTED;
#endif
}
#ifdef __cplusplus
}
#endif
#endif
