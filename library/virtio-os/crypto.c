/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_crypto_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used)
{
  unsigned char *in = d->input, *out = d->output;
  uint32_t op, algo, status = 0;
  mulib_virtio_os_cipher_session_t *session = NULL;
  if (ni < 72 || !no) return MULIB_VMMDEV_EINVAL;
  op = mulib_endian_load_le32(in); algo = mulib_endian_load_le32(in + 4);
  if (q == 1 && op == 2) {
    uint32_t key = mulib_endian_load_le32(in + 20), direction = mulib_endian_load_le32(in + 24);
    if (no < 16) return MULIB_VMMDEV_EINVAL;
    if (algo != 3 || mulib_endian_load_le32(in + 16) != 3 || mulib_endian_load_le32(in + 64) != 1) status = 3;
    else if ((key != 16 && key != 24 && key != 32) || ni - 72 != key || direction < 1 || direction > 2) status = 1;
    if (!status) {
      for (unsigned i = 0; i < 16; ++i) if (!d->sessions[i].id) { session = d->sessions + i; break; }
      if (!session || d->next_session == UINT64_MAX) status = 1;
      else {
        session->id = ++d->next_session; session->key_size = key; session->direction = direction;
        memcpy(session->key, in + 72, key);
      }
    }
    memset(out, 0, 16); mulib_vmmdev_store64(out, status ? 0 : session->id);
    mulib_endian_store_le32(out + 8, status); *used = 16;
  } else if (q == 1 && op == 3) {
    uint64_t id = mulib_vmmdev_load64(in + 16);
    if (ni != 72) return MULIB_VMMDEV_EINVAL;
    for (unsigned i = 0; i < 16; ++i) if (id && d->sessions[i].id == id) session = d->sessions + i;
    if (!session) status = 4; else mulib_virtio_os_erase_(session, sizeof *session);
    out[0] = (unsigned char)status; *used = 1;
  } else if (q == 0) {
    uint64_t id = mulib_vmmdev_load64(in + 8);
    uint32_t iv = mulib_endian_load_le32(in + 24), src = mulib_endian_load_le32(in + 28);
    uint32_t dst = mulib_endian_load_le32(in + 32);
    size_t done = 0;
    int rc;
    if (ni < 88 || no - 1 != dst || src != ni - 88 || iv != 16 || src != dst || src % 16
        || src > mulib_vmmdev_load64(d->config + 48)) return MULIB_VMMDEV_EINVAL;
    if (op > 1 || algo != 3 || mulib_endian_load_le32(in + 64) != 1) status = 3;
    for (unsigned i = 0; i < 16; ++i) if (id && d->sessions[i].id == id) session = d->sessions + i;
    if (!status && (!session || session->direction != op + 1)) status = 4;
    if (!status) {
      mulib_virtio_service_request_t r;
      memset(&r, 0, sizeof r); r.device_id = MULIB_VIRTIO_CRYPTO; r.operation = MULIB_VIRTIO_SERVICE_CIPHER;
      r.address = id; r.flags = op; r.input = in + 72; r.input_size = ni - 72;
      r.output = out; r.output_capacity = dst; r.key = session->key; r.key_size = session->key_size;
      rc = d->service(d->context, &r, &done);
      if (rc == MULIB_VMMDEV_AGAIN) return rc;
      if (rc || done != dst) status = 1;
    }
    if (status) memset(out, 0, no);
    out[no - 1] = (unsigned char)status; *used = no;
  } else return MULIB_VMMDEV_ENOTSUP;
  return 0;
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_crypto_(const mulib_virtio_os_device_t *d)
{
  if (mulib_endian_load_le32(d->config) != 1 || mulib_endian_load_le32(d->config + 4) != 1
        || mulib_endian_load_le32(d->config + 8) != 1 || mulib_endian_load_le32(d->config + 12) != 8
        || !mulib_virtio_os_zero_(d->config + 16, 20) || mulib_endian_load_le32(d->config + 36) != 32
        || !mulib_virtio_os_zero_(d->config + 40, 8) || !mulib_vmmdev_load64(d->config + 48)
        || mulib_vmmdev_load64(d->config + 48) > d->limit - 88) return MULIB_VMMDEV_EINVAL;
    return MULIB_VMMDEV_OK;
}
