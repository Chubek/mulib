/**
 * @file mulib-vmm-host.h
 * @brief Concrete RAM block storage and Linux system-entropy services for MuLIB VMM.
 * Define MULIB_IMPL_VMM_HOST plus VIRTIO, VMMDEV and ENDIAN once.
 */
#ifndef MULIB_VMM_HOST_H
#define MULIB_VMM_HOST_H
#include "mulib-virtio.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Borrowed RAM disk; size is a multiple of 512. Media survives device reset. */
typedef struct mulib_vmm_host_block {
  unsigned char *data;
  size_t size;
  unsigned readonly;
  unsigned char identifier[20];
} mulib_vmm_host_block_t;
/** @brief Initialize an explicitly volatile block device; no ownership transfer. */
int mulib_vmm_host_block_init(mulib_vmm_host_block_t *block, void *data, size_t size,
                             int readonly, const unsigned char identifier[20]);
/** @brief Encode capacity and select RO/FLUSH features for the RAM disk. */
int mulib_vmm_host_block_config(const mulib_vmm_host_block_t *block, unsigned char config[8], uint64_t *features);
/** @brief Virtio service callback for RAM read/write/get-ID/flush/reset. */
int mulib_vmm_host_block_service(void *context, const mulib_virtio_service_request_t *request, size_t *used);
/** @brief Virtio entropy service using nonblocking Linux getrandom; no fallback PRNG. */
int mulib_vmm_host_entropy_service(void *context, const mulib_virtio_service_request_t *request, size_t *used);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VMM_HOST) && !defined(MULIB_VMM_HOST_IMPLEMENTED)
#define MULIB_VMM_HOST_IMPLEMENTED
#include <string.h>
#include <errno.h>
#ifdef __linux__
#include <sys/random.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
int mulib_vmm_host_block_init(mulib_vmm_host_block_t *out, void *data, size_t n,
                             int readonly, const unsigned char identifier[20])
{
  mulib_vmm_host_block_t b;
  if (!out || !data || !n || n % 512 || n > UINTPTR_MAX - (uintptr_t)data
      || (readonly != 0 && readonly != 1)) return MULIB_VMMDEV_EINVAL;
  memset(&b, 0, sizeof b); b.data = (unsigned char *)data; b.size = n; b.readonly = (unsigned)readonly;
  if (identifier) memcpy(b.identifier, identifier, 20);
  else memcpy(b.identifier, "mulib-ram-disk", 14);
  *out = b; return 0;
}
int mulib_vmm_host_block_config(const mulib_vmm_host_block_t *b, unsigned char config[8], uint64_t *features)
{
  if (!b || !b->data || !b->size || b->size % 512 || !config || !features) return MULIB_VMMDEV_EINVAL;
  mulib_vmmdev_store64(config, b->size / 512);
  *features = (UINT64_C(1) << 9) | (b->readonly ? UINT64_C(1) << 5 : 0); return 0;
}
int mulib_vmm_host_block_service(void *context, const mulib_virtio_service_request_t *r, size_t *used)
{
  mulib_vmm_host_block_t *b = (mulib_vmm_host_block_t *)context;
  size_t n;
  if (!b || !b->data || !r || !used || r->device_id != MULIB_VIRTIO_BLK) return MULIB_VMMDEV_EINVAL;
  *used = 0;
  if (r->operation == MULIB_VIRTIO_SERVICE_RESET || r->operation == MULIB_VIRTIO_SERVICE_FLUSH) return 0;
  if (r->operation == MULIB_VIRTIO_SERVICE_ID) {
    if (!r->output || r->output_capacity < 20) return MULIB_VMMDEV_ENOSPC;
    memcpy(r->output, b->identifier, 20); *used = 20; return 0;
  }
  if (r->operation != MULIB_VIRTIO_SERVICE_READ && r->operation != MULIB_VIRTIO_SERVICE_WRITE)
    return MULIB_VMMDEV_ENOTSUP;
  n = r->operation == MULIB_VIRTIO_SERVICE_READ ? r->output_capacity : r->input_size;
  if (r->address % 512 || n % 512 || r->address > b->size || n > b->size - (size_t)r->address)
    return MULIB_VMMDEV_EBOUNDS;
  if (r->operation == MULIB_VIRTIO_SERVICE_WRITE) {
    if (b->readonly) return MULIB_VMMDEV_EACCESS;
    if (!r->input && n) return MULIB_VMMDEV_EINVAL;
    if (n) memcpy(b->data + (size_t)r->address, r->input, n);
  } else {
    if (!r->output && n) return MULIB_VMMDEV_EINVAL;
    if (n) memcpy(r->output, b->data + (size_t)r->address, n);
    *used = n;
  }
  return 0;
}
int mulib_vmm_host_entropy_service(void *context, const mulib_virtio_service_request_t *r, size_t *used)
{
  (void)context;
  if (!r || !used || r->device_id != MULIB_VIRTIO_RNG) return MULIB_VMMDEV_EINVAL;
  *used = 0;
  if (r->operation == MULIB_VIRTIO_SERVICE_RESET) return 0;
  if (r->operation != MULIB_VIRTIO_SERVICE_ENTROPY) return MULIB_VMMDEV_ENOTSUP;
  if (!r->output || !r->output_capacity || r->output_capacity > MULIB_VIRTIO_TRANSFER_MAX || r->input_size)
    return MULIB_VMMDEV_EINVAL;
#ifdef __linux__
  {
    ssize_t n;
    do { n = getrandom(r->output, r->output_capacity, GRND_NONBLOCK); } while (n < 0 && errno == EINTR);
    if (n < 0) return errno == EAGAIN ? MULIB_VMMDEV_AGAIN : (errno == ENOSYS ? MULIB_VMMDEV_ENOTSUP : MULIB_VMMDEV_EIO);
    if (!n) return MULIB_VMMDEV_AGAIN;
    *used = (size_t)n; return 0;
  }
#else
  return MULIB_VMMDEV_ENOTSUP;
#endif
}
#ifdef __cplusplus
}
#endif
#endif
