/**
 * @file mulib-vmm.h
 * @brief Serialized type-2 VMM lifecycle, device isolation and guest-call dispatch.
 * Define MULIB_IMPL_VMM plus VMM_MEMORY, VIRTIO_OS, VIRTIO, VMMDEV and ENDIAN once.
 * All RAM, devices and execution providers are borrowed. No hidden allocation.
 */
#ifndef MULIB_VMM_H
#define MULIB_VMM_H
#include "mulib-vmm-memory.h"
#include "mulib-virtio-os.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_VMM_MAX_REGIONS 32u
#define MULIB_VMM_MAX_DEVICES 32u
#define MULIB_VMM_CALL_SIZE 32u
#define MULIB_VMM_INVENTORY_MAX (16u + 8u * MULIB_VMM_MAX_DEVICES)
enum mulib_vmm_state {
  MULIB_VMM_EMPTY, MULIB_VMM_READY, MULIB_VMM_RUNNING,
  MULIB_VMM_HALTED, MULIB_VMM_FAULTED, MULIB_VMM_CLOSED
};
enum mulib_vmm_exit_kind { MULIB_VMM_EXIT_CALL = 1, MULIB_VMM_EXIT_HALT, MULIB_VMM_EXIT_YIELD };
/** @brief Executor exit. CALL supplies a guest physical 32-byte call-frame address. */
typedef struct mulib_vmm_exit {
  unsigned kind;
  uint64_t address;
  uint32_t native_reason;
  int result;
} mulib_vmm_exit_t;
/** @brief A stopped-on-exit execution engine; callbacks run while the VM lock is held. */
typedef struct mulib_vmm_executor {
  int (*step)(void *context, mulib_vmm_exit_t *exit);
  int (*reset)(void *context);
  void *context;
} mulib_vmm_executor_t;
/** @brief Device and its explicit DMA allowlist. An empty allowlist grants no DMA. */
typedef struct mulib_vmm_binding {
  mulib_virtio_os_device_t *device;
  mulib_vmmdev_memory_t dma;
} mulib_vmm_binding_t;
/** @brief Configuration; inventory metadata is copied, backing storage remains borrowed. */
typedef struct mulib_vmm_options {
  mulib_vmmdev_memory_t ram;
  const mulib_vmm_binding_t *devices;
  size_t device_count;
  mulib_vmm_executor_t executor;
  int (*lock)(void *context);
  void (*unlock)(void *context);
  void *lock_context;
} mulib_vmm_options_t;
/** @brief Private slot storage, public only to support caller allocation. */
typedef struct mulib_vmm_slot {
  mulib_virtio_os_device_t *device;
  mulib_vmmdev_region_t regions[MULIB_VMM_MAX_REGIONS];
  mulib_virtio_os_bus_t bus;
} mulib_vmm_slot_t;
/** @brief Stable-address VM. Initialize with {0}; never copy after init. */
typedef struct mulib_vmm {
  const struct mulib_vmm *self;
  unsigned state, busy;
  int last_error;
  uint64_t dispatches;
  mulib_vmmdev_region_t regions[MULIB_VMM_MAX_REGIONS];
  mulib_vmmdev_memory_t ram;
  mulib_vmm_slot_t slots[MULIB_VMM_MAX_DEVICES];
  size_t device_count;
  mulib_vmm_executor_t executor;
  int (*lock)(void *context);
  void (*unlock)(void *context);
  void *lock_context;
  unsigned char request[MULIB_VMMDEV_MAX_PACKET], response[MULIB_VMMDEV_MAX_PACKET];
} mulib_vmm_t;
#define MULIB_VMM_INIT {0}
/** @brief Publish validated RAM/devices in READY state; unchanged on failure. */
int mulib_vmm_init(mulib_vmm_t *vm, const mulib_vmm_options_t *options);
/** @brief Start/resume a READY execution engine. */
int mulib_vmm_start(mulib_vmm_t *vm);
/** @brief Pause between exits, retaining CPU/device state. */
int mulib_vmm_stop(mulib_vmm_t *vm);
/** @brief Run one executor exit and service at most one guest call. */
int mulib_vmm_step(mulib_vmm_t *vm, mulib_vmm_exit_t *exit);
/** @brief Reset devices then executor; partial failure leaves FAULTED and may be retried. */
int mulib_vmm_reset(mulib_vmm_t *vm);
/** @brief Reset devices and retire borrowed bindings; idempotent, retryable on failure. */
int mulib_vmm_close(mulib_vmm_t *vm);
/** @brief Local serialized MVMD exchange, compatible with mulib_virtio_guest_attach. */
int mulib_vmm_exchange(void *vm, const void *request, size_t size, void *response, size_t *capacity);
/** @brief Dispatch guest-addressed packet. Capacity in; bytes out, or required size on ENOSPC. */
int mulib_vmm_call(mulib_vmm_t *vm, uint64_t request, size_t size, uint64_t response, size_t *capacity);
/** @brief Process a call frame, storing transport status/length at offsets 24/28. */
int mulib_vmm_trap(mulib_vmm_t *vm, uint64_t frame_address);
/** @brief Encode immutable MVID boot discovery into disjoint host memory. */
int mulib_vmm_inventory(mulib_vmm_t *vm, void *output, size_t *capacity);
/** @brief Load an image into writable guest RAM while READY; atomic bounds validation. */
int mulib_vmm_load(mulib_vmm_t *vm, uint64_t address, const void *image, size_t size);
/** @brief Serialized host balloon/virtio-mem target update. */
int mulib_vmm_request_memory(mulib_vmm_t *vm, uint32_t instance, uint64_t amount);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VMM) && !defined(MULIB_VMM_IMPLEMENTED)
#define MULIB_VMM_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_vmm_overlap_(uint64_t a, size_t an, uint64_t b, size_t bn)
{ return an && bn && a < b + bn && b < a + an; }
static int mulib_vmm_host_private_(const mulib_vmmdev_memory_t *m, const void *p, size_t n)
{
  uintptr_t a = (uintptr_t)p;
  if ((!p && n) || n > UINTPTR_MAX - a) return 0;
  for (size_t i = 0; i < m->count; ++i) {
    uintptr_t b = (uintptr_t)m->regions[i].data;
    if (n && a < b + m->regions[i].size && b < a + n) return 0;
  }
  return 1;
}
static int mulib_vmm_enter_(mulib_vmm_t *v)
{
  int rc;
  if (!v || v->self != v) return MULIB_VMMDEV_EINVAL;
  if (v->lock) { rc = v->lock(v->lock_context); if (rc) return rc; }
  if (v->busy) { if (v->unlock) v->unlock(v->lock_context); return MULIB_VMMDEV_ESTATE; }
  v->busy = 1; return 0;
}
static int mulib_vmm_leave_(mulib_vmm_t *v, int rc)
{
  v->last_error = rc; v->busy = 0;
  if (v->unlock) v->unlock(v->lock_context);
  return rc;
}
static int mulib_vmm_live_(const mulib_vmm_t *v)
{ return v->state == MULIB_VMM_READY || v->state == MULIB_VMM_RUNNING; }
int mulib_vmm_init(mulib_vmm_t *out, const mulib_vmm_options_t *o)
{
  mulib_vmm_t v;
  int rc;
  if (!out || !o || o->ram.count > MULIB_VMM_MAX_REGIONS || o->device_count > MULIB_VMM_MAX_DEVICES
      || (!o->devices && o->device_count) || (!!o->lock != !!o->unlock)
      || (!!o->executor.step != !!o->executor.reset)) return MULIB_VMMDEV_EINVAL;
  if (out->state != MULIB_VMM_EMPTY && out->state != MULIB_VMM_CLOSED) return MULIB_VMMDEV_ESTATE;
  rc = mulib_vmmdev_memory_validate(&o->ram); if (rc) return rc;
  if (!mulib_vmm_host_private_(&o->ram, out, sizeof *out)) return MULIB_VMMDEV_EACCESS;
  for (size_t i = 0; i < o->ram.count; ++i)
    if (!(o->ram.regions[i].permissions & MULIB_VMMDEV_READ)) return MULIB_VMMDEV_EACCESS;
  memset(&v, 0, sizeof v); v.ram.count = o->ram.count; v.ram.regions = v.regions;
  if (o->ram.count) memcpy(v.regions, o->ram.regions, o->ram.count * sizeof v.regions[0]);
  v.device_count = o->device_count; v.executor = o->executor;
  v.lock = o->lock; v.unlock = o->unlock; v.lock_context = o->lock_context;
  for (size_t i = 0; i < o->device_count; ++i) {
    const mulib_vmm_binding_t *binding = o->devices + i;
    mulib_virtio_os_device_t *d = binding->device;
    mulib_vmm_slot_t *slot = v.slots + i;
    mulib_vmmdev_memory_t view;
    if (!d || !d->profile || !d->instance || !d->epoch || d->status
        || !d->service || !d->input || !d->output || d->limit < 512
        || d->limit > MULIB_VIRTIO_TRANSFER_MAX || binding->dma.count > MULIB_VMM_MAX_REGIONS)
      return MULIB_VMMDEV_EINVAL;
    if (!mulib_vmm_host_private_(&v.ram, d, sizeof *d)
        || !mulib_vmm_host_private_(&v.ram, d->input, d->limit)
        || !mulib_vmm_host_private_(&v.ram, d->output, d->limit)) return MULIB_VMMDEV_EACCESS;
    for (size_t j = 0; j < i; ++j)
      if (v.slots[j].device->instance == d->instance) return MULIB_VMMDEV_EINVAL;
    rc = mulib_vmm_memory_subset(&v.ram, &binding->dma); if (rc) return rc;
    slot->device = d;
    if (binding->dma.count) memcpy(slot->regions, binding->dma.regions, binding->dma.count * sizeof slot->regions[0]);
    view.regions = slot->regions; view.count = binding->dma.count;
    rc = mulib_virtio_os_bus_init(&slot->bus, &slot->device, 1, view); if (rc) return rc;
  }
  v.state = MULIB_VMM_READY; *out = v; out->self = out; out->ram.regions = out->regions;
  for (size_t i = 0; i < out->device_count; ++i) {
    out->slots[i].bus.devices = &out->slots[i].device;
    out->slots[i].bus.memory.regions = out->slots[i].regions;
  }
  return 0;
}
static int mulib_vmm_dispatch_(mulib_vmm_t *v, size_t size, size_t *capacity)
{
  mulib_vmmdev_message_t m;
  mulib_virtio_os_bus_t empty;
  int rc = mulib_vmmdev_decode(v->request, size, &m); if (rc) return rc;
  if (v->dispatches == UINT64_MAX) return MULIB_VMMDEV_ESTATE;
  ++v->dispatches;
  for (size_t i = 0; i < v->device_count; ++i)
    if (v->slots[i].device->instance == m.device)
      return mulib_virtio_os_exchange(&v->slots[i].bus, v->request, size, v->response, capacity);
  memset(&empty, 0, sizeof empty);
  return mulib_virtio_os_exchange(&empty, v->request, size, v->response, capacity);
}
static int mulib_vmm_call_(mulib_vmm_t *v, uint64_t request, size_t size, uint64_t response,
                           size_t *capacity, uint64_t frame, size_t frame_size)
{
  unsigned char *input, *output;
  mulib_vmmdev_message_t m;
  size_t cap = *capacity, written;
  int rc;
  *capacity = 0;
  if (!mulib_vmm_live_(v)) return MULIB_VMMDEV_ESTATE;
  if (size < 40 || size > MULIB_VMMDEV_MAX_PACKET || cap > MULIB_VMMDEV_MAX_PACKET
      || size > UINT64_MAX - request || cap > UINT64_MAX - response) return MULIB_VMMDEV_EINVAL;
  if (cap < 40) { *capacity = 40; return MULIB_VMMDEV_ENOSPC; }
  if (mulib_vmm_overlap_(request, size, response, cap)
      || mulib_vmm_overlap_(frame, frame_size, request, size)
      || mulib_vmm_overlap_(frame, frame_size, response, cap)) return MULIB_VMMDEV_EINVAL;
  rc = mulib_vmmdev_map(&v->ram, request, size, MULIB_VMMDEV_READ, &input); if (rc) return rc;
  rc = mulib_vmmdev_map(&v->ram, response, cap, MULIB_VMMDEV_WRITE, &output); if (rc) return rc;
  memcpy(v->request, input, size);
  rc = mulib_vmmdev_decode(v->request, size, &m); if (rc) return rc;
  if (m.opcode == MULIB_VMMDEV_SUBMIT) {
    if (!m.count || m.count > MULIB_VMMDEV_MAX_SPANS || m.length != 16u * m.count) return MULIB_VMMDEV_EINVAL;
    for (uint32_t i = 0; i < m.count; ++i) {
      const unsigned char *span = m.payload + 16 * i;
      uint64_t a = mulib_vmmdev_load64(span); size_t n = mulib_endian_load_le32(span + 8);
      if (!n || n > UINT64_MAX - a || mulib_vmm_overlap_(a, n, request, size)
          || mulib_vmm_overlap_(a, n, response, cap) || mulib_vmm_overlap_(a, n, frame, frame_size))
        return MULIB_VMMDEV_EINVAL;
    }
  }
  written = cap; rc = mulib_vmm_dispatch_(v, size, &written);
  if (rc == MULIB_VMMDEV_ENOSPC) *capacity = written;
  if (rc) return rc;
  if (written > cap) return MULIB_VMMDEV_EIO;
  memcpy(output, v->response, written); *capacity = written; return 0;
}
int mulib_vmm_call(mulib_vmm_t *v, uint64_t request, size_t size, uint64_t response, size_t *capacity)
{
  int rc;
  if (!capacity) return MULIB_VMMDEV_EINVAL;
  rc = mulib_vmm_enter_(v); if (rc) return rc;
  rc = mulib_vmm_call_(v, request, size, response, capacity, 0, 0);
  return mulib_vmm_leave_(v, rc);
}
static int mulib_vmm_trap_(mulib_vmm_t *v, uint64_t address)
{
  unsigned char *frame, snapshot[24];
  size_t capacity;
  int rc;
  if (!mulib_vmm_live_(v)) return MULIB_VMMDEV_ESTATE;
  rc = mulib_vmmdev_map(&v->ram, address, MULIB_VMM_CALL_SIZE, 3, &frame); if (rc) return rc;
  memcpy(snapshot, frame, sizeof snapshot); capacity = mulib_endian_load_le32(snapshot + 20);
  rc = mulib_vmm_call_(v, mulib_vmmdev_load64(snapshot), mulib_endian_load_le32(snapshot + 16),
                       mulib_vmmdev_load64(snapshot + 8), &capacity, address, MULIB_VMM_CALL_SIZE);
  mulib_endian_store_le32(frame + 24, (uint32_t)rc);
  mulib_endian_store_le32(frame + 28, (uint32_t)capacity);
  return 0;
}
int mulib_vmm_trap(mulib_vmm_t *v, uint64_t address)
{
  int rc = mulib_vmm_enter_(v); if (rc) return rc;
  return mulib_vmm_leave_(v, mulib_vmm_trap_(v, address));
}
int mulib_vmm_exchange(void *context, const void *request, size_t size, void *response, size_t *capacity)
{
  mulib_vmm_t *v = (mulib_vmm_t *)context;
  size_t cap, written;
  int rc;
  if (!request || !response || !capacity || size < 40 || size > MULIB_VMMDEV_MAX_PACKET) return MULIB_VMMDEV_EINVAL;
  cap = *capacity;
  rc = mulib_vmm_enter_(v); if (rc) return rc;
  if (!mulib_vmm_live_(v)) return mulib_vmm_leave_(v, MULIB_VMMDEV_ESTATE);
  if (!mulib_vmm_host_private_(&v->ram, request, size) || !mulib_vmm_host_private_(&v->ram, response, cap)
      || mulib_vmm_overlap_((uintptr_t)request, size, (uintptr_t)response, cap)
      || mulib_vmm_overlap_((uintptr_t)request, size, (uintptr_t)v, sizeof *v)
      || mulib_vmm_overlap_((uintptr_t)response, cap, (uintptr_t)v, sizeof *v))
    return mulib_vmm_leave_(v, MULIB_VMMDEV_EINVAL);
  memcpy(v->request, request, size); written = cap > sizeof v->response ? sizeof v->response : cap;
  rc = mulib_vmm_dispatch_(v, size, &written);
  if (!rc) memcpy(response, v->response, written);
  if (!rc || rc == MULIB_VMMDEV_ENOSPC) *capacity = written;
  return mulib_vmm_leave_(v, rc);
}
int mulib_vmm_start(mulib_vmm_t *v)
{
  int rc = mulib_vmm_enter_(v); if (rc) return rc;
  rc = v->state != MULIB_VMM_READY ? MULIB_VMMDEV_ESTATE : (!v->executor.step ? MULIB_VMMDEV_ENOTSUP : 0);
  if (!rc) v->state = MULIB_VMM_RUNNING;
  return mulib_vmm_leave_(v, rc);
}
int mulib_vmm_stop(mulib_vmm_t *v)
{
  int rc = mulib_vmm_enter_(v); if (rc) return rc;
  rc = v->state == MULIB_VMM_RUNNING || v->state == MULIB_VMM_READY ? 0 : MULIB_VMMDEV_ESTATE;
  if (!rc) v->state = MULIB_VMM_READY;
  return mulib_vmm_leave_(v, rc);
}
int mulib_vmm_step(mulib_vmm_t *v, mulib_vmm_exit_t *out)
{
  mulib_vmm_exit_t e;
  int rc;
  if (!out) return MULIB_VMMDEV_EINVAL;
  rc = mulib_vmm_enter_(v); if (rc) return rc;
  if (v->state != MULIB_VMM_RUNNING || !v->executor.step) return mulib_vmm_leave_(v, MULIB_VMMDEV_ESTATE);
  memset(&e, 0, sizeof e); rc = v->executor.step(v->executor.context, &e);
  if (rc < 0 || rc > MULIB_VMMDEV_ENOSPC) rc = MULIB_VMMDEV_EIO;
  if (!rc) {
    if (e.kind == MULIB_VMM_EXIT_CALL) rc = mulib_vmm_trap_(v, e.address);
    else if (e.kind == MULIB_VMM_EXIT_HALT) v->state = MULIB_VMM_HALTED;
    else if (e.kind != MULIB_VMM_EXIT_YIELD) rc = MULIB_VMMDEV_EIO;
  }
  if (rc) v->state = MULIB_VMM_FAULTED;
  e.result = rc; *out = e; return mulib_vmm_leave_(v, rc);
}
static int mulib_vmm_reset_devices_(mulib_vmm_t *v)
{
  mulib_vmmdev_message_t m, r;
  int rc;
  /* Check epoch exhaustion for the entire inventory before resetting any device. */
  for (size_t i = 0; i < v->device_count; ++i)
    if (!v->slots[i].device->epoch || v->slots[i].device->epoch == UINT32_MAX) return MULIB_VMMDEV_ESTATE;
  for (size_t i = 0; i < v->device_count; ++i) {
    size_t n = sizeof v->request, cap = sizeof v->response;
    memset(&m, 0, sizeof m); m.opcode = MULIB_VMMDEV_RESET;
    m.device = v->slots[i].device->instance; m.epoch = v->slots[i].device->epoch;
    rc = mulib_vmmdev_encode(&m, v->request, &n); if (rc) return rc;
    rc = mulib_virtio_os_exchange(&v->slots[i].bus, v->request, n, v->response, &cap); if (rc) return rc;
    rc = mulib_vmmdev_decode(v->response, cap, &r); if (rc) return rc;
    if (r.argument) return (int)r.argument;
  }
  return 0;
}
int mulib_vmm_reset(mulib_vmm_t *v)
{
  int rc = mulib_vmm_enter_(v); if (rc) return rc;
  if (v->state == MULIB_VMM_CLOSED) return mulib_vmm_leave_(v, MULIB_VMMDEV_ESTATE);
  rc = mulib_vmm_reset_devices_(v);
  if (!rc && v->executor.reset) rc = v->executor.reset(v->executor.context);
  if (rc < 0 || rc > MULIB_VMMDEV_ENOSPC) rc = MULIB_VMMDEV_EIO;
  v->state = rc ? MULIB_VMM_FAULTED : MULIB_VMM_READY;
  return mulib_vmm_leave_(v, rc);
}
int mulib_vmm_close(mulib_vmm_t *v)
{
  int rc = mulib_vmm_enter_(v); if (rc) return rc;
  if (v->state == MULIB_VMM_CLOSED) return mulib_vmm_leave_(v, 0);
  for (size_t i = 0; i < v->device_count; ++i) {
    rc = mulib_virtio_os_shutdown(v->slots[i].device);
    if (rc) break;
  }
  v->state = rc ? MULIB_VMM_FAULTED : MULIB_VMM_CLOSED;
  if (!rc) {
    volatile unsigned char *a = v->request, *b = v->response;
    for (size_t i = 0; i < sizeof v->request; ++i) a[i] = b[i] = 0;
  }
  return mulib_vmm_leave_(v, rc);
}
int mulib_vmm_inventory(mulib_vmm_t *v, void *output, size_t *capacity)
{
  unsigned char *p = (unsigned char *)output;
  size_t need;
  int rc;
  if (!p || !capacity) return MULIB_VMMDEV_EINVAL;
  rc = mulib_vmm_enter_(v); if (rc) return rc;
  if (v->state == MULIB_VMM_CLOSED) return mulib_vmm_leave_(v, MULIB_VMMDEV_ESTATE);
  need = 16 + 8 * v->device_count;
  if (*capacity < need) { *capacity = need; return mulib_vmm_leave_(v, MULIB_VMMDEV_ENOSPC); }
  if (!mulib_vmm_host_private_(&v->ram, p, need)
      || mulib_vmm_overlap_((uintptr_t)p, need, (uintptr_t)v, sizeof *v)) return mulib_vmm_leave_(v, MULIB_VMMDEV_EINVAL);
  memcpy(p, "MVID", 4); mulib_vmmdev_store16(p + 4, 1); mulib_vmmdev_store16(p + 6, 0);
  mulib_endian_store_le32(p + 8, (uint32_t)v->device_count); mulib_endian_store_le32(p + 12, (uint32_t)need);
  for (size_t i = 0; i < v->device_count; ++i) {
    mulib_endian_store_le32(p + 16 + i * 8, v->slots[i].device->instance);
    mulib_endian_store_le32(p + 20 + i * 8, v->slots[i].device->profile->id);
  }
  *capacity = need; return mulib_vmm_leave_(v, 0);
}
int mulib_vmm_load(mulib_vmm_t *v, uint64_t address, const void *image, size_t size)
{
  int rc = mulib_vmm_enter_(v); if (rc) return rc;
  rc = v->state == MULIB_VMM_READY ? mulib_vmm_memory_write(&v->ram, address, image, size) : MULIB_VMMDEV_ESTATE;
  return mulib_vmm_leave_(v, rc);
}
int mulib_vmm_request_memory(mulib_vmm_t *v, uint32_t instance, uint64_t amount)
{
  int rc = mulib_vmm_enter_(v); if (rc) return rc;
  if (!mulib_vmm_live_(v)) return mulib_vmm_leave_(v, MULIB_VMMDEV_ESTATE);
  for (size_t i = 0; i < v->device_count; ++i)
    if (v->slots[i].device->instance == instance)
      return mulib_vmm_leave_(v, mulib_virtio_os_request_memory(v->slots[i].device, amount));
  return mulib_vmm_leave_(v, MULIB_VMMDEV_ENODEV);
}
#ifdef __cplusplus
}
#endif
#endif
