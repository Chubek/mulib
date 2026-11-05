/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_mem_(mulib_virtio_os_device_t *d, size_t ni, size_t no, size_t *used)
{
  uint16_t type, count, result = 0, state = 0;
  uint64_t block = mulib_vmmdev_load64(d->config), base = mulib_vmmdev_load64(d->config + 16);
  uint64_t region = mulib_vmmdev_load64(d->config + 32), current = mulib_vmmdev_load64(d->config + 40);
  uint64_t target = mulib_vmmdev_load64(d->config + 48), addr, bytes;
  unsigned first = 0, active = 0;
  size_t done;
  int rc;
  if (ni != 24 || no < 10) return MULIB_VMMDEV_EINVAL;
  type = mulib_vmmdev_load16(d->input); addr = mulib_vmmdev_load64(d->input + 8);
  count = mulib_vmmdev_load16(d->input + 16); bytes = (uint64_t)count * block;
  if (type > 3 || !mulib_virtio_os_zero_(d->input + 2, 6)
      || !mulib_virtio_os_zero_(d->input + 18, 6)) result = 3;
  if (!result && type != 2) {
    if (!count || count > region / block || addr < base || (addr - base) % block || addr - base > region
        || bytes > region - (addr - base)) result = 3;
    else {
      first = (unsigned)((addr - base) / block);
      for (unsigned i = first; i < first + count; ++i)
        active += (d->plugged[i / 8] >> (i % 8)) & 1u;
    }
  }
  if (!result && type == 3) state = active == count ? 0 : (active ? 2 : 1);
  if (!result && (type == 0 || type == 1)) {
    if ((type == 0 && active) || (type == 1 && active != count)) result = 3;
    else if (type == 0 && (current > target || bytes > target - current)) result = 1;
  }
  if (!result && type != 3) {
    if (type == 2) { addr = base; count = (uint16_t)(region / block); }
    rc = mulib_virtio_os_call_(d, type == 0 ? MULIB_VIRTIO_SERVICE_PLUG : MULIB_VIRTIO_SERVICE_UNPLUG,
                              0, addr, count, NULL, 0, NULL, 0, &done);
    if (rc) result = rc == MULIB_VMMDEV_AGAIN ? 2 : (type == 0 ? 1 : 3);
    else {
      if (type == 2) { memset(d->plugged, 0, sizeof d->plugged); current = 0; }
      else {
        for (unsigned i = first; i < first + count; ++i) {
          if (type == 0) d->plugged[i / 8] |= (unsigned char)(1u << (i % 8));
          else d->plugged[i / 8] &= (unsigned char)~(1u << (i % 8));
        }
        current = type == 0 ? current + bytes : current - bytes;
      }
      mulib_vmmdev_store64(d->config + 40, current); ++d->generation;
    }
  }
  memset(d->output, 0, 10); mulib_vmmdev_store16(d->output, result);
  mulib_vmmdev_store16(d->output + 8, state); *used = 10; return MULIB_VMMDEV_OK;
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_mem_(const mulib_virtio_os_device_t *d)
{
  uint64_t block = mulib_vmmdev_load64(d->config), base = mulib_vmmdev_load64(d->config + 16);
    uint64_t region = mulib_vmmdev_load64(d->config + 24), usable = mulib_vmmdev_load64(d->config + 32);
    uint64_t target = mulib_vmmdev_load64(d->config + 48);
    if (block < 4096 || (block & (block - 1)) || base % block || !region || region % block
        || region > UINT64_MAX - base || region / block > 2048 || usable != region
        || target > usable || target % block || mulib_vmmdev_load64(d->config + 40)
        || !mulib_virtio_os_zero_(d->config + 8, 8)) return MULIB_VMMDEV_EINVAL;
    return MULIB_VMMDEV_OK;
}
