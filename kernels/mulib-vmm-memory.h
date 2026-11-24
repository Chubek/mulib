/**
 * @file mulib-vmm-memory.h
 * @brief Transactional guest RAM copies and per-device DMA view validation.
 * Define MULIB_IMPL_VMM_MEMORY, MULIB_IMPL_VMMDEV and MULIB_IMPL_ENDIAN once.
 * Inventories and RAM are borrowed and externally serialized. No allocation.
 */
#ifndef MULIB_VMM_MEMORY_H
#define MULIB_VMM_MEMORY_H
#include "mulib-vmmdev.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Check an entire GPA range, including adjacent regions, before access. */
int mulib_vmm_memory_check(const mulib_vmmdev_memory_t *memory, uint64_t address,
                           size_t size, unsigned permissions);
/** @brief Read validated RAM into disjoint host storage; unchanged on failure. */
int mulib_vmm_memory_read(const mulib_vmmdev_memory_t *memory, uint64_t address,
                          void *output, size_t size);
/** @brief Load disjoint host bytes into RAM; all destinations checked before copying. */
int mulib_vmm_memory_write(const mulib_vmmdev_memory_t *memory, uint64_t address,
                           const void *input, size_t size);
/** @brief Zero a complete writable range; gaps/permission failures have no side effects. */
int mulib_vmm_memory_zero(const mulib_vmmdev_memory_t *memory, uint64_t address, size_t size);
/** @brief Validate that a DMA view contains only permission-reduced subregions of RAM. */
int mulib_vmm_memory_subset(const mulib_vmmdev_memory_t *ram, const mulib_vmmdev_memory_t *view);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VMM_MEMORY) && !defined(MULIB_VMM_MEMORY_IMPLEMENTED)
#define MULIB_VMM_MEMORY_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static const mulib_vmmdev_region_t *mulib_vmm_memory_region_(const mulib_vmmdev_memory_t *m, uint64_t address)
{
  for (size_t i = 0; i < m->count; ++i)
    if (address >= m->regions[i].address && address - m->regions[i].address < m->regions[i].size)
      return m->regions + i;
  return NULL;
}
int mulib_vmm_memory_check(const mulib_vmmdev_memory_t *m, uint64_t address, size_t n, unsigned permissions)
{
  int rc;
  if (!permissions || (permissions & ~3u) || n > UINT64_MAX - address) return MULIB_VMMDEV_EINVAL;
  rc = mulib_vmmdev_memory_validate(m); if (rc) return rc;
  while (n) {
    const mulib_vmmdev_region_t *r = mulib_vmm_memory_region_(m, address);
    size_t take;
    if (!r) return MULIB_VMMDEV_EBOUNDS;
    if ((r->permissions & permissions) != permissions) return MULIB_VMMDEV_EACCESS;
    take = r->size - (size_t)(address - r->address); if (take > n) take = n;
    address += take; n -= take;
  }
  return 0;
}
static int mulib_vmm_memory_copy_(const mulib_vmmdev_memory_t *m, uint64_t address,
                                 void *buffer, size_t n, unsigned operation)
{
  unsigned char *p = (unsigned char *)buffer;
  int rc;
  if (operation != 2 && ((!buffer && n) || n > UINTPTR_MAX - (uintptr_t)buffer)) return MULIB_VMMDEV_EINVAL;
  rc = mulib_vmm_memory_check(m, address, n, operation ? MULIB_VMMDEV_WRITE : MULIB_VMMDEV_READ);
  if (rc) return rc;
  /* Reject host aliases before writing anything, including aliases to later regions. */
  if (operation != 2 && n) for (size_t i = 0; i < m->count; ++i) {
    uintptr_t a = (uintptr_t)buffer, b = (uintptr_t)m->regions[i].data;
    if (a < b + m->regions[i].size && b < a + n) return MULIB_VMMDEV_EINVAL;
  }
  while (n) {
    const mulib_vmmdev_region_t *r = mulib_vmm_memory_region_(m, address);
    size_t off = (size_t)(address - r->address), take = r->size - off;
    if (take > n) take = n;
    if (operation == 2) memset(r->data + off, 0, take);
    else if (operation) memcpy(r->data + off, p, take);
    else memcpy(p, r->data + off, take);
    address += take; n -= take; if (operation != 2) p += take;
  }
  return 0;
}
int mulib_vmm_memory_read(const mulib_vmmdev_memory_t *m, uint64_t address, void *p, size_t n)
{ return mulib_vmm_memory_copy_(m, address, p, n, 0); }
int mulib_vmm_memory_write(const mulib_vmmdev_memory_t *m, uint64_t address, const void *p, size_t n)
{ return mulib_vmm_memory_copy_(m, address, (void *)p, n, 1); }
int mulib_vmm_memory_zero(const mulib_vmmdev_memory_t *m, uint64_t address, size_t n)
{ return mulib_vmm_memory_copy_(m, address, NULL, n, 2); }
int mulib_vmm_memory_subset(const mulib_vmmdev_memory_t *ram, const mulib_vmmdev_memory_t *view)
{
  int rc = mulib_vmmdev_memory_validate(ram); if (rc) return rc;
  rc = mulib_vmmdev_memory_validate(view); if (rc) return rc;
  for (size_t i = 0; i < view->count; ++i) {
    const mulib_vmmdev_region_t *r = view->regions + i;
    unsigned char *p;
    rc = mulib_vmmdev_map(ram, r->address, r->size, r->permissions, &p);
    if (rc) return rc;
    if (p != r->data) return MULIB_VMMDEV_EACCESS;
  }
  return 0;
}
#ifdef __cplusplus
}
#endif
#endif
