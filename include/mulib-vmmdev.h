/**
 * @file mulib-vmmdev.h
 * @brief Bounded, little-endian device messages and guest-memory access for MuLIB VMM.
 * Define MULIB_IMPL_VMMDEV and MULIB_IMPL_ENDIAN in one translation unit.
 * See MULIB_VMMPROTO.md for the normative version-1 contract. No allocations.
 */
#ifndef MULIB_VMMDEV_H
#define MULIB_VMMDEV_H
#include "mulib-endian.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_VMMDEV_VERSION 1u
#define MULIB_VMMDEV_HEADER_SIZE 40u
#define MULIB_VMMDEV_MAX_SPANS 64u
#define MULIB_VMMDEV_MAX_PACKET 4096u
#define MULIB_VMMDEV_RESPONSE 0x8000u
#define MULIB_VMMDEV_READ 1u
#define MULIB_VMMDEV_WRITE 2u
enum mulib_vmmdev_result {
  MULIB_VMMDEV_OK, MULIB_VMMDEV_EINVAL, MULIB_VMMDEV_EVERSION,
  MULIB_VMMDEV_ENODEV, MULIB_VMMDEV_ESTALE, MULIB_VMMDEV_ESTATE,
  MULIB_VMMDEV_ENOTSUP, MULIB_VMMDEV_EBOUNDS, MULIB_VMMDEV_EACCESS,
  MULIB_VMMDEV_EIO, MULIB_VMMDEV_AGAIN, MULIB_VMMDEV_ENOSPC
};
enum mulib_vmmdev_opcode {
  MULIB_VMMDEV_PROBE = 1, MULIB_VMMDEV_RESET, MULIB_VMMDEV_STATUS,
  MULIB_VMMDEV_FEATURES, MULIB_VMMDEV_CONFIG_READ,
  MULIB_VMMDEV_CONFIG_WRITE, MULIB_VMMDEV_SUBMIT
};
/** @brief Decoded header; payload is borrowed, never a native wire struct. */
typedef struct mulib_vmmdev_message {
  uint16_t opcode;
  uint32_t device, epoch;
  uint64_t token, argument;
  uint32_t count, length;
  const unsigned char *payload;
} mulib_vmmdev_message_t;
/** @brief Host-owned mapping; permissions are from the device's perspective. */
typedef struct mulib_vmmdev_region {
  uint64_t address;
  size_t size;
  unsigned char *data;
  unsigned permissions;
} mulib_vmmdev_region_t;
/** @brief Borrowed, immutable region inventory; validate before publishing it. */
typedef struct mulib_vmmdev_memory {
  const mulib_vmmdev_region_t *regions;
  size_t count;
} mulib_vmmdev_memory_t;
/** @brief Guest span, serialized as address:u64, length:u32, flags:u32. */
typedef struct mulib_vmmdev_span {
  uint64_t address;
  uint32_t length, flags;
} mulib_vmmdev_span_t;
/** @brief Validated chain; writable spans follow readable spans. Borrowed for one call. */
typedef struct mulib_vmmdev_chain {
  unsigned char *data[MULIB_VMMDEV_MAX_SPANS];
  uint32_t size[MULIB_VMMDEV_MAX_SPANS];
  unsigned count, readable;
  size_t input_size, output_size;
} mulib_vmmdev_chain_t;
/** @brief Synchronous exchange. capacity in, actual response size out. No retained pointers. */
typedef int (*mulib_vmmdev_exchange_fn)(void *context, const void *request,
                                      size_t size, void *response, size_t *capacity);
/** @brief Unaligned little-endian 16/64-bit helpers complement mulib-endian. */
uint16_t mulib_vmmdev_load16(const unsigned char *p);
uint64_t mulib_vmmdev_load64(const unsigned char *p);
/** @brief Store 16/64-bit wire values; p must have sufficient space. */
void mulib_vmmdev_store16(unsigned char *p, uint16_t value);
void mulib_vmmdev_store64(unsigned char *p, uint64_t value);
/** @brief Decode an exact message, rejecting trailing bytes and invalid versions. */
int mulib_vmmdev_decode(const void *bytes, size_t size, mulib_vmmdev_message_t *out);
/** @brief Encode atomically on capacity failure. Payload and destination must not overlap. */
int mulib_vmmdev_encode(const mulib_vmmdev_message_t *message, void *bytes, size_t *capacity);
/** @brief Validate permissions, host/address overflow, and disjoint mappings. */
int mulib_vmmdev_memory_validate(const mulib_vmmdev_memory_t *memory);
/** @brief Resolve a nonempty span entirely within one permitted region. */
int mulib_vmmdev_map(const mulib_vmmdev_memory_t *memory, uint64_t address,
                     size_t size, unsigned permissions, unsigned char **out);
