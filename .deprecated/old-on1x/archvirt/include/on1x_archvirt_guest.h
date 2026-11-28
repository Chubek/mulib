#ifndef ON1X_ARCHVIRT_GUEST_H
#define ON1X_ARCHVIRT_GUEST_H

#include <stdint.h>

/* Internal guest runtime ABI. On1x must explicitly declare this exact extern. */
int64_t on1x_archvirt_read_i64(void);

#endif
