/**
 * @file mulib-virtio-os.h
 * @brief Synchronous Virtio device backends over the MuLIB VMM device protocol.
 * Define MULIB_IMPL_VIRTIO_OS and the VIRTIO, VMMDEV, ENDIAN switches once.
 * All objects, scratch buffers and services are caller-owned. No allocation.
 */
#ifndef MULIB_VIRTIO_OS_H
#define MULIB_VIRTIO_OS_H
#include "mulib-virtio.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Backend construction parameters. Scratch arrays each have transfer_limit bytes. */
typedef struct mulib_virtio_os_options {
  uint32_t instance, device_id;
  uint64_t features;
  const unsigned char *config;
  size_t config_size, transfer_limit;
  unsigned char *input_scratch, *output_scratch;
  mulib_virtio_service_fn service;
  void *context;
} mulib_virtio_os_options_t;
/** @brief Bounded cipher session storage; key material is erased on destroy/reset. */
typedef struct mulib_virtio_os_cipher_session {
  uint64_t id;
  unsigned key_size, direction;
  unsigned char key[32];
} mulib_virtio_os_cipher_session_t;
/** @brief Device-owned metadata for one 2D renderer resource. */
typedef struct mulib_virtio_os_gpu_resource {
  uint32_t id, width, height;
  uint64_t backing_size;
} mulib_virtio_os_gpu_resource_t;
/** @brief Initialized device; do not copy or modify fields after publication. */
typedef struct mulib_virtio_os_device {
  uint32_t instance, epoch, generation;
  unsigned status, i2c_failed;
  uint64_t offered, negotiated;
  const mulib_virtio_profile_t *profile;
  unsigned char config[MULIB_VIRTIO_CONFIG_MAX];
  size_t config_size, limit;
  unsigned char *input, *output;
  mulib_virtio_service_fn service;
  void *context;
  unsigned char plugged[256];
  unsigned pcm_state[2];
  uint32_t pcm_period[2];
  uint64_t next_session;
  mulib_virtio_os_cipher_session_t sessions[16];
  mulib_virtio_os_gpu_resource_t resources[16];
} mulib_virtio_os_device_t;
/** @brief Endpoint borrows a fixed device pointer inventory and guest-memory inventory. */
typedef struct mulib_virtio_os_bus {
  mulib_virtio_os_device_t *const *devices;
  size_t count;
  mulib_vmmdev_memory_t memory;
  unsigned busy;
} mulib_virtio_os_bus_t;
/** @brief Initialize a supported device transactionally, validating config and features. */
int mulib_virtio_os_init(mulib_virtio_os_device_t *device, const mulib_virtio_os_options_t *options);
/** @brief Validate and publish a fixed bus. Instances must be nonzero and unique. */
int mulib_virtio_os_bus_init(mulib_virtio_os_bus_t *bus,
                            mulib_virtio_os_device_t *const *devices, size_t count,
                            mulib_vmmdev_memory_t memory);
/** @brief Exchange implementation for guest clients. Caller serializes all bus/guest access. */
int mulib_virtio_os_exchange(void *bus, const void *request, size_t size,
                            void *response, size_t *capacity);
