/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_zero_(const unsigned char *p, size_t n)
{ for (size_t i = 0; i < n; ++i) if (p[i]) return 0; return 1; }
MULIB_VIRTIO_INTERNAL void mulib_virtio_os_erase_(void *p, size_t n)
{ volatile unsigned char *v = (volatile unsigned char *)p; while (n--) *v++ = 0; }
/* Invalid host ranges are never considered disjoint. */
static int mulib_virtio_os_overlap_(const void *p, size_t n, const void *q, size_t m)
{
  uintptr_t a = (uintptr_t)p, b = (uintptr_t)q;
  if (n > UINTPTR_MAX - a || m > UINTPTR_MAX - b) return 1;
  return n && m && a < b + m && b < a + n;
}
static int mulib_virtio_os_private_(const mulib_vmmdev_memory_t *memory, const void *p, size_t n)
{
  if (n > UINTPTR_MAX - (uintptr_t)p) return 0;
  for (size_t i = 0; i < memory->count; ++i)
    if (mulib_virtio_os_overlap_(p, n, memory->regions[i].data, memory->regions[i].size)) return 0;
  return 1;
}
/* Packets and their capacity word must not clobber the endpoint or its DMA. */
static int mulib_virtio_os_packet_(const mulib_virtio_os_bus_t *b, const void *p, size_t n)
{
  if (!mulib_virtio_os_private_(&b->memory, p, n)
      || mulib_virtio_os_overlap_(p, n, b, sizeof *b)
      || mulib_virtio_os_overlap_(p, n, b->devices, b->count * sizeof *b->devices)
      || mulib_virtio_os_overlap_(p, n, b->memory.regions, b->memory.count * sizeof *b->memory.regions)) return 0;
  for (size_t i = 0; i < b->count; ++i) {
    const mulib_virtio_os_device_t *d = b->devices[i];
    if (mulib_virtio_os_overlap_(p, n, d, sizeof *d)
        || mulib_virtio_os_overlap_(p, n, d->input, d->limit)
        || mulib_virtio_os_overlap_(p, n, d->output, d->limit)) return 0;
  }
  return 1;
}
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_call_(mulib_virtio_os_device_t *d, unsigned op, unsigned queue,
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
  d->negotiated = 0; d->i2c_failed = 0; d->can_started = 0;
  d->status = MULIB_VIRTIO_FAILED; d->epoch = 0;
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
      || (a < b + o->transfer_limit && b < a + o->transfer_limit)
      || mulib_virtio_os_overlap_(out, sizeof *out, o->input_scratch, o->transfer_limit)
      || mulib_virtio_os_overlap_(out, sizeof *out, o->output_scratch, o->transfer_limit))
    return MULIB_VMMDEV_EINVAL;
  if (p->id != MULIB_VIRTIO_9P && o->config_size != p->config_size) return MULIB_VMMDEV_EINVAL;
  memset(&d, 0, sizeof d);
  d.instance = o->instance; d.epoch = 1; d.profile = p;
  d.offered = o->features | MULIB_VIRTIO_VERSION_1;
  d.config_size = o->config_size; d.limit = o->transfer_limit;
  if (o->config_size) memcpy(d.config, o->config, o->config_size);
  d.input = o->input_scratch; d.output = o->output_scratch;
  d.service = o->service; d.context = o->context;
  switch (p->id) {
  case MULIB_VIRTIO_CAN:
    { int rc = mulib_virtio_os_validate_can_(&d); if (rc) return rc; }
    break;
  case MULIB_VIRTIO_BT:
    { int rc = mulib_virtio_os_validate_bt_(&d); if (rc) return rc; }
    break;
  case MULIB_VIRTIO_SPI:
    { int rc = mulib_virtio_os_validate_spi_(&d); if (rc) return rc; }
    break;
  case MULIB_VIRTIO_GPU:
    { int rc = mulib_virtio_os_validate_gpu_(&d); if (rc) return rc; }
    break;
  case MULIB_VIRTIO_VSOCK:
    { int rc = mulib_virtio_os_validate_vsock_(&d); if (rc) return rc; }
    break;
  case MULIB_VIRTIO_SCSI:
    { int rc = mulib_virtio_os_validate_scsi_(&d); if (rc) return rc; }
    break;
  case MULIB_VIRTIO_CRYPTO:
    { int rc = mulib_virtio_os_validate_crypto_(&d); if (rc) return rc; }
    break;
  case MULIB_VIRTIO_IOMMU:
    { int rc = mulib_virtio_os_validate_iommu_(&d); if (rc) return rc; }
    break;
  case MULIB_VIRTIO_SND:
    { int rc = mulib_virtio_os_validate_snd_(&d); if (rc) return rc; }
    break;
  case MULIB_VIRTIO_RPMB:
    { int rc = mulib_virtio_os_validate_rpmb_(&d); if (rc) return rc; }
    break;
  case MULIB_VIRTIO_BLK:
    { int rc = mulib_virtio_os_validate_blk_(&d); if (rc) return rc; }
    break;
  case MULIB_VIRTIO_BALLOON:
    { int rc = mulib_virtio_os_validate_balloon_(&d); if (rc) return rc; }
    break;
  case MULIB_VIRTIO_9P:
    { int rc = mulib_virtio_os_validate_p9_(&d); if (rc) return rc; }
    break;
  case MULIB_VIRTIO_INPUT:
    { int rc = mulib_virtio_os_validate_input_(&d); if (rc) return rc; }
    break;
  case MULIB_VIRTIO_FS:
    { int rc = mulib_virtio_os_validate_fs_(&d); if (rc) return rc; }
    break;
  case MULIB_VIRTIO_PMEM:
    { int rc = mulib_virtio_os_validate_pmem_(&d); if (rc) return rc; }
    break;
  case MULIB_VIRTIO_GPIO:
    { int rc = mulib_virtio_os_validate_gpio_(&d); if (rc) return rc; }
    break;
  case MULIB_VIRTIO_MEM:
    { int rc = mulib_virtio_os_validate_mem_(&d); if (rc) return rc; }
    break;
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
  if (!out || (!devices && n) || n > SIZE_MAX / sizeof *devices
      || memory.count > SIZE_MAX / sizeof *memory.regions) return MULIB_VMMDEV_EINVAL;
  rc = mulib_vmmdev_memory_validate(&memory); if (rc) return rc;
  if (!mulib_virtio_os_private_(&memory, out, sizeof *out)
      || !mulib_virtio_os_private_(&memory, devices, n * sizeof *devices)
      || !mulib_virtio_os_private_(&memory, memory.regions, memory.count * sizeof *memory.regions)
      || mulib_virtio_os_overlap_(out, sizeof *out, devices, n * sizeof *devices)
      || mulib_virtio_os_overlap_(out, sizeof *out, memory.regions, memory.count * sizeof *memory.regions))
    return MULIB_VMMDEV_EINVAL;
  for (size_t i = 0; i < n; ++i) {
    mulib_virtio_os_device_t *d = devices[i];
    if (!d || !d->profile || !d->instance || !d->epoch) return MULIB_VMMDEV_EINVAL;
    for (size_t j = 0; j < i; ++j)
      if (devices[j]->instance == d->instance) return MULIB_VMMDEV_EINVAL;
    /* Device metadata (including keys), inventories and scratch are never RAM. */
    if (!mulib_virtio_os_private_(&memory, d, sizeof *d)
        || !mulib_virtio_os_private_(&memory, d->input, d->limit)
        || !mulib_virtio_os_private_(&memory, d->output, d->limit)
        || mulib_virtio_os_overlap_(out, sizeof *out, d, sizeof *d)
        || mulib_virtio_os_overlap_(devices, n * sizeof *devices, d, sizeof *d)
        || mulib_virtio_os_overlap_(memory.regions, memory.count * sizeof *memory.regions, d, sizeof *d))
      return MULIB_VMMDEV_EINVAL;
    for (unsigned k = 0; k < 2; ++k) {
      const void *scratch = k ? d->output : d->input;
      if (mulib_virtio_os_overlap_(scratch, d->limit, out, sizeof *out)
          || mulib_virtio_os_overlap_(scratch, d->limit, devices, n * sizeof *devices)
          || mulib_virtio_os_overlap_(scratch, d->limit, memory.regions, memory.count * sizeof *memory.regions))
        return MULIB_VMMDEV_EINVAL;
      for (size_t j = 0; j < n; ++j)
        if (mulib_virtio_os_overlap_(scratch, d->limit, devices[j], sizeof *d)) return MULIB_VMMDEV_EINVAL;
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
static int mulib_virtio_os_dispatch_(mulib_virtio_os_device_t *d, const mulib_vmmdev_memory_t *memory, unsigned q,
                                    size_t ni, size_t no, size_t *used)
{
  *used = 0;
  switch (d->profile->id) {
  case MULIB_VIRTIO_NSM: return mulib_virtio_os_nsm_(d, q, ni, no, used);
  case MULIB_VIRTIO_RTC: return mulib_virtio_os_rtc_(d, q, ni, no, used);
  case MULIB_VIRTIO_CAN: return mulib_virtio_os_can_(d, q, ni, no, used);
  case MULIB_VIRTIO_BT: return mulib_virtio_os_bt_(d, q, ni, no, used);
  case MULIB_VIRTIO_SPI: return mulib_virtio_os_spi_(d, q, ni, no, used);
  case MULIB_VIRTIO_NET: return mulib_virtio_os_net_(d, q, ni, no, used);
  case MULIB_VIRTIO_BLK: return mulib_virtio_os_block_(d, ni, no, used);
  case MULIB_VIRTIO_CONSOLE: return mulib_virtio_os_console_(d, q, ni, no, used);
  case MULIB_VIRTIO_RNG: return mulib_virtio_os_rng_(d, q, ni, no, used);
  case MULIB_VIRTIO_BALLOON: return mulib_virtio_os_balloon_(d, q, ni, no, used);
  case MULIB_VIRTIO_RPMSG: return mulib_virtio_os_rpmsg_(d, q, ni, no, used);
  case MULIB_VIRTIO_SCSI: return mulib_virtio_os_scsi_(d, q, ni, no, used);
  case MULIB_VIRTIO_9P: return mulib_virtio_os_p9_(d, q, ni, no, used);
  case MULIB_VIRTIO_GPU: return mulib_virtio_os_gpu_(d, memory, q, ni, no, used);
  case MULIB_VIRTIO_INPUT: return mulib_virtio_os_input_(d, q, ni, no, used);
  case MULIB_VIRTIO_VSOCK: return mulib_virtio_os_vsock_(d, q, ni, no, used);
  case MULIB_VIRTIO_CRYPTO: return mulib_virtio_os_crypto_(d, q, ni, no, used);
  case MULIB_VIRTIO_IOMMU: return mulib_virtio_os_iommu_(d, q, ni, no, used);
  case MULIB_VIRTIO_MEM: return mulib_virtio_os_mem_(d, ni, no, used);
  case MULIB_VIRTIO_SND: return mulib_virtio_os_sound_(d, q, ni, no, used);
  case MULIB_VIRTIO_FS: return mulib_virtio_os_fs_(d, q, ni, no, used);
  case MULIB_VIRTIO_PMEM: return mulib_virtio_os_pmem_(d, q, ni, no, used);
  case MULIB_VIRTIO_RPMB: return mulib_virtio_os_rpmb_(d, ni, no, used);
  case MULIB_VIRTIO_SCMI: return mulib_virtio_os_scmi_(d, q, ni, no, used);
  case MULIB_VIRTIO_I2C: return mulib_virtio_os_i2c_(d, q, ni, no, used);
  case MULIB_VIRTIO_GPIO: return mulib_virtio_os_gpio_(d, q, ni, no, used);
  default: return MULIB_VMMDEV_ENOTSUP;
  }
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
    d->status = 0; d->negotiated = 0; d->i2c_failed = 0; d->can_started = 0; ++d->epoch;
    d->rtc_reading = 0;
    if (d->profile->id == MULIB_VIRTIO_CAN) memset(d->config, 0, 2);
    memset(d->pcm_state, 0, sizeof d->pcm_state); memset(d->pcm_period, 0, sizeof d->pcm_period);
    mulib_virtio_os_erase_(d->sessions, sizeof d->sessions);
    mulib_virtio_os_erase_(d->input, d->limit); mulib_virtio_os_erase_(d->output, d->limit);
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
  if (!request || !mulib_virtio_os_packet_(b, capacity, sizeof *capacity)
      || !mulib_virtio_os_packet_(b, request, n) || !mulib_virtio_os_packet_(b, response, *capacity)
      || mulib_virtio_os_overlap_(request, n, response, *capacity)
      || mulib_virtio_os_overlap_(capacity, sizeof *capacity, request, n)
      || mulib_virtio_os_overlap_(capacity, sizeof *capacity, response, *capacity)) return MULIB_VMMDEV_EINVAL;
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