/** @brief Validate an entire serialized chain before exposing any guest memory. */
int mulib_vmmdev_chain_decode(const mulib_vmmdev_memory_t *memory,
                              const void *bytes, size_t size, unsigned count,
                              size_t limit, mulib_vmmdev_chain_t *out);
/** @brief Copy a range of the concatenated readable or writable stream. */
int mulib_vmmdev_chain_read(const mulib_vmmdev_chain_t *chain, size_t offset,
                            void *out, size_t size);
/** @brief Copy to the writable stream. Validates the whole range before writing. */
int mulib_vmmdev_chain_write(const mulib_vmmdev_chain_t *chain, size_t offset,
                             const void *bytes, size_t size);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VMMDEV) && !defined(MULIB_VMMDEV_IMPLEMENTED)
#define MULIB_VMMDEV_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
uint16_t mulib_vmmdev_load16(const unsigned char *p)
{ return (uint16_t)((uint16_t)p[0] | (uint16_t)p[1] << 8); }
uint64_t mulib_vmmdev_load64(const unsigned char *p)
{ return mulib_endian_load_le32(p) | (uint64_t)mulib_endian_load_le32(p + 4) << 32; }
void mulib_vmmdev_store16(unsigned char *p, uint16_t v)
{ p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
void mulib_vmmdev_store64(unsigned char *p, uint64_t v)
{ mulib_endian_store_le32(p, (uint32_t)v); mulib_endian_store_le32(p + 4, (uint32_t)(v >> 32)); }
int mulib_vmmdev_decode(const void *bytes, size_t n, mulib_vmmdev_message_t *out)
{
  const unsigned char *p = (const unsigned char *)bytes;
  mulib_vmmdev_message_t m;
  if (!p || !out || n < 40 || n > MULIB_VMMDEV_MAX_PACKET || memcmp(p, "MVMD", 4))
    return MULIB_VMMDEV_EINVAL;
  if (mulib_vmmdev_load16(p + 4) != MULIB_VMMDEV_VERSION) return MULIB_VMMDEV_EVERSION;
  m.opcode = mulib_vmmdev_load16(p + 6);
  m.device = mulib_endian_load_le32(p + 8); m.epoch = mulib_endian_load_le32(p + 12);
  m.token = mulib_vmmdev_load64(p + 16); m.argument = mulib_vmmdev_load64(p + 24);
  m.count = mulib_endian_load_le32(p + 32); m.length = mulib_endian_load_le32(p + 36);
  if (m.length != n - 40) return MULIB_VMMDEV_EINVAL;
  m.payload = p + 40; *out = m;
  return MULIB_VMMDEV_OK;
}
int mulib_vmmdev_encode(const mulib_vmmdev_message_t *m, void *bytes, size_t *capacity)
{
  unsigned char *p = (unsigned char *)bytes;
  size_t n;
  if (!m || !p || !capacity || (!m->payload && m->length)
      || m->length > MULIB_VMMDEV_MAX_PACKET - 40) return MULIB_VMMDEV_EINVAL;
  n = 40 + (size_t)m->length;
  if (*capacity < n) { *capacity = n; return MULIB_VMMDEV_ENOSPC; }
  memcpy(p, "MVMD", 4); mulib_vmmdev_store16(p + 4, MULIB_VMMDEV_VERSION);
  mulib_vmmdev_store16(p + 6, m->opcode);
  mulib_endian_store_le32(p + 8, m->device); mulib_endian_store_le32(p + 12, m->epoch);
  mulib_vmmdev_store64(p + 16, m->token); mulib_vmmdev_store64(p + 24, m->argument);
  mulib_endian_store_le32(p + 32, m->count); mulib_endian_store_le32(p + 36, m->length);
  if (m->length) memcpy(p + 40, m->payload, m->length);
  *capacity = n; return MULIB_VMMDEV_OK;
}
int mulib_vmmdev_memory_validate(const mulib_vmmdev_memory_t *m)
{
  if (!m || (!m->regions && m->count)) return MULIB_VMMDEV_EINVAL;
  for (size_t i = 0; i < m->count; ++i) {
    const mulib_vmmdev_region_t *a = m->regions + i;
    uintptr_t p = (uintptr_t)a->data;
    if (!a->data || !a->size || a->size > UINT64_MAX - a->address
        || a->size > UINTPTR_MAX - p || !a->permissions || (a->permissions & ~3u))
      return MULIB_VMMDEV_EINVAL;
    for (size_t j = 0; j < i; ++j) {
      const mulib_vmmdev_region_t *b = m->regions + j;
      uintptr_t q = (uintptr_t)b->data;
      if ((a->address < b->address + b->size && b->address < a->address + a->size)
          || (p < q + b->size && q < p + a->size)) return MULIB_VMMDEV_EINVAL;
    }
  }
  return MULIB_VMMDEV_OK;
}
int mulib_vmmdev_map(const mulib_vmmdev_memory_t *m, uint64_t addr,
                     size_t n, unsigned flags, unsigned char **out)
{
  if (!m || !out || (!m->regions && m->count) || !n || !flags || (flags & ~3u)
      || n > UINT64_MAX - addr) return MULIB_VMMDEV_EINVAL;
  for (size_t i = 0; i < m->count; ++i) {
    const mulib_vmmdev_region_t *r = m->regions + i;
    if (addr >= r->address && addr - r->address < r->size) {
      size_t off = (size_t)(addr - r->address);
      if (n > r->size - off) return MULIB_VMMDEV_EBOUNDS;
      if ((r->permissions & flags) != flags) return MULIB_VMMDEV_EACCESS;
      *out = r->data + off; return MULIB_VMMDEV_OK;
    }
  }
  return MULIB_VMMDEV_EBOUNDS;
}
int mulib_vmmdev_chain_decode(const mulib_vmmdev_memory_t *m, const void *bytes,
                              size_t n, unsigned count, size_t limit,
                              mulib_vmmdev_chain_t *out)
{
  const unsigned char *p = (const unsigned char *)bytes;
  mulib_vmmdev_chain_t c;
  uint64_t addresses[MULIB_VMMDEV_MAX_SPANS];
  int writing = 0, rc;
  if (!out || !p || !count || count > MULIB_VMMDEV_MAX_SPANS || n != 16u * count)
    return MULIB_VMMDEV_EINVAL;
  memset(&c, 0, sizeof c); c.count = count;
  for (unsigned i = 0; i < count; ++i) {
    uint64_t addr = mulib_vmmdev_load64(p + 16u * i);
    uint32_t len = mulib_endian_load_le32(p + 16u * i + 8);
    uint32_t f = mulib_endian_load_le32(p + 16u * i + 12);
    size_t *total;
    if (!len || f > 1 || (writing && !f)) return MULIB_VMMDEV_EINVAL;
    if (f) writing = 1; else ++c.readable;
    total = f ? &c.output_size : &c.input_size;
    if (*total > limit || len > limit - *total) return MULIB_VMMDEV_EBOUNDS;
    rc = mulib_vmmdev_map(m, addr, len, f ? MULIB_VMMDEV_WRITE : MULIB_VMMDEV_READ, &c.data[i]);
    if (rc) return rc;
    for (unsigned j = 0; j < i; ++j)
      if (addr < addresses[j] + c.size[j] && addresses[j] < addr + len)
        return MULIB_VMMDEV_EINVAL;
    addresses[i] = addr; c.size[i] = len; *total += len;
  }
  *out = c; return MULIB_VMMDEV_OK;
}
static int mulib_vmmdev_copy_(const mulib_vmmdev_chain_t *c, size_t off,
                              void *bytes, size_t n, int write)
{
  unsigned char *p = (unsigned char *)bytes;
  size_t total;
  unsigned begin, end;
  if (!c || (!bytes && n)) return MULIB_VMMDEV_EINVAL;
  total = write ? c->output_size : c->input_size;
  if (off > total || n > total - off) return MULIB_VMMDEV_EBOUNDS;
  begin = write ? c->readable : 0; end = write ? c->count : c->readable;
  for (unsigned i = begin; i < end && n; ++i) {
    size_t take;
    if (off >= c->size[i]) { off -= c->size[i]; continue; }
    take = c->size[i] - off; if (take > n) take = n;
    if (write) memcpy(c->data[i] + off, p, take); else memcpy(p, c->data[i] + off, take);
    p += take; n -= take; off = 0;
  }
  return n ? MULIB_VMMDEV_EINVAL : MULIB_VMMDEV_OK;
}
int mulib_vmmdev_chain_read(const mulib_vmmdev_chain_t *c, size_t off, void *p, size_t n)
{ return mulib_vmmdev_copy_(c, off, p, n, 0); }
int mulib_vmmdev_chain_write(const mulib_vmmdev_chain_t *c, size_t off, const void *p, size_t n)
{ return mulib_vmmdev_copy_(c, off, (void *)p, n, 1); }
#ifdef __cplusplus
}
#endif
#endif
