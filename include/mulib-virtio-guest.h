/**
 * @file mulib-virtio-guest.h
 * @brief Virtio frontend negotiation, queues and device requests over MuLIB VMM.
 * Define MULIB_IMPL_VIRTIO_GUEST and VIRTIO, VMMDEV, ENDIAN switches once.
 */
#ifndef MULIB_VIRTIO_GUEST_H
#define MULIB_VIRTIO_GUEST_H
#include "mulib-virtio.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Borrowed transport and guest DMA workspace. Serialize access; never copy an active client. */
typedef struct mulib_virtio_guest {
  mulib_vmmdev_exchange_fn exchange;
  void *context;
  uint32_t instance, epoch, device_id, status, queues, config_size, generation, limit;
  uint64_t token, offered, features, address;
  unsigned char *scratch;
  size_t scratch_size;
} mulib_virtio_guest_t;
/**
 * @brief Probe/reset/negotiate through FEATURES_OK. Required features are automatic.
 * scratch is guest RAM at address, writable by both sides, with at least 512 bytes.
 * optional_features may contain only features understood by this profile.
 * On failure out is zeroed. A partially initialized remote device is marked FAILED.
 */
int mulib_virtio_guest_attach(mulib_virtio_guest_t *out, mulib_vmmdev_exchange_fn exchange,
                             void *context, uint32_t instance, uint64_t optional_features,
                             uint64_t address, void *scratch, size_t scratch_size);
/** @brief Finish initialization; submission becomes possible after success. */
int mulib_virtio_guest_start(mulib_virtio_guest_t *guest);
/** @brief Reset and invalidate submissions. Reattach to negotiate a new connection. */
int mulib_virtio_guest_reset(mulib_virtio_guest_t *guest);
/** @brief Atomic config read; generation is recorded in the client. */
int mulib_virtio_guest_config_read(mulib_virtio_guest_t *guest, size_t offset, void *out, size_t size);
/** @brief Write only profile-defined driver-writable configuration fields. */
int mulib_virtio_guest_config_write(mulib_virtio_guest_t *guest, size_t offset, const void *bytes, size_t size);
/** @brief Submit caller-managed DMA spans; writable flags=1, readable flags=0. */
int mulib_virtio_guest_submit(mulib_virtio_guest_t *guest, unsigned queue,
                             const mulib_vmmdev_span_t *spans, unsigned count, size_t *used);
/**
 * @brief Copy a request into guest scratch, submit it and copy the used response out.
 * capacity is input space/output actual bytes. Input/output must not overlap scratch.
 * AGAIN consumes nothing. Host services may still have side effects on other errors.
 */
int mulib_virtio_guest_transact(mulib_virtio_guest_t *guest, unsigned queue,
                               const void *input, size_t input_size, void *output, size_t *capacity);
/** @brief Read entropy; length in/out. Short reads are valid. */
int mulib_virtio_guest_entropy(mulib_virtio_guest_t *guest, void *out, size_t *size);
/** @brief Block read/write (types 0/1), flush (4), or get-id (8). Sector units are 512 bytes. */
int mulib_virtio_guest_block(mulib_virtio_guest_t *guest, uint32_t type, uint64_t sector,
                            void *data, size_t size);
/** @brief Send one network packet, console bytes, RPMsg frame or input status events. */
int mulib_virtio_guest_send(mulib_virtio_guest_t *guest, const void *bytes, size_t size);
/** @brief Receive one network packet, console bytes, RPMsg frame or input events. */
int mulib_virtio_guest_receive(mulib_virtio_guest_t *guest, void *bytes, size_t *capacity);
/** @brief GPIO get/set; value is input for setters, output for getters. */
int mulib_virtio_guest_gpio(mulib_virtio_guest_t *guest, uint16_t type, uint16_t line, uint32_t *value);
/** @brief I2C transaction segment; flags bit1 selects read, bit0 groups requests. */
int mulib_virtio_guest_i2c(mulib_virtio_guest_t *guest, uint16_t wire_address, uint32_t flags,
                          void *bytes, size_t size);
