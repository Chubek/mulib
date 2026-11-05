/* Nitro Secure Module CBOR transport. PCRs, entropy and signatures belong to the provider. */
#include "internal.h"

static int mulib_virtio_os_nsm_utf8_(const unsigned char *p, size_t n)
{
  size_t i = 0;
  while (i < n) {
    uint32_t value, minimum;
    unsigned more, first = p[i++];
    if (first < 128) continue;
    if (first >= 0xc2 && first <= 0xdf) { more = 1; value = first & 31; minimum = 128; }
    else if (first >= 0xe0 && first <= 0xef) { more = 2; value = first & 15; minimum = 2048; }
    else if (first >= 0xf0 && first <= 0xf4) { more = 3; value = first & 7; minimum = 65536; }
    else return 0;
    if (more > n - i) return 0;
    while (more--) {
      unsigned next = p[i++];
      if ((next & 0xc0) != 0x80) return 0;
      value = (value << 6) | (next & 63);
    }
    if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return 0;
  }
  return 1;
}

/* Definite-length CBOR subset used by the NSM serde API; no tags or floating point. */
static int mulib_virtio_os_nsm_head_(const unsigned char *p, size_t n, size_t *offset,
                                    unsigned *major, uint64_t *argument)
{
  unsigned first, extra;
  uint64_t value;
  if (*offset >= n) return 0;
  first = p[(*offset)++]; *major = first >> 5; extra = first & 31;
  if (extra < 24) { *argument = extra; return 1; }
  if (extra > 27) return 0;
  extra = 1u << (extra - 24);
  if (extra > n - *offset) return 0;
  value = 0;
  while (extra--) value = (value << 8) | p[(*offset)++];
  *argument = value; return 1;
}

static int mulib_virtio_os_nsm_item_(const unsigned char *p, size_t n, size_t *offset, unsigned depth)
{
  unsigned major;
  uint64_t argument, count;
  size_t start = *offset;
  if (depth > 16 || !mulib_virtio_os_nsm_head_(p, n, offset, &major, &argument)) return 0;
  switch (major) {
  case 0: case 1: return 1;
  case 2: case 3:
    if (argument > n - *offset) return 0;
    if (major == 3 && !mulib_virtio_os_nsm_utf8_(p + *offset, (size_t)argument)) return 0;
    *offset += (size_t)argument; return 1;
  case 4: case 5:
    if (argument > (n - *offset) / (major == 5 ? 2u : 1u)) return 0;
    count = argument * (major == 5 ? 2u : 1u);
    while (count--) if (!mulib_virtio_os_nsm_item_(p, n, offset, depth + 1)) return 0;
    return 1;
  case 7: return p[start] >= 0xf4 && p[start] <= 0xf6;
  default: return 0;
  }
}

/* Externally tagged enum: either a unit-variant string, or one map entry. */
static int mulib_virtio_os_nsm_envelope_(const unsigned char *p, size_t n,
                                        const unsigned char **name, size_t *length)
{
  size_t offset = 0, end = 0;
  unsigned major, map = 0;
  uint64_t argument;
  if (!mulib_virtio_os_nsm_item_(p, n, &end, 0) || end != n
      || !mulib_virtio_os_nsm_head_(p, n, &offset, &major, &argument)) return 0;
  if (major == 5) {
    if (argument != 1) return 0;
    map = 1;
    if (!mulib_virtio_os_nsm_head_(p, n, &offset, &major, &argument)) return 0;
  }
  if (major != 3 || !argument || argument > 64 || argument > n - offset) return 0;
  *name = p + offset; *length = (size_t)argument;
  return map || offset + argument == n;
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_nsm_(mulib_virtio_os_device_t *d, unsigned q,
                                             size_t ni, size_t no, size_t *used)
{
  const unsigned char *request_name, *response_name;
  size_t request_length, response_length, done;
  int rc;
  *used = 0;
  if (q || !ni || ni > MULIB_VIRTIO_NSM_REQUEST_MAX || !no
      || !mulib_virtio_os_nsm_envelope_(d->input, ni, &request_name, &request_length))
    return MULIB_VMMDEV_EINVAL;
  rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_REQUEST, q, 0, 0,
                            d->input, ni, d->output,
                            no > MULIB_VIRTIO_NSM_RESPONSE_MAX ? MULIB_VIRTIO_NSM_RESPONSE_MAX : no, &done);
  if (rc) return rc;
  if (!mulib_virtio_os_nsm_envelope_(d->output, done, &response_name, &response_length))
    return MULIB_VMMDEV_EIO;
  if (!(response_length == 5 && !memcmp(response_name, "Error", 5))
      && (response_length != request_length || memcmp(response_name, request_name, request_length)))
    return MULIB_VMMDEV_EIO;
  *used = done; return 0;
}
