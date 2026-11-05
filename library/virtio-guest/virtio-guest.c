/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

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
MULIB_VIRTIO_INTERNAL int mulib_virtio_guest_workspace_(mulib_virtio_guest_t *g, size_t ni, size_t no)
{
  if (!g || !g->scratch) return MULIB_VMMDEV_EINVAL;
  if (ni > g->limit || no > g->limit || ni > g->scratch_size || no > g->scratch_size - ni)
    return MULIB_VMMDEV_ENOSPC;
  return 0;
}
MULIB_VIRTIO_INTERNAL int mulib_virtio_guest_prepared_(mulib_virtio_guest_t *g, unsigned q, size_t ni, size_t no, size_t *used)
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
static int mulib_virtio_guest_stream_(const mulib_virtio_guest_t *g)
{
  return g && (g->device_id == MULIB_VIRTIO_NET || g->device_id == MULIB_VIRTIO_CONSOLE
               || g->device_id == MULIB_VIRTIO_RPMSG || g->device_id == MULIB_VIRTIO_INPUT
               || g->device_id == MULIB_VIRTIO_BT);
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
  return mulib_virtio_guest_prepared_(g, g->device_id == MULIB_VIRTIO_BT ? 0 : 1, n + h, 0, &used);
}
int mulib_virtio_guest_receive(mulib_virtio_guest_t *g, void *p, size_t *capacity)
{
  size_t h, used;
  int rc;
  if (!mulib_virtio_guest_stream_(g) || !p || !capacity || *capacity > MULIB_VIRTIO_TRANSFER_MAX - 12)
    return MULIB_VMMDEV_EINVAL;
  h = g->device_id == MULIB_VIRTIO_NET ? 12 : 0;
  rc = mulib_virtio_guest_prepared_(g, g->device_id == MULIB_VIRTIO_BT ? 1 : 0,
                                   0, *capacity + h, &used); if (rc) return rc;
  if (used < h) return MULIB_VMMDEV_EIO;
  if (h) for (size_t i = 0; i < h; ++i) if (g->scratch[i]) return MULIB_VMMDEV_EIO;
  memcpy(p, g->scratch + h, used - h); *capacity = used - h; return 0;
}