/** @brief Flush persistent memory. */
int mulib_virtio_guest_pmem_flush(mulib_virtio_guest_t *guest);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VIRTIO_GUEST) && !defined(MULIB_VIRTIO_GUEST_IMPLEMENTED)
#define MULIB_VIRTIO_GUEST_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_virtio_guest_call_(mulib_virtio_guest_t *g, uint16_t op, uint64_t arg, uint32_t count,
                                   const void *payload, size_t n, unsigned char *reply,
                                   mulib_vmmdev_message_t *r)
{
  unsigned char request[MULIB_VMMDEV_MAX_PACKET];
  mulib_vmmdev_message_t m;
  size_t reqsize = sizeof request, repsize = MULIB_VMMDEV_MAX_PACKET;
  int rc;
  if (!g || !g->exchange || !g->instance || n > MULIB_VMMDEV_MAX_PACKET - 40
      || g->token == UINT64_MAX) return MULIB_VMMDEV_EINVAL;
  memset(&m, 0, sizeof m); m.opcode = op; m.device = g->instance;
  m.epoch = op == MULIB_VMMDEV_PROBE ? 0 : g->epoch; m.token = ++g->token;
  m.argument = arg; m.count = count; m.payload = (const unsigned char *)payload; m.length = (uint32_t)n;
  rc = mulib_vmmdev_encode(&m, request, &reqsize); if (rc) return rc;
  rc = g->exchange(g->context, request, reqsize, reply, &repsize); if (rc) return rc;
  if (repsize > MULIB_VMMDEV_MAX_PACKET) return MULIB_VMMDEV_EIO;
  rc = mulib_vmmdev_decode(reply, repsize, r); if (rc) return rc;
  if (r->opcode != (op | MULIB_VMMDEV_RESPONSE) || r->device != g->instance || r->token != m.token
      || r->argument > MULIB_VMMDEV_ENOSPC) return MULIB_VMMDEV_EIO;
  if (r->argument) {
    if (r->count || r->length) return MULIB_VMMDEV_EIO;
    return (int)r->argument;
  }
  if (op == MULIB_VMMDEV_PROBE) {
    if (!r->epoch || r->length != 32 || r->count) return MULIB_VMMDEV_EIO;
  } else if (op == MULIB_VMMDEV_RESET) {
    if (g->epoch == UINT32_MAX || r->epoch != g->epoch + 1) return MULIB_VMMDEV_EIO;
  } else if (r->epoch != g->epoch) return MULIB_VMMDEV_ESTALE;
  if (op != MULIB_VMMDEV_PROBE && op != MULIB_VMMDEV_CONFIG_READ && r->length) return MULIB_VMMDEV_EIO;
  if (op != MULIB_VMMDEV_SUBMIT && op != MULIB_VMMDEV_CONFIG_READ && r->count) return MULIB_VMMDEV_EIO;
  return 0;
}
static int mulib_virtio_guest_status_(mulib_virtio_guest_t *g, unsigned status)
{
  unsigned char reply[MULIB_VMMDEV_MAX_PACKET];
  mulib_vmmdev_message_t r;
  int rc = mulib_virtio_guest_call_(g, MULIB_VMMDEV_STATUS, status, 0, NULL, 0, reply, &r);
  if (!rc) g->status = status;
  return rc;
}
int mulib_virtio_guest_reset(mulib_virtio_guest_t *g)
{
  unsigned char reply[MULIB_VMMDEV_MAX_PACKET];
  mulib_vmmdev_message_t r;
  int rc = mulib_virtio_guest_call_(g, MULIB_VMMDEV_RESET, 0, 0, NULL, 0, reply, &r);
  if (!rc) { g->epoch = r.epoch; g->status = 0; g->features = 0; }
  return rc;
}
int mulib_virtio_guest_attach(mulib_virtio_guest_t *out, mulib_vmmdev_exchange_fn fn,
                             void *context, uint32_t instance, uint64_t optional,
                             uint64_t address, void *scratch, size_t size)
{
  mulib_virtio_guest_t g;
  mulib_vmmdev_message_t r;
  const mulib_virtio_profile_t *p;
  unsigned char reply[MULIB_VMMDEV_MAX_PACKET];
  int rc;
  if (!out) return MULIB_VMMDEV_EINVAL;
  memset(out, 0, sizeof *out);
  if (!fn || !instance || !scratch || size < 512 || size > UINT64_MAX - address) return MULIB_VMMDEV_EINVAL;
  memset(&g, 0, sizeof g); g.exchange = fn; g.context = context; g.instance = instance;
  g.address = address; g.scratch = (unsigned char *)scratch; g.scratch_size = size;
  rc = mulib_virtio_guest_call_(&g, MULIB_VMMDEV_PROBE, 0, 0, NULL, 0, reply, &r); if (rc) return rc;
  g.epoch = r.epoch; g.device_id = mulib_endian_load_le32(r.payload);
  g.status = mulib_endian_load_le32(r.payload + 4); g.offered = mulib_vmmdev_load64(r.payload + 8);
  g.queues = mulib_endian_load_le32(r.payload + 16); g.config_size = mulib_endian_load_le32(r.payload + 20);
  g.generation = mulib_endian_load_le32(r.payload + 24); g.limit = mulib_endian_load_le32(r.payload + 28);
  p = mulib_virtio_profile(g.device_id);
  if (!p || p->support != MULIB_VIRTIO_PROFILE || (optional & ~(p->features | MULIB_VIRTIO_VERSION_1)))
    return MULIB_VMMDEV_ENOTSUP;
  if (g.queues != p->queues || g.config_size < p->config_size || g.config_size > MULIB_VIRTIO_CONFIG_MAX
      || g.limit < 512 || g.limit > MULIB_VIRTIO_TRANSFER_MAX) return MULIB_VMMDEV_EIO;
  if (!(g.offered & MULIB_VIRTIO_VERSION_1) || (g.offered & p->required_features) != p->required_features)
    return MULIB_VMMDEV_ENOTSUP;
  rc = mulib_virtio_guest_reset(&g); if (rc) return rc;
  rc = mulib_virtio_guest_status_(&g, 1); if (rc) goto fail;
  rc = mulib_virtio_guest_status_(&g, 3); if (rc) goto fail;
  g.features = MULIB_VIRTIO_VERSION_1 | p->required_features | (optional & g.offered);
  rc = mulib_virtio_guest_call_(&g, MULIB_VMMDEV_FEATURES, g.features, 0, NULL, 0, reply, &r);
  if (rc) goto fail;
  rc = mulib_virtio_guest_status_(&g, 11); if (rc) goto fail;
  *out = g; return 0;
fail:
  (void)mulib_virtio_guest_status_(&g, g.status | MULIB_VIRTIO_FAILED);
  return rc;
}
int mulib_virtio_guest_start(mulib_virtio_guest_t *g)
{
  if (!g || g->status != 11) return MULIB_VMMDEV_ESTATE;
  return mulib_virtio_guest_status_(g, 15);
}
int mulib_virtio_guest_config_read(mulib_virtio_guest_t *g, size_t off, void *out, size_t n)
{
  unsigned char reply[MULIB_VMMDEV_MAX_PACKET];
  mulib_vmmdev_message_t r;
  int rc;
  if (!g || (!out && n) || off > g->config_size || n > g->config_size - off) return MULIB_VMMDEV_EINVAL;
  rc = mulib_virtio_guest_call_(g, MULIB_VMMDEV_CONFIG_READ, off, (uint32_t)n, NULL, 0, reply, &r);
  if (rc) return rc;
  if (r.length != n) return MULIB_VMMDEV_EIO;
  if (n) memcpy(out, r.payload, n);
  g->generation = r.count; return 0;
}
int mulib_virtio_guest_config_write(mulib_virtio_guest_t *g, size_t off, const void *p, size_t n)
{
  unsigned char reply[MULIB_VMMDEV_MAX_PACKET];
  mulib_vmmdev_message_t r;
  if (!g || (!p && n) || off > g->config_size || n > g->config_size - off) return MULIB_VMMDEV_EINVAL;
  return mulib_virtio_guest_call_(g, MULIB_VMMDEV_CONFIG_WRITE, off, 0, p, n, reply, &r);
}
int mulib_virtio_guest_submit(mulib_virtio_guest_t *g, unsigned q,
                             const mulib_vmmdev_span_t *spans, unsigned count, size_t *used)
{
  unsigned char wire[16 * MULIB_VMMDEV_MAX_SPANS], reply[MULIB_VMMDEV_MAX_PACKET];
  mulib_vmmdev_message_t r;
  size_t ni = 0, no = 0;
  int writing = 0, rc;
  if (!g || !used || !spans || !count || count > MULIB_VMMDEV_MAX_SPANS) return MULIB_VMMDEV_EINVAL;
  *used = 0;
  if (g->status != 15) return MULIB_VMMDEV_ESTATE;
  if (q >= g->queues) return MULIB_VMMDEV_EBOUNDS;
  for (unsigned i = 0; i < count; ++i) {
    size_t *total = spans[i].flags ? &no : &ni;
    if (!spans[i].length || spans[i].flags > 1 || (writing && !spans[i].flags)
        || spans[i].length > UINT64_MAX - spans[i].address) return MULIB_VMMDEV_EINVAL;
    if (spans[i].flags) writing = 1;
    if (spans[i].length > g->limit - *total) return MULIB_VMMDEV_EBOUNDS;
    *total += spans[i].length;
    mulib_vmmdev_store64(wire + 16u * i, spans[i].address);
    mulib_endian_store_le32(wire + 16u * i + 8, spans[i].length);
    mulib_endian_store_le32(wire + 16u * i + 12, spans[i].flags);
  }
  rc = mulib_virtio_guest_call_(g, MULIB_VMMDEV_SUBMIT, q, count, wire, 16u * count, reply, &r);
  if (rc) return rc;
  if (r.count > no) return MULIB_VMMDEV_EIO;
  *used = r.count; return 0;
}
static int mulib_virtio_guest_workspace_(mulib_virtio_guest_t *g, size_t ni, size_t no)
{
  if (!g || !g->scratch) return MULIB_VMMDEV_EINVAL;
  if (ni > g->limit || no > g->limit || ni > g->scratch_size || no > g->scratch_size - ni)
    return MULIB_VMMDEV_ENOSPC;
  return 0;
}
static int mulib_virtio_guest_prepared_(mulib_virtio_guest_t *g, unsigned q, size_t ni, size_t no, size_t *used)
{
  mulib_vmmdev_span_t spans[2];
  unsigned count = 0;
  int rc = mulib_virtio_guest_workspace_(g, ni, no); if (rc) return rc;
  if (ni) { spans[count].address = g->address; spans[count].length = (uint32_t)ni; spans[count++].flags = 0; }
  if (no) {
    spans[count].address = g->address + ni; spans[count].length = (uint32_t)no; spans[count++].flags = 1;
    memset(g->scratch + ni, 0, no);
  }
  return mulib_virtio_guest_submit(g, q, spans, count, used);
}
int mulib_virtio_guest_transact(mulib_virtio_guest_t *g, unsigned q, const void *in, size_t ni,
                               void *out, size_t *capacity)
{
  size_t used;
  int rc;
  if (!capacity || (!in && ni) || (!out && *capacity)) return MULIB_VMMDEV_EINVAL;
  rc = mulib_virtio_guest_workspace_(g, ni, *capacity); if (rc) return rc;
  if (ni) memcpy(g->scratch, in, ni);
  rc = mulib_virtio_guest_prepared_(g, q, ni, *capacity, &used); if (rc) return rc;
  if (used) memcpy(out, g->scratch + ni, used);
  *capacity = used; return 0;
}
int mulib_virtio_guest_entropy(mulib_virtio_guest_t *g, void *out, size_t *n)
{
  if (!g || g->device_id != MULIB_VIRTIO_RNG) return MULIB_VMMDEV_EINVAL;
  return mulib_virtio_guest_transact(g, 0, NULL, 0, out, n);
}
int mulib_virtio_guest_block(mulib_virtio_guest_t *g, uint32_t type, uint64_t sector, void *data, size_t n)
{
  size_t ni, no, used;
  unsigned char status;
  int rc;
  if (!g || g->device_id != MULIB_VIRTIO_BLK || (!data && n)
      || ((type == 0 || type == 1) && n % 512) || (type == 4 && (n || sector))
      || (type == 8 && (n != 20 || sector)) || (type != 0 && type != 1 && type != 4 && type != 8)
      || n > MULIB_VIRTIO_TRANSFER_MAX - 16) return MULIB_VMMDEV_EINVAL;
  ni = type == 1 ? 16 + n : 16; no = type == 0 || type == 8 ? n + 1 : 1;
  rc = mulib_virtio_guest_workspace_(g, ni, no); if (rc) return rc;
  mulib_virtio_blk_header(g->scratch, type, sector);
  if (type == 1 && n) memcpy(g->scratch + 16, data, n);
  rc = mulib_virtio_guest_prepared_(g, 0, ni, no, &used); if (rc) return rc;
  if (used != no) return MULIB_VMMDEV_EIO;
  status = g->scratch[ni + no - 1];
  if (status) return status == 2 ? MULIB_VMMDEV_ENOTSUP : MULIB_VMMDEV_EIO;
  if ((type == 0 || type == 8) && n) memcpy(data, g->scratch + ni, n);
  return 0;
}
static int mulib_virtio_guest_stream_(const mulib_virtio_guest_t *g)
{
  return g && (g->device_id == MULIB_VIRTIO_NET || g->device_id == MULIB_VIRTIO_CONSOLE
               || g->device_id == MULIB_VIRTIO_RPMSG || g->device_id == MULIB_VIRTIO_INPUT);
}
int mulib_virtio_guest_send(mulib_virtio_guest_t *g, const void *p, size_t n)
{
  size_t h, used;
  int rc;
  if (!mulib_virtio_guest_stream_(g) || !p || !n || n > MULIB_VIRTIO_TRANSFER_MAX - 12)
    return MULIB_VMMDEV_EINVAL;
  h = g->device_id == MULIB_VIRTIO_NET ? 12 : 0;
  rc = mulib_virtio_guest_workspace_(g, n + h, 0); if (rc) return rc;
  memset(g->scratch, 0, h); memcpy(g->scratch + h, p, n);
  return mulib_virtio_guest_prepared_(g, 1, n + h, 0, &used);
}
int mulib_virtio_guest_receive(mulib_virtio_guest_t *g, void *p, size_t *capacity)
{
  size_t h, used;
  int rc;
  if (!mulib_virtio_guest_stream_(g) || !p || !capacity || *capacity > MULIB_VIRTIO_TRANSFER_MAX - 12)
    return MULIB_VMMDEV_EINVAL;
  h = g->device_id == MULIB_VIRTIO_NET ? 12 : 0;
  rc = mulib_virtio_guest_prepared_(g, 0, 0, *capacity + h, &used); if (rc) return rc;
  if (used < h) return MULIB_VMMDEV_EIO;
  if (h) for (size_t i = 0; i < h; ++i) if (g->scratch[i]) return MULIB_VMMDEV_EIO;
  memcpy(p, g->scratch + h, used - h); *capacity = used - h; return 0;
}
int mulib_virtio_guest_gpio(mulib_virtio_guest_t *g, uint16_t type, uint16_t line, uint32_t *value)
{
  unsigned char request[8], response[2];
  size_t n = 2;
  int rc;
  if (!g || g->device_id != MULIB_VIRTIO_GPIO || !value || type < 2 || type > 5) return MULIB_VMMDEV_EINVAL;
  rc = mulib_virtio_gpio_request(request, type, line, type == 3 || type == 5 ? *value : 0);
  if (rc) return rc;
  rc = mulib_virtio_guest_transact(g, 0, request, 8, response, &n); if (rc) return rc;
  if (n != 2 || response[0] || (type == 2 && response[1] > 2) || (type == 4 && response[1] > 1))
    return MULIB_VMMDEV_EIO;
  if (type == 2 || type == 4) *value = response[1];
  return 0;
}
int mulib_virtio_guest_i2c(mulib_virtio_guest_t *g, uint16_t address, uint32_t flags, void *bytes, size_t n)
{
  size_t ni, no, used;
  unsigned char h[8];
  int rc;
  if (!g || g->device_id != MULIB_VIRTIO_I2C || (!bytes && n) || n > MULIB_VIRTIO_TRANSFER_MAX - 8)
    return MULIB_VMMDEV_EINVAL;
  rc = mulib_virtio_i2c_header(h, address, flags); if (rc) return rc;
  ni = flags & 2 ? 8 : 8 + n; no = flags & 2 ? n + 1 : 1;
  rc = mulib_virtio_guest_workspace_(g, ni, no); if (rc) return rc;
  memcpy(g->scratch, h, 8);
  if (!(flags & 2) && n) memcpy(g->scratch + 8, bytes, n);
  rc = mulib_virtio_guest_prepared_(g, 0, ni, no, &used); if (rc) return rc;
  if (used != no || g->scratch[ni + no - 1]) return MULIB_VMMDEV_EIO;
  if ((flags & 2) && n) memcpy(bytes, g->scratch + ni, n);
  return 0;
}
int mulib_virtio_guest_pmem_flush(mulib_virtio_guest_t *g)
{
  unsigned char req[4] = {0}, rep[4];
  size_t n = 4;
  int rc;
  if (!g || g->device_id != MULIB_VIRTIO_PMEM) return MULIB_VMMDEV_EINVAL;
  rc = mulib_virtio_guest_transact(g, 0, req, 4, rep, &n); if (rc) return rc;
  return n == 4 && !mulib_endian_load_le32(rep) ? 0 : MULIB_VMMDEV_EIO;
}
#ifdef __cplusplus
}
#endif
#endif
