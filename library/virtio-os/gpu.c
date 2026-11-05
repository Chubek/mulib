/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_gpu_(mulib_virtio_os_device_t *d, const mulib_vmmdev_memory_t *memory,
                               unsigned q, size_t ni, size_t no, size_t *used)
{
  unsigned char *in = d->input, *out = d->output;
  uint32_t type, flags, id = 0, response = 0x1100, width = 0, height = 0;
  uint64_t backing = 0;
  mulib_virtio_os_gpu_resource_t *resource = NULL, *free_slot = NULL;
  size_t expect = 24, done = 0, body = 0;
  int rc;
  if (ni < 24 || (no < 24 && !(q == 1 && !no))) return MULIB_VMMDEV_EINVAL;
  type = mulib_endian_load_le32(in); flags = mulib_endian_load_le32(in + 4);
  if ((flags & ~1u) || mulib_endian_load_le32(in + 16)) response = 0x1204;
  if (q == 1) {
    if (ni != 56 || (type != 0x300 && type != 0x301)) return MULIB_VMMDEV_EINVAL;
    if (mulib_endian_load_le32(in + 24) >= mulib_endian_load_le32(d->config + 8)) response = 0x1202;
    if (type == 0x300) id = mulib_endian_load_le32(in + 40);
  } else {
    switch (type) {
    case 0x100: expect = 24; body = 384; break;
    case 0x101: expect = 40; break;
    case 0x102: case 0x107: expect = 32; break;
    case 0x103: case 0x104: expect = 48; break;
    case 0x105: expect = 56; break;
    case 0x106:
      if (ni < 32 || mulib_endian_load_le32(in + 28) > 64) return MULIB_VMMDEV_EINVAL;
      expect = 32u + 16u * mulib_endian_load_le32(in + 28); break;
    default: response = 0x1205; expect = ni; break;
    }
    if (ni != expect || (body && no < 24 + body)) return MULIB_VMMDEV_EINVAL;
    if (type >= 0x101 && type <= 0x107)
      id = mulib_endian_load_le32(in + (type == 0x103 ? 44 : (type == 0x104 ? 40 : (type == 0x105 ? 48 : 24))));
  }
  for (unsigned i = 0; i < 16; ++i) {
    if (id && d->resources[i].id == id) resource = d->resources + i;
    if (!d->resources[i].id) free_slot = d->resources + i;
  }
  if (response == 0x1100 && type == 0x101) {
    uint32_t format = mulib_endian_load_le32(in + 28);
    width = mulib_endian_load_le32(in + 32); height = mulib_endian_load_le32(in + 36);
    if (!id || resource) response = 0x1203;
    else if (!free_slot) response = 0x1201;
    else if (!width || !height || width > 16384 || height > 16384
             || !((format >= 1 && format <= 4) || format == 67 || format == 68 || format == 121 || format == 134))
      response = 0x1205;
  } else if (response == 0x1100 && ((type >= 0x102 && type <= 0x107) || type == 0x300)) {
    if (!resource && !((type == 0x103 || type == 0x300) && !id)) response = 0x1203;
    if (type == 0x103 && mulib_endian_load_le32(in + 40) >= mulib_endian_load_le32(d->config + 8)) response = 0x1202;
    if (resource && type >= 0x103 && type <= 0x105) {
      uint32_t x = mulib_endian_load_le32(in + 24), y = mulib_endian_load_le32(in + 28);
      uint32_t w = mulib_endian_load_le32(in + 32), h = mulib_endian_load_le32(in + 36);
      if (!w || !h || x > resource->width || w > resource->width - x
          || y > resource->height || h > resource->height - y) response = 0x1205;
      else if (type == 0x105) {
        uint64_t off = mulib_vmmdev_load64(in + 40), bytes = (uint64_t)(h - 1) * resource->width * 4 + (uint64_t)w * 4;
        if (off > resource->backing_size || bytes > resource->backing_size - off) response = 0x1205;
      }
    }
    if (resource && type == 0x300 && (resource->width != 64 || resource->height != 64
        || mulib_endian_load_le32(in + 44) >= 64 || mulib_endian_load_le32(in + 48) >= 64)) response = 0x1205;
    if (resource && type == 0x106) {
      uint32_t count = mulib_endian_load_le32(in + 28);
      if (!count || resource->backing_size) response = 0x1205;
      for (uint32_t i = 0; i < count && response == 0x1100; ++i) {
        const unsigned char *entry = in + 32 + 16 * i;
        unsigned char *mapped;
        uint32_t bytes = mulib_endian_load_le32(entry + 8);
        if (mulib_vmmdev_map(memory, mulib_vmmdev_load64(entry), bytes, MULIB_VMMDEV_READ, &mapped)) response = 0x1205;
        backing += bytes;
      }
    }
  }
  if (response == 0x1100) {
    mulib_virtio_service_request_t r;
    memset(&r, 0, sizeof r); r.device_id = MULIB_VIRTIO_GPU; r.operation = MULIB_VIRTIO_SERVICE_GPU;
    r.queue = q; r.flags = type; r.address = id; r.input = in + 24; r.input_size = ni - 24;
    r.output = out + 24; r.output_capacity = body; r.memory = memory;
    rc = d->service(d->context, &r, &done);
    if (rc == MULIB_VMMDEV_AGAIN) return rc;
    if (rc || done != body) response = rc == MULIB_VMMDEV_ENOSPC ? 0x1201 : 0x1200;
    else if (type == 0x100) {
      for (unsigned i = 0; i < 16; ++i) {
        unsigned char *mode = out + 24 + 24 * i;
        uint32_t enabled = mulib_endian_load_le32(mode + 16);
        if (enabled > 1 || mulib_endian_load_le32(mode + 20)
            || (i >= mulib_endian_load_le32(d->config + 8) && enabled)) response = 0x1200;
      }
      if (response == 0x1100) response = 0x1101;
    } else if (type == 0x101) {
      free_slot->id = id; free_slot->width = width; free_slot->height = height;
    } else if (resource && type == 0x102) memset(resource, 0, sizeof *resource);
    else if (resource && type == 0x106) resource->backing_size = backing;
    else if (resource && type == 0x107) resource->backing_size = 0;
  }
  if (!no) { *used = 0; return response < 0x1200 ? 0 : MULIB_VMMDEV_EIO; }
  memset(out, 0, 24); mulib_endian_store_le32(out, response);
  mulib_endian_store_le32(out + 4, flags & 1);
  if (flags & 1) memcpy(out + 8, in + 8, 8);
  *used = response == 0x1101 ? 408 : 24;
  return 0;
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_gpu_(const mulib_virtio_os_device_t *d)
{
  if (!mulib_virtio_os_zero_(d->config, 8) || !mulib_endian_load_le32(d->config + 8)
        || mulib_endian_load_le32(d->config + 8) > 16 || mulib_endian_load_le32(d->config + 12))
      return MULIB_VMMDEV_EINVAL;
    return MULIB_VMMDEV_OK;
}
