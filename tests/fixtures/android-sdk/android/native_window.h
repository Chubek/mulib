/* MuLIB test double, not an Android SDK header. */
#ifndef MULIB_TEST_WINDOW_H
#define MULIB_TEST_WINDOW_H
#include <stdint.h>
typedef struct ANativeWindow ANativeWindow;
typedef struct ARect { int32_t left, top, right, bottom; } ARect;
typedef struct ANativeWindow_Buffer {
  int32_t width, height, stride, format;
  void *bits;
  uint32_t reserved[6];
} ANativeWindow_Buffer;
void ANativeWindow_acquire (ANativeWindow *);
void ANativeWindow_release (ANativeWindow *);
int32_t ANativeWindow_setBuffersGeometry (ANativeWindow *, int32_t, int32_t, int32_t);
int32_t ANativeWindow_lock (ANativeWindow *, ANativeWindow_Buffer *, ARect *);
int32_t ANativeWindow_unlockAndPost (ANativeWindow *);
#endif
