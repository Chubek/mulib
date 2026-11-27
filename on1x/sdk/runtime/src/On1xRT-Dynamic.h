#ifndef ON1X_RT_DYNAMIC_INTERNAL_H
#define ON1X_RT_DYNAMIC_INTERNAL_H

#include "On1xRT.h"

#if defined(ON1X_RT_ENABLE_DYNAMIC) && defined(__linux__) && defined(__GLIBC__)
#define ON1X_RT_HAS_DYNAMIC 1
#define ON1X_RT_INTERNAL __attribute__((visibility("hidden")))

typedef struct On1xRT_DynamicImage {
  int fd;
  size_t size;
  char sha256[65];
} On1xRT_DynamicImage;

ON1X_RT_INTERNAL On1xRT_Status
on1x_rt_dynamic_snapshot_(const char *path, size_t budget,
                          On1xRT_DynamicImage *image, const char **message);
ON1X_RT_INTERNAL void on1x_rt_dynamic_image_release_(int fd);
ON1X_RT_INTERNAL On1xRT_Status
on1x_rt_dynamic_open_(const On1xRT_DynamicImage *image, void **library,
                      On1xRT_ModuleEntryFn *entry, const char **message);
ON1X_RT_INTERNAL void on1x_rt_dynamic_close_(void *library);
#undef ON1X_RT_INTERNAL
#endif

#endif
