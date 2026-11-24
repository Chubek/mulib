#ifndef ON1X_ARCHVIRT_HOST_STAGE_H
#define ON1X_ARCHVIRT_HOST_STAGE_H
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
int on1x_archvirt_run_image(const void *bytes, size_t size);
#ifdef __cplusplus
}
#endif

#endif