/** @brief Change balloon target or virtio-mem requested bytes, incrementing config generation. */
int mulib_virtio_os_request_memory(mulib_virtio_os_device_t *device, uint64_t amount);
/** @brief Host retirement: reset provider, erase transient state, set epoch zero. Retryable/idempotent. */
int mulib_virtio_os_shutdown(mulib_virtio_os_device_t *device);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VIRTIO_OS) && !defined(MULIB_VIRTIO_OS_IMPLEMENTED)
#define MULIB_VIRTIO_OS_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_virtio_os_zero_(const unsigned char *p, size_t n)
{ for (size_t i = 0; i < n; ++i) if (p[i]) return 0; return 1; }
static void mulib_virtio_os_erase_(void *p, size_t n)
{ volatile unsigned char *v = (volatile unsigned char *)p; while (n--) *v++ = 0; }
static int mulib_virtio_os_call_(mulib_virtio_os_device_t *d, unsigned op, unsigned queue,
                                uint64_t addr, uint32_t flags, const unsigned char *in,
                                size_t ni, unsigned char *out, size_t no, size_t *used)
{
  mulib_virtio_service_request_t r;
  int rc;
  memset(&r, 0, sizeof r);
  r.device_id = d->profile->id; r.operation = op; r.queue = queue;
  r.address = addr; r.flags = flags; r.input = in; r.input_size = ni;
  r.output = out; r.output_capacity = no; *used = 0;
  rc = d->service(d->context, &r, used);
  if (rc < 0 || rc > MULIB_VMMDEV_ENOSPC || *used > no) return MULIB_VMMDEV_EIO;
  return rc;
}
int mulib_virtio_os_shutdown(mulib_virtio_os_device_t *d)
{
  size_t used;
  int rc;
  if (!d || !d->profile || !d->service) return MULIB_VMMDEV_EINVAL;
  if (!d->epoch) return 0;
  rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_RESET, 0, 0, 0, NULL, 0, NULL, 0, &used);
  if (rc) { d->status |= MULIB_VIRTIO_NEEDS_RESET; return rc; }
  mulib_virtio_os_erase_(d->sessions, sizeof d->sessions);
  mulib_virtio_os_erase_(d->input, d->limit); mulib_virtio_os_erase_(d->output, d->limit);
  memset(d->resources, 0, sizeof d->resources); memset(d->pcm_state, 0, sizeof d->pcm_state);
  memset(d->pcm_period, 0, sizeof d->pcm_period);
  d->negotiated = 0; d->i2c_failed = 0; d->status = MULIB_VIRTIO_FAILED; d->epoch = 0;
  return 0;
}
int mulib_virtio_os_init(mulib_virtio_os_device_t *out, const mulib_virtio_os_options_t *o)
{
  mulib_virtio_os_device_t d;
  const mulib_virtio_profile_t *p;
  uintptr_t a, b;
  if (!out || !o || !o->instance || !o->service || !o->input_scratch || !o->output_scratch
      || o->transfer_limit < 512 || o->transfer_limit > MULIB_VIRTIO_TRANSFER_MAX
      || (!o->config && o->config_size) || o->config_size > MULIB_VIRTIO_CONFIG_MAX)
    return MULIB_VMMDEV_EINVAL;
  p = mulib_virtio_profile(o->device_id);
  if (!p) return MULIB_VMMDEV_ENODEV;
  if (p->support != MULIB_VIRTIO_PROFILE) return MULIB_VMMDEV_ENOTSUP;
  if ((o->features & ~(p->features | MULIB_VIRTIO_VERSION_1))
      || (o->features & p->required_features) != p->required_features)
    return MULIB_VMMDEV_ENOTSUP;
  a = (uintptr_t)o->input_scratch; b = (uintptr_t)o->output_scratch;
  if (o->transfer_limit > UINTPTR_MAX - a || o->transfer_limit > UINTPTR_MAX - b
      || (a < b + o->transfer_limit && b < a + o->transfer_limit)) return MULIB_VMMDEV_EINVAL;
  if (p->id != MULIB_VIRTIO_9P && o->config_size != p->config_size) return MULIB_VMMDEV_EINVAL;
  memset(&d, 0, sizeof d);
  d.instance = o->instance; d.epoch = 1; d.profile = p;
  d.offered = o->features | MULIB_VIRTIO_VERSION_1;
  d.config_size = o->config_size; d.limit = o->transfer_limit;
  if (o->config_size) memcpy(d.config, o->config, o->config_size);
  d.input = o->input_scratch; d.output = o->output_scratch;
  d.service = o->service; d.context = o->context;
  switch (p->id) {
  case MULIB_VIRTIO_GPU:
    if (!mulib_virtio_os_zero_(d.config, 8) || !mulib_endian_load_le32(d.config + 8)
        || mulib_endian_load_le32(d.config + 8) > 16 || mulib_endian_load_le32(d.config + 12))
      return MULIB_VMMDEV_EINVAL;
    break;
  case MULIB_VIRTIO_VSOCK:
    if (mulib_vmmdev_load64(d.config) < 3 || mulib_vmmdev_load64(d.config) >= UINT32_MAX)
      return MULIB_VMMDEV_EINVAL;
    break;
  case MULIB_VIRTIO_SCSI:
    if (mulib_endian_load_le32(d.config) != 1 || !mulib_endian_load_le32(d.config + 4)
        || mulib_endian_load_le32(d.config + 4) > MULIB_VMMDEV_MAX_SPANS - 2
        || !mulib_endian_load_le32(d.config + 8) || !mulib_endian_load_le32(d.config + 12)
        || mulib_endian_load_le32(d.config + 16) != 16
        || mulib_endian_load_le32(d.config + 20) != 96
        || mulib_endian_load_le32(d.config + 24) != 32 || mulib_vmmdev_load16(d.config + 28)
        || mulib_vmmdev_load16(d.config + 30) > 255 || mulib_endian_load_le32(d.config + 32) > 16383)
      return MULIB_VMMDEV_EINVAL;
    break;
  case MULIB_VIRTIO_CRYPTO:
    if (mulib_endian_load_le32(d.config) != 1 || mulib_endian_load_le32(d.config + 4) != 1
        || mulib_endian_load_le32(d.config + 8) != 1 || mulib_endian_load_le32(d.config + 12) != 8
        || !mulib_virtio_os_zero_(d.config + 16, 20) || mulib_endian_load_le32(d.config + 36) != 32
        || !mulib_virtio_os_zero_(d.config + 40, 8) || !mulib_vmmdev_load64(d.config + 48)
        || mulib_vmmdev_load64(d.config + 48) > d.limit - 88) return MULIB_VMMDEV_EINVAL;
    break;
  case MULIB_VIRTIO_IOMMU:
    if (mulib_vmmdev_load64(d.config) != 4096) return MULIB_VMMDEV_EINVAL;
    break;
  case MULIB_VIRTIO_SND:
    if (mulib_endian_load_le32(d.config) || mulib_endian_load_le32(d.config + 4) != 2
        || mulib_endian_load_le32(d.config + 8)) return MULIB_VMMDEV_EINVAL;
    break;
  case MULIB_VIRTIO_RPMB:
    if (d.config[0] > 128) return MULIB_VMMDEV_EINVAL;
    break;
  case MULIB_VIRTIO_BLK:
    if (mulib_vmmdev_load64(d.config) > UINT64_MAX / 512) return MULIB_VMMDEV_EINVAL;
    break;
  case MULIB_VIRTIO_BALLOON:
    if (mulib_endian_load_le32(d.config + 4)) return MULIB_VMMDEV_EINVAL;
    break;
  case MULIB_VIRTIO_9P:
    if (d.config_size < 3 || mulib_vmmdev_load16(d.config) != d.config_size - 2)
      return MULIB_VMMDEV_EINVAL;
    break;
  case MULIB_VIRTIO_INPUT:
    if (!mulib_virtio_os_zero_(d.config, d.config_size)) return MULIB_VMMDEV_EINVAL;
    break;
  case MULIB_VIRTIO_FS:
    if (!d.config[0] || !memchr(d.config, 0, 36) || mulib_endian_load_le32(d.config + 36) != 1)
      return MULIB_VMMDEV_EINVAL;
    break;
  case MULIB_VIRTIO_PMEM:
    if (!mulib_vmmdev_load64(d.config + 8)
        || mulib_vmmdev_load64(d.config + 8) > UINT64_MAX - mulib_vmmdev_load64(d.config))
      return MULIB_VMMDEV_EINVAL;
    break;
  case MULIB_VIRTIO_GPIO:
    if (!mulib_vmmdev_load16(d.config) || !mulib_virtio_os_zero_(d.config + 2, 6))
      return MULIB_VMMDEV_EINVAL;
    break;
  case MULIB_VIRTIO_MEM: {
    uint64_t block = mulib_vmmdev_load64(d.config), base = mulib_vmmdev_load64(d.config + 16);
    uint64_t region = mulib_vmmdev_load64(d.config + 24), usable = mulib_vmmdev_load64(d.config + 32);
    uint64_t target = mulib_vmmdev_load64(d.config + 48);
    if (block < 4096 || (block & (block - 1)) || base % block || !region || region % block
        || region > UINT64_MAX - base || region / block > 2048 || usable != region
        || target > usable || target % block || mulib_vmmdev_load64(d.config + 40)
        || !mulib_virtio_os_zero_(d.config + 8, 8)) return MULIB_VMMDEV_EINVAL;
    break;
  }
  default: break;
  }
  *out = d; return MULIB_VMMDEV_OK;
}
int mulib_virtio_os_bus_init(mulib_virtio_os_bus_t *out,
                            mulib_virtio_os_device_t *const *devices, size_t n,
                            mulib_vmmdev_memory_t memory)
{
  mulib_virtio_os_bus_t b;
  int rc;
  if (!out || (!devices && n)) return MULIB_VMMDEV_EINVAL;
  rc = mulib_vmmdev_memory_validate(&memory); if (rc) return rc;
  for (size_t i = 0; i < n; ++i) {
    mulib_virtio_os_device_t *d = devices[i];
    if (!d || !d->profile || !d->instance || !d->epoch) return MULIB_VMMDEV_EINVAL;
    for (size_t j = 0; j < i; ++j)
      if (devices[j]->instance == d->instance) return MULIB_VMMDEV_EINVAL;
    /* Scratch is private host storage, never guest DMA memory. */
    for (size_t j = 0; j < memory.count; ++j) {
      uintptr_t a = (uintptr_t)memory.regions[j].data, end = a + memory.regions[j].size;
      uintptr_t x = (uintptr_t)d->input, y = (uintptr_t)d->output;
      if ((a < x + d->limit && x < end) || (a < y + d->limit && y < end))
        return MULIB_VMMDEV_EINVAL;
    }
  }
  b.devices = devices; b.count = n; b.memory = memory; b.busy = 0;
  *out = b; return MULIB_VMMDEV_OK;
}
int mulib_virtio_os_request_memory(mulib_virtio_os_device_t *d, uint64_t amount)
{
  if (!d || !d->profile) return MULIB_VMMDEV_EINVAL;
  if (!d->epoch) return MULIB_VMMDEV_ESTATE;
  if (d->profile->id == MULIB_VIRTIO_BALLOON) {
    if (amount > UINT32_MAX) return MULIB_VMMDEV_EBOUNDS;
    mulib_endian_store_le32(d->config, (uint32_t)amount);
  } else if (d->profile->id == MULIB_VIRTIO_MEM) {
    if (amount > mulib_vmmdev_load64(d->config + 32)
        || amount % mulib_vmmdev_load64(d->config)) return MULIB_VMMDEV_EBOUNDS;
    mulib_vmmdev_store64(d->config + 48, amount);
  } else return MULIB_VMMDEV_ENOTSUP;
  ++d->generation; return MULIB_VMMDEV_OK;
}
static int mulib_virtio_os_block_(mulib_virtio_os_device_t *d, size_t ni, size_t no, size_t *used)
{
  uint32_t type;
  uint64_t sector, capacity;
  size_t n = 0, done = 0;
  unsigned op = 0;
  int rc;
  unsigned char status = 0;
  if (ni < 16 || no < 1 || mulib_endian_load_le32(d->input + 4)) return MULIB_VMMDEV_EINVAL;
  type = mulib_endian_load_le32(d->input); sector = mulib_vmmdev_load64(d->input + 8);
  capacity = mulib_vmmdev_load64(d->config);
  switch (type) {
  case 0:
    if (ni != 16 || (no - 1) % 512) return MULIB_VMMDEV_EINVAL;
    n = no - 1; op = MULIB_VIRTIO_SERVICE_READ; break;
  case 1:
    if (no != 1 || (ni - 16) % 512) return MULIB_VMMDEV_EINVAL;
    n = ni - 16; op = MULIB_VIRTIO_SERVICE_WRITE;
    if (d->offered & (UINT64_C(1) << 5)) status = 1;
    break;
  case 4:
    if (ni != 16 || no != 1 || sector) return MULIB_VMMDEV_EINVAL;
    if (!(d->negotiated & (UINT64_C(1) << 9))) status = 2;
    op = MULIB_VIRTIO_SERVICE_FLUSH; break;
  case 8:
    if (ni != 16 || no != 21 || sector) return MULIB_VMMDEV_EINVAL;
    op = MULIB_VIRTIO_SERVICE_ID; break;
  default: status = 2; break;
  }
  if ((type == 0 || type == 1) && (sector > capacity || n / 512 > capacity - sector)) status = 1;
  if (!status) {
    rc = mulib_virtio_os_call_(d, op, 0, sector * 512, 0,
                              d->input + 16, type == 1 ? n : 0,
                              d->output, type == 0 ? n : (type == 8 ? 20 : 0), &done);
    if (rc == MULIB_VMMDEV_AGAIN) return rc;
    if (rc || (type == 0 && done != n) || (type == 8 && done != 20)) status = 1;
  }
  if (status) memset(d->output, 0, no);
  d->output[no - 1] = status;
  *used = no; return MULIB_VMMDEV_OK;
}
static int mulib_virtio_os_mem_(mulib_virtio_os_device_t *d, size_t ni, size_t no, size_t *used)
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
static int mulib_virtio_os_scsi_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used)
{
  unsigned char *in = d->input, *out = d->output;
  size_t done = 0;
  int rc;
  if (q == 1) return ni || no < 16 ? MULIB_VMMDEV_EINVAL : MULIB_VMMDEV_AGAIN;
  if (q == 0) {
    uint32_t type;
    if (ni < 4 || !no) return MULIB_VMMDEV_EINVAL;
    type = mulib_endian_load_le32(in);
    if (type == 0) {
      if (ni != 24 || no < 1) return MULIB_VMMDEV_EINVAL;
      if (mulib_endian_load_le32(in + 4) > 7) { out[0] = 11; *used = 1; return 0; }
      rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_SCSI_TMF, q,
                                mulib_vmmdev_load64(in + 16), mulib_endian_load_le32(in + 4),
                                in + 8, 8, out, 1, &done);
      if (rc == MULIB_VMMDEV_AGAIN) return rc;
      if (rc || done != 1 || out[0] > 12) out[0] = 9;
      *used = 1; return 0;
    }
    if ((type == 1 || type == 2) && ni == 16 && no >= 5) {
      memset(out, 0, 5); *used = 5; return 0; /* no asynchronous events */
    }
    return MULIB_VMMDEV_ENOTSUP;
  }
  {
    size_t cdb = mulib_endian_load_le32(d->config + 24), sense = mulib_endian_load_le32(d->config + 20);
    size_t header = 19 + cdb, result = 12 + sense;
    if (ni < header || no < result || in[16] > 3) return MULIB_VMMDEV_EINVAL;
    if (ni > header && no > result) { memset(out, 0, result); out[11] = 9; *used = result; return 0; }
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_SCSI_COMMAND, q,
                              mulib_vmmdev_load64(in + 8), (uint32_t)sense,
                              in, ni, out, no, &done);
    if (rc == MULIB_VMMDEV_AGAIN) return rc;
    if (rc || done < result || out[11] > 9 || mulib_endian_load_le32(out) > sense
        || mulib_endian_load_le32(out + 4) > (ni - header) + (no - result)) {
      memset(out, 0, result); out[11] = 9; done = result;
    }
  }
  *used = done; return 0;
}
static int mulib_virtio_os_iommu_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used)
{
  unsigned char *in = d->input, *out = d->output;
  unsigned type, status = 0;
  size_t done;
  int rc;
  if (q == 1) {
    if (ni || no < 24) return MULIB_VMMDEV_EINVAL;
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_RECEIVE, q, 0, 0, NULL, 0, out, 24, &done);
    if (!rc && (done != 24 || out[0] > 2 || (mulib_endian_load_le32(out + 4) & ~UINT32_C(0x103))
                || !mulib_virtio_os_zero_(out + 1, 3) || mulib_endian_load_le32(out + 12)))
      return MULIB_VMMDEV_EIO;
    if (!rc) *used = done;
    return rc;
  }
  if (ni < 4 || no < 4) return MULIB_VMMDEV_EINVAL;
  type = in[0];
  if (type < 1 || type > 4) status = 2;
  else if ((type <= 2 && ni != 20) || (type == 3 && ni != 36) || (type == 4 && ni != 28)) status = 4;
  else if (type == 1 && mulib_endian_load_le32(in + 12)) status = 2;
  else if (type >= 3) {
    uint64_t start = mulib_vmmdev_load64(in + 8), end = mulib_vmmdev_load64(in + 16);
    if (start > end || (start & 4095) || (end & 4095) != 4095) status = 5;
    if (!status && type == 3) {
      uint64_t phys = mulib_vmmdev_load64(in + 24);
      uint32_t flags = mulib_endian_load_le32(in + 32);
      if ((phys & 4095) || end - start > UINT64_MAX - phys) status = 5;
      if (!flags || (flags & ~3u)) status = 2;
    }
  }
  if (!status) {
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_IOMMU, q, mulib_endian_load_le32(in + 4),
                              type, in + 4, ni - 4, out, 1, &done);
    if (rc == MULIB_VMMDEV_AGAIN) return rc;
    status = rc || done != 1 || out[0] > 8 ? 3 : out[0];
  }
  memset(out, 0, 4); out[0] = (unsigned char)status; *used = 4; return 0;
}
static int mulib_virtio_os_sound_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used)
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
static int mulib_virtio_os_crypto_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used)
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
static unsigned mulib_virtio_os_be16_(const unsigned char *p)
{ return (unsigned)p[0] << 8 | p[1]; }
static int mulib_virtio_os_rpmb_(mulib_virtio_os_device_t *d, size_t ni, size_t no, size_t *used)
{
  unsigned char *in = d->input, *out = d->output;
  unsigned type, blocks, result_read = 0;
  size_t frames, expected = 0, done;
  int rc;
  if (!ni || ni % 512 || no % 512) return MULIB_VMMDEV_EINVAL;
  frames = ni / 512; type = mulib_virtio_os_be16_(in + 510); blocks = mulib_virtio_os_be16_(in + 506);
  if (type == 1 || type == 3) {
    if (frames > 1 && mulib_virtio_os_be16_(in + ni - 2) == 5) { result_read = 1; --frames; expected = 512; }
    if (!blocks || (type == 1 && (blocks != 1 || frames != 1))
        || (type == 3 && (frames != blocks || (d->config[1] && blocks > d->config[1]))))
      return MULIB_VMMDEV_EINVAL;
    for (size_t i = 0; i < frames; ++i)
      if (mulib_virtio_os_be16_(in + i * 512 + 510) != type) return MULIB_VMMDEV_EINVAL;
    if (result_read && mulib_virtio_os_be16_(in + ni - 6) != 1) return MULIB_VMMDEV_EINVAL;
  } else if (type == 2 || type == 4 || type == 5) {
    if (frames != 1 || !blocks || ((type == 2 || type == 5) && blocks != 1)
        || (type == 4 && d->config[2] && blocks > d->config[2])) return MULIB_VMMDEV_EINVAL;
    expected = type == 4 ? (size_t)blocks * 512 : 512;
  } else return MULIB_VMMDEV_ENOTSUP;
  if (no != expected) return MULIB_VMMDEV_EINVAL;
  rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_RPMB, 0, 0, type, in, ni, out, no, &done);
  if (rc) return rc;
  if (done != expected) return MULIB_VMMDEV_EIO;
  for (size_t i = 0; i < done; i += 512) {
    unsigned response = mulib_virtio_os_be16_(out + i + 510);
    if ((type != 5 && response != type * 256) || (type == 5 && response != 256 && response != 768))
      return MULIB_VMMDEV_EIO;
  }
  *used = done; return 0;
}
static int mulib_virtio_os_vsock_valid_(const unsigned char *p, size_t n, uint64_t cid, int receive)
{
  uint32_t len, flags;
  unsigned op;
  if (n < 44) return 0;
  len = mulib_endian_load_le32(p + 24); op = mulib_vmmdev_load16(p + 30);
  flags = mulib_endian_load_le32(p + 32);
  return len <= n - 44 && mulib_vmmdev_load64(p + (receive ? 8 : 0)) == cid
    && mulib_vmmdev_load64(p + (receive ? 0 : 8)) <= UINT32_MAX
    && op >= 1 && op <= 7 && (op == 5 || !len) && (op == 4 ? !(flags & ~3u) : !flags);
}
static int mulib_virtio_os_vsock_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used)
{
  size_t done = 0;
  int rc;
  uint64_t cid = mulib_vmmdev_load64(d->config);
  if (q == 1) {
    if (no || !mulib_virtio_os_vsock_valid_(d->input, ni, cid, 0)) return MULIB_VMMDEV_EINVAL;
    /* The socket service owns connections, credit accounting, and RST replies
       for unknown socket types; this adapter never silently drops such packets. */
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_SEND, q, cid, 0,
                              d->input, 44u + mulib_endian_load_le32(d->input + 24), NULL, 0, &done);
  } else {
    if (ni || no < (q == 2 ? 4u : 44u)) return MULIB_VMMDEV_EINVAL;
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_RECEIVE, q, cid, 0, NULL, 0,
                              d->output, q == 2 ? 4 : no, &done);
    if (!rc && ((q == 2 && (done != 4 || mulib_endian_load_le32(d->output)))
                || (q == 0 && (!mulib_virtio_os_vsock_valid_(d->output, done, cid, 1)
                              || done != 44u + mulib_endian_load_le32(d->output + 24)))))
      return MULIB_VMMDEV_EIO;
  }
  if (!rc) *used = done;
  return rc;
}
static int mulib_virtio_os_gpu_(mulib_virtio_os_device_t *d, const mulib_vmmdev_memory_t *memory,
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
static int mulib_virtio_os_dispatch_(mulib_virtio_os_device_t *d, const mulib_vmmdev_memory_t *memory, unsigned q,
                                    size_t ni, size_t no, size_t *used)
{
  unsigned char *in = d->input, *out = d->output;
  unsigned op;
  size_t done = 0;
  int rc;
  uint32_t id = d->profile->id;
  *used = 0;
  if (id == MULIB_VIRTIO_GPU) return mulib_virtio_os_gpu_(d, memory, q, ni, no, used);
  if (id == MULIB_VIRTIO_VSOCK) return mulib_virtio_os_vsock_(d, q, ni, no, used);
  if (id == MULIB_VIRTIO_SCSI) return mulib_virtio_os_scsi_(d, q, ni, no, used);
  if (id == MULIB_VIRTIO_IOMMU) return mulib_virtio_os_iommu_(d, q, ni, no, used);
  if (id == MULIB_VIRTIO_SND) return mulib_virtio_os_sound_(d, q, ni, no, used);
  if (id == MULIB_VIRTIO_CRYPTO) return mulib_virtio_os_crypto_(d, q, ni, no, used);
  if (id == MULIB_VIRTIO_RPMB) return mulib_virtio_os_rpmb_(d, ni, no, used);
  if (id == MULIB_VIRTIO_BLK) return mulib_virtio_os_block_(d, ni, no, used);
  if (id == MULIB_VIRTIO_MEM) return mulib_virtio_os_mem_(d, ni, no, used);
  if (id == MULIB_VIRTIO_RNG) {
    if (ni || !no) return MULIB_VMMDEV_EINVAL;
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_ENTROPY, q, 0, 0, NULL, 0, out, no, &done);
    if (!rc && !done) return MULIB_VMMDEV_EIO;
  } else if (id == MULIB_VIRTIO_NET || id == MULIB_VIRTIO_CONSOLE || id == MULIB_VIRTIO_RPMSG
             || id == MULIB_VIRTIO_INPUT) {
    size_t header = id == MULIB_VIRTIO_NET ? 12 : 0;
    if (q == 0) {
      if (ni || no <= header || (id == MULIB_VIRTIO_RPMSG && no < 16)
          || (id == MULIB_VIRTIO_INPUT && (no < 8 || no % 8))) return MULIB_VMMDEV_EINVAL;
      memset(out, 0, header);
      rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_RECEIVE, q, 0, 0,
                                NULL, 0, out + header, no - header, &done);
      if (!rc && !done) return MULIB_VMMDEV_AGAIN;
      if (!rc && id == MULIB_VIRTIO_RPMSG
          && (done < 16 || mulib_vmmdev_load16(out + 12) != done - 16
              || !mulib_virtio_os_zero_(out + 8, 4) || mulib_vmmdev_load16(out + 14)))
        return MULIB_VMMDEV_EIO;
      if (!rc && id == MULIB_VIRTIO_INPUT && done % 8) return MULIB_VMMDEV_EIO;
      done += header;
    } else {
      if (no || ni <= header || (header && !mulib_virtio_os_zero_(in, header)))
        return MULIB_VMMDEV_EINVAL;
      if (id == MULIB_VIRTIO_RPMSG
          && (ni < 16 || mulib_vmmdev_load16(in + 12) != ni - 16
              || !mulib_virtio_os_zero_(in + 8, 4) || mulib_vmmdev_load16(in + 14)))
        return MULIB_VMMDEV_EINVAL;
      if (id == MULIB_VIRTIO_INPUT && ni % 8) return MULIB_VMMDEV_EINVAL;
      rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_SEND, q, 0, 0,
                                in + header, ni - header, NULL, 0, &done);
    }
  } else if (id == MULIB_VIRTIO_BALLOON) {
    if (!ni || ni % 4 || no) return MULIB_VMMDEV_EINVAL;
    op = q == 0 ? MULIB_VIRTIO_SERVICE_INFLATE : MULIB_VIRTIO_SERVICE_DEFLATE;
    rc = mulib_virtio_os_call_(d, op, q, 0, 0, in, ni, NULL, 0, &done);
  } else if (id == MULIB_VIRTIO_PMEM) {
    if (ni != 4 || no < 4 || mulib_endian_load_le32(in)) return MULIB_VMMDEV_EINVAL;
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_FLUSH, q, 0, 0, NULL, 0, NULL, 0, &done);
    if (rc == MULIB_VMMDEV_AGAIN) return rc;
    mulib_endian_store_le32(out, rc ? 1 : 0); done = 4; rc = 0;
  } else if (id == MULIB_VIRTIO_I2C) {
    uint16_t address;
    uint32_t flags;
    unsigned char h[8];
    size_t n;
    if (ni < 8 || no < 1) return MULIB_VMMDEV_EINVAL;
    address = mulib_vmmdev_load16(in); flags = mulib_endian_load_le32(in + 4);
    if (mulib_virtio_i2c_header(h, address, flags) || mulib_vmmdev_load16(in + 2))
      return MULIB_VMMDEV_EINVAL;
    if ((flags & 2) ? ni != 8 : no != 1) return MULIB_VMMDEV_EINVAL;
    n = (flags & 2) ? no - 1 : ni - 8;
    rc = d->i2c_failed ? MULIB_VMMDEV_EIO :
      mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_I2C, q, address, flags,
                            in + 8, (flags & 2) ? 0 : n, out, (flags & 2) ? n : 0, &done);
    if (rc == MULIB_VMMDEV_AGAIN) return rc;
    if (!rc && (flags & 2) && done != n) rc = MULIB_VMMDEV_EIO;
    d->i2c_failed = rc && (flags & 1);
    if (rc) memset(out, 0, no);
    out[no - 1] = rc ? 1 : 0; done = no; rc = 0;
  } else if (id == MULIB_VIRTIO_GPIO) {
    uint16_t type, line;
    uint32_t value;
    unsigned char h[8];
    if (ni != 8 || no < 1) return MULIB_VMMDEV_EINVAL;
    type = mulib_vmmdev_load16(in); line = mulib_vmmdev_load16(in + 2);
    value = mulib_endian_load_le32(in + 4);
    if (type == 1) { out[0] = 1; *used = 1; return 0; } /* names_size is zero */
    if (no < 2) return MULIB_VMMDEV_EINVAL;
    rc = mulib_virtio_gpio_request(h, type, line, value);
    if (rc || type == 1 || line >= mulib_vmmdev_load16(d->config)) rc = MULIB_VMMDEV_EINVAL;
    else rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_GPIO, q, line, type,
                                   in + 4, 4, out + 1, 1, &done);
    if (rc == MULIB_VMMDEV_AGAIN) return rc;
    if (!rc && ((type == 2 && (done != 1 || out[1] > 2))
                || (type == 4 && (done != 1 || out[1] > 1)))) rc = MULIB_VMMDEV_EIO;
    if (rc || type == 3 || type == 5) out[1] = 0;
    out[0] = rc ? 1 : 0; done = 2; rc = 0;
  } else if (id == MULIB_VIRTIO_9P || id == MULIB_VIRTIO_FS || id == MULIB_VIRTIO_SCMI) {
    if (id == MULIB_VIRTIO_9P && (ni < 7 || no < 7 || mulib_endian_load_le32(in) != ni))
      return MULIB_VMMDEV_EINVAL;
    if (id == MULIB_VIRTIO_FS) {
      uint32_t opcode;
      int forget;
      if (ni < 40 || mulib_endian_load_le32(in) != ni) return MULIB_VMMDEV_EINVAL;
      opcode = mulib_endian_load_le32(in + 4); forget = opcode == 2 || opcode == 42 || opcode == 41;
      if ((!q && opcode != 2 && opcode != 42) || (forget ? no != 0 : no < 16))
        return MULIB_VMMDEV_EINVAL;
    }
    if (id == MULIB_VIRTIO_SCMI
        && (ni < 4 || ni % 4 || no < 8 || (mulib_endian_load_le32(in) & UINT32_C(0xf0000300))))
      return MULIB_VMMDEV_EINVAL;
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_REQUEST, q, 0, 0, in, ni, out, no, &done);
    if (!rc && id == MULIB_VIRTIO_9P
        && (done < 7 || mulib_endian_load_le32(out) != done
            || mulib_vmmdev_load16(out + 5) != mulib_vmmdev_load16(in + 5)
            || (out[4] != (unsigned)in[4] + 1 && out[4] != 107 && out[4] != 7)))
      return MULIB_VMMDEV_EIO;
    if (!rc && id == MULIB_VIRTIO_FS && no
        && (done < 16 || mulib_endian_load_le32(out) != done
            || mulib_vmmdev_load64(out + 8) != mulib_vmmdev_load64(in + 8))) return MULIB_VMMDEV_EIO;
    if (!rc && id == MULIB_VIRTIO_SCMI
        && (done < 8 || done % 4 || mulib_endian_load_le32(out) != mulib_endian_load_le32(in)
            || (mulib_endian_load_le32(out + 4) && done != 8))) return MULIB_VMMDEV_EIO;
  } else return MULIB_VMMDEV_ENOTSUP;
  if (rc) return rc;
  *used = done; return MULIB_VMMDEV_OK;
}
static int mulib_virtio_os_config_write_(mulib_virtio_os_device_t *d, size_t off,
                                        const unsigned char *p, size_t n)
{
  if (!(d->status & MULIB_VIRTIO_FEATURES_OK) || (d->status & (MULIB_VIRTIO_FAILED | MULIB_VIRTIO_NEEDS_RESET)))
    return MULIB_VMMDEV_ESTATE;
  if (d->profile->id == MULIB_VIRTIO_BALLOON && off == 4 && n == 4) memcpy(d->config + 4, p, 4);
  else if (d->profile->id == MULIB_VIRTIO_GPU && off == 4 && n == 4) {
    uint32_t events = mulib_endian_load_le32(d->config);
    mulib_endian_store_le32(d->config, events & ~mulib_endian_load_le32(p));
  }
  else if (d->profile->id == MULIB_VIRTIO_SCSI && (off == 20 || off == 24) && n == 4) {
    uint32_t value = mulib_endian_load_le32(p);
    if (!value || value > 256) return MULIB_VMMDEV_EBOUNDS;
    memcpy(d->config + off, p, 4);
  }
  else if (d->profile->id == MULIB_VIRTIO_INPUT && off < 2 && n && n <= 2 - off) {
    unsigned char select[2] = {d->config[0], d->config[1]};
    size_t done;
    int rc;
    memcpy(select + off, p, n);
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_INPUT_CONFIG, 0, select[0], select[1],
                              NULL, 0, d->output, 128, &done);
    if (rc) return rc;
    memset(d->config, 0, 136); d->config[0] = select[0]; d->config[1] = select[1];
    d->config[2] = (unsigned char)done; memcpy(d->config + 8, d->output, done);
  } else return MULIB_VMMDEV_EACCESS;
  ++d->generation; return MULIB_VMMDEV_OK;
}
static int mulib_virtio_os_control_(mulib_virtio_os_bus_t *b, mulib_virtio_os_device_t *d,
                                   const mulib_vmmdev_message_t *m,
                                   mulib_vmmdev_message_t *r, unsigned char *payload)
{
  size_t used = 0;
  int rc;
  if (!d->epoch) return MULIB_VMMDEV_ENODEV;
  if (m->opcode != MULIB_VMMDEV_PROBE && m->epoch != d->epoch) return MULIB_VMMDEV_ESTALE;
  if (m->opcode != MULIB_VMMDEV_CONFIG_WRITE && m->opcode != MULIB_VMMDEV_SUBMIT && m->length)
    return MULIB_VMMDEV_EINVAL;
  if (m->opcode != MULIB_VMMDEV_CONFIG_READ && m->opcode != MULIB_VMMDEV_SUBMIT && m->count)
    return MULIB_VMMDEV_EINVAL;
  switch (m->opcode) {
  case MULIB_VMMDEV_PROBE:
    if (m->argument || m->count || m->length || m->epoch) return MULIB_VMMDEV_EINVAL;
    mulib_endian_store_le32(payload, d->profile->id); mulib_endian_store_le32(payload + 4, d->status);
    mulib_vmmdev_store64(payload + 8, d->offered);
    mulib_endian_store_le32(payload + 16, d->profile->queues);
    mulib_endian_store_le32(payload + 20, (uint32_t)d->config_size);
    mulib_endian_store_le32(payload + 24, d->generation);
    mulib_endian_store_le32(payload + 28, (uint32_t)d->limit);
    r->length = 32; return 0;
  case MULIB_VMMDEV_RESET:
    if (m->argument) return MULIB_VMMDEV_EINVAL;
    if (d->epoch == UINT32_MAX) return MULIB_VMMDEV_ESTATE;
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_RESET, 0, 0, 0, NULL, 0, NULL, 0, &used);
    if (rc) { d->status |= MULIB_VIRTIO_NEEDS_RESET; return rc; }
    d->status = 0; d->negotiated = 0; d->i2c_failed = 0; ++d->epoch;
    memset(d->pcm_state, 0, sizeof d->pcm_state); memset(d->pcm_period, 0, sizeof d->pcm_period);
    mulib_virtio_os_erase_(d->sessions, sizeof d->sessions);
    memset(d->resources, 0, sizeof d->resources);
    if (d->profile->id == MULIB_VIRTIO_SCSI) {
      mulib_endian_store_le32(d->config + 20, 96); mulib_endian_store_le32(d->config + 24, 32);
    }
    if (d->profile->id == MULIB_VIRTIO_INPUT) memset(d->config, 0, 136);
    if (d->profile->id == MULIB_VIRTIO_BALLOON) memset(d->config + 4, 0, 4);
    ++d->generation; return 0;
  case MULIB_VMMDEV_FEATURES:
    if (d->status != (MULIB_VIRTIO_ACKNOWLEDGE | MULIB_VIRTIO_DRIVER)) return MULIB_VMMDEV_ESTATE;
    if ((m->argument & ~d->offered) || !(m->argument & MULIB_VIRTIO_VERSION_1)
        || (m->argument & d->profile->required_features) != d->profile->required_features)
      return MULIB_VMMDEV_ENOTSUP;
    d->negotiated = m->argument; return 0;
  case MULIB_VMMDEV_STATUS: {
    unsigned s;
    if (m->argument > 255 || (m->argument & ~UINT64_C(143))) return MULIB_VMMDEV_EINVAL;
    s = (unsigned)m->argument;
    if ((d->status & (MULIB_VIRTIO_FAILED | MULIB_VIRTIO_NEEDS_RESET)) || (s & d->status) != d->status)
      return MULIB_VMMDEV_ESTATE;
    if (s & MULIB_VIRTIO_FAILED) { d->status = s; return 0; }
    if (s == d->status) return 0;
    if (!((d->status == 0 && s == 1) || (d->status == 1 && s == 3)
          || (d->status == 3 && s == 11 && (d->negotiated & MULIB_VIRTIO_VERSION_1))
          || (d->status == 11 && s == 15))) return MULIB_VMMDEV_ESTATE;
    d->status = s; return 0;
  }
  case MULIB_VMMDEV_CONFIG_READ:
    if (m->argument > d->config_size || m->count > d->config_size - (size_t)m->argument)
      return MULIB_VMMDEV_EBOUNDS;
    memcpy(payload, d->config + (size_t)m->argument, m->count);
    r->length = m->count; r->count = d->generation; return 0;
  case MULIB_VMMDEV_CONFIG_WRITE:
    if (m->argument > d->config_size || m->length > d->config_size - (size_t)m->argument)
      return MULIB_VMMDEV_EBOUNDS;
    return mulib_virtio_os_config_write_(d, (size_t)m->argument, m->payload, m->length);
  case MULIB_VMMDEV_SUBMIT: {
    mulib_vmmdev_chain_t c;
    if (d->status != 15) return MULIB_VMMDEV_ESTATE;
    if (m->argument >= d->profile->queues) return MULIB_VMMDEV_EBOUNDS;
    rc = mulib_vmmdev_chain_decode(&b->memory, m->payload, m->length, m->count, d->limit, &c);
    if (rc) return rc;
    rc = mulib_vmmdev_chain_read(&c, 0, d->input, c.input_size); if (rc) return rc;
    memset(d->output, 0, c.output_size);
    rc = mulib_virtio_os_dispatch_(d, &b->memory, (unsigned)m->argument, c.input_size, c.output_size, &used);
    if (d->profile->id == MULIB_VIRTIO_CRYPTO || d->profile->id == MULIB_VIRTIO_RPMB)
      mulib_virtio_os_erase_(d->input, c.input_size);
    if (rc) return rc;
    if (used > c.output_size) return MULIB_VMMDEV_EIO;
    rc = mulib_vmmdev_chain_write(&c, 0, d->output, used); if (rc) return rc;
    r->count = (uint32_t)used; return 0;
  }
  default: return MULIB_VMMDEV_ENOTSUP;
  }
}
int mulib_virtio_os_exchange(void *context, const void *request, size_t n, void *response, size_t *capacity)
{
  mulib_virtio_os_bus_t *b = (mulib_virtio_os_bus_t *)context;
  mulib_vmmdev_message_t m, r;
  mulib_virtio_os_device_t *d = NULL;
  unsigned char payload[MULIB_VIRTIO_CONFIG_MAX];
  size_t needed = 40;
  int rc;
  if (!b || !capacity || !response || b->busy) return MULIB_VMMDEV_EINVAL;
  rc = mulib_vmmdev_decode(request, n, &m); if (rc) return rc;
  if (m.opcode & MULIB_VMMDEV_RESPONSE) return MULIB_VMMDEV_EINVAL;
  for (size_t i = 0; i < b->count; ++i)
    if (b->devices[i]->instance == m.device) { d = b->devices[i]; break; }
  /* Capacity is checked before side effects, including RESET and host I/O. */
  if (d && m.opcode == MULIB_VMMDEV_PROBE) needed += 32;
  if (d && m.opcode == MULIB_VMMDEV_CONFIG_READ && m.count <= MULIB_VIRTIO_CONFIG_MAX) needed += m.count;
  if (*capacity < needed) { *capacity = needed; return MULIB_VMMDEV_ENOSPC; }
  memset(&r, 0, sizeof r); r.opcode = (uint16_t)(m.opcode | MULIB_VMMDEV_RESPONSE);
  r.device = m.device; r.token = m.token; r.payload = payload;
  b->busy = 1;
  rc = d ? mulib_virtio_os_control_(b, d, &m, &r, payload) : MULIB_VMMDEV_ENODEV;
  r.argument = (uint64_t)rc; r.epoch = d ? d->epoch : 0;
  if (rc) { r.length = 0; r.count = 0; }
  rc = mulib_vmmdev_encode(&r, response, capacity); b->busy = 0;
  return rc;
}
#ifdef __cplusplus
}
#endif
#endif
