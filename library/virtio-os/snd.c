/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_sound_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used)
{
  unsigned char *in = d->input, *out = d->output;
  uint32_t code, stream, status = 0x8000;
  size_t done = 0;
  int rc = 0;
  if (q == 1) return ni || no < 8 ? MULIB_VMMDEV_EINVAL : MULIB_VMMDEV_AGAIN;
  if (ni < 4 || no < (q ? 8u : 4u)) return MULIB_VMMDEV_EINVAL;
  code = mulib_endian_load_le32(in);
  if (!q) {
    if (code == 1 || code == 0x100 || code == 0x200) {
      uint32_t start, count, size, total = code == 0x100 ? 2 : 0;
      if (ni != 16) return MULIB_VMMDEV_EINVAL;
      start = mulib_endian_load_le32(in + 4); count = mulib_endian_load_le32(in + 8);
      size = mulib_endian_load_le32(in + 12);
      if (start > total || count > total - start || (code == 0x100 && size != 32)) status = 0x8001;
      else if (count > (no - 4) / 32) status = 0x8001;
      else for (uint32_t i = 0; i < count; ++i) {
        unsigned char *info = out + 4 + 32 * i;
        memset(info, 0, 32); mulib_vmmdev_store64(info + 8, UINT64_C(1) << 5);
        mulib_vmmdev_store64(info + 16, UINT64_C(1) << 7);
        info[24] = (unsigned char)(start + i); info[25] = 2; info[26] = 2;
      }
      done = status == 0x8000 ? 4 + 32u * count : 4;
    } else if (code >= 0x101 && code <= 0x105) {
      unsigned next = 0;
      if (ni != (code == 0x101 ? 24u : 8u)) return MULIB_VMMDEV_EINVAL;
      stream = mulib_endian_load_le32(in + 4);
      if (stream > 1) status = 0x8001;
      else if (code == 0x101) {
        uint32_t buffer = mulib_endian_load_le32(in + 8), period = mulib_endian_load_le32(in + 12);
        if (d->pcm_state[stream] > 1 || !period || buffer < period || buffer % period
            || period % 4 || period > d->limit - 8 || mulib_endian_load_le32(in + 16)
            || in[20] != 2 || in[21] != 5 || in[22] != 7 || in[23]) status = 0x8002;
        next = 1;
      } else {
        unsigned state = d->pcm_state[stream];
        if (code == 0x102) { next = 2; if (state != 1 && state != 2) status = 0x8001; }
        if (code == 0x103) { next = 1; if (state != 2) status = 0x8001; }
        if (code == 0x104) { next = 3; if (state != 2) status = 0x8001; }
        if (code == 0x105) { next = 2; if (state != 3) status = 0x8001; }
      }
      if (status == 0x8000) {
        rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_PCM_CONTROL, q, stream, code,
                                  in + 8, ni - 8, NULL, 0, &done);
        if (rc == MULIB_VMMDEV_AGAIN) return rc;
        if (rc) status = 0x8003;
        else { d->pcm_state[stream] = next; if (code == 0x101) d->pcm_period[stream] = mulib_endian_load_le32(in + 12); }
      }
      done = 4;
    } else { status = 0x8002; done = 4; }
    mulib_endian_store_le32(out, status); *used = done; return 0;
  }
  stream = code;
  if (stream > 1 || (q == 2 && (stream != 0 || no != 8)) || (q == 3 && (stream != 1 || ni != 4)))
    return MULIB_VMMDEV_EINVAL;
  {
    size_t bytes = q == 2 ? ni - 4 : no - 8;
    if (!bytes || bytes % 4 || bytes > d->pcm_period[stream]) return MULIB_VMMDEV_EINVAL;
    if (d->pcm_state[stream] == 2) return MULIB_VMMDEV_AGAIN;
    if (d->pcm_state[stream] != 3) status = 0x8003;
    else {
      rc = mulib_virtio_os_call_(d, q == 2 ? MULIB_VIRTIO_SERVICE_WRITE : MULIB_VIRTIO_SERVICE_READ,
                                q, stream, 0, in + 4, q == 2 ? bytes : 0, out, q == 3 ? bytes : 0, &done);
      if (rc == MULIB_VMMDEV_AGAIN) return rc;
      if (rc || (q == 3 && done != bytes)) { status = 0x8003; memset(out, 0, no); }
    }
    mulib_endian_store_le32(out + no - 8, status); mulib_endian_store_le32(out + no - 4, 0);
    *used = no; return 0;
  }
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_snd_(const mulib_virtio_os_device_t *d)
{
  if (mulib_endian_load_le32(d->config) || mulib_endian_load_le32(d->config + 4) != 2
        || mulib_endian_load_le32(d->config + 8)) return MULIB_VMMDEV_EINVAL;
    return MULIB_VMMDEV_OK;
}
