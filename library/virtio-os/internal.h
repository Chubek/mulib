#ifndef MULIB_VIRTIO_OS_INTERNAL_H
#define MULIB_VIRTIO_OS_INTERNAL_H
#include "mulib-virtio-os.h"
#include <string.h>
#define MULIB_VIRTIO_INTERNAL

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_nsm_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_rtc_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_can_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_bt_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_spi_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_can_(const mulib_virtio_os_device_t *d);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_bt_(const mulib_virtio_os_device_t *d);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_spi_(const mulib_virtio_os_device_t *d);

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_block_(mulib_virtio_os_device_t *d, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_mem_(mulib_virtio_os_device_t *d, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_scsi_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_iommu_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_sound_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_crypto_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_rpmb_(mulib_virtio_os_device_t *d, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_vsock_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_gpu_(mulib_virtio_os_device_t *d, const mulib_vmmdev_memory_t *memory,
                               unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_rng_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_net_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_console_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_rpmsg_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_input_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_balloon_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_pmem_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_i2c_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_gpio_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_p9_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_fs_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_scmi_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_gpu_(const mulib_virtio_os_device_t *d);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_vsock_(const mulib_virtio_os_device_t *d);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_scsi_(const mulib_virtio_os_device_t *d);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_crypto_(const mulib_virtio_os_device_t *d);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_iommu_(const mulib_virtio_os_device_t *d);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_snd_(const mulib_virtio_os_device_t *d);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_rpmb_(const mulib_virtio_os_device_t *d);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_blk_(const mulib_virtio_os_device_t *d);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_balloon_(const mulib_virtio_os_device_t *d);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_p9_(const mulib_virtio_os_device_t *d);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_input_(const mulib_virtio_os_device_t *d);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_fs_(const mulib_virtio_os_device_t *d);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_pmem_(const mulib_virtio_os_device_t *d);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_gpio_(const mulib_virtio_os_device_t *d);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_mem_(const mulib_virtio_os_device_t *d);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_zero_(const unsigned char *p, size_t n);
MULIB_VIRTIO_INTERNAL void mulib_virtio_os_erase_(void *p, size_t n);
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_call_(mulib_virtio_os_device_t *d, unsigned op, unsigned queue,
                                uint64_t addr, uint32_t flags, const unsigned char *in,
                                size_t ni, unsigned char *out, size_t no, size_t *used);
#endif
