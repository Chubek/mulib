/** @file mulib-bootmgr.h
 * @brief Bounded boot selection and explicit-backend image launch with fallback.
 * Define MULIB_IMPL_BOOTMGR once. Does not parse executable images or authenticate them.
 */
#ifndef MULIB_BOOTMGR_H
#define MULIB_BOOTMGR_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { MULIB_BOOTMGR_EFI = 1, MULIB_BOOTMGR_BIOS = 2, MULIB_BOOTMGR_KERNEL = 3 };
typedef struct {
    const char *label, *path, *arguments;
    unsigned kind, enabled, priority;
} mulib_bootmgr_entry_t;
typedef struct {
    void *context;
    int (*load)(void *, const mulib_bootmgr_entry_t *, void **);
    int (*start)(void *, void *, const mulib_bootmgr_entry_t *);
    void (*unload)(void *, void *);
} mulib_bootmgr_backend_t;
/** @brief Choose highest priority enabled entry; ties use original order. SIZE_MAX if none. */
size_t mulib_bootmgr_select(const mulib_bootmgr_entry_t *entries, size_t count);
/** @brief Launch preferred first, then remaining enabled entries in array order.
 * load success transfers a temporary handle; unload is always called if start returns.
 * start returning zero means success; nonzero errors trigger fallback. Outputs attempted index.
 */
int mulib_bootmgr_boot(const mulib_bootmgr_entry_t *entries, size_t count, size_t preferred,
                      const mulib_bootmgr_backend_t *backend, size_t *selected);
/** @brief Check only the legacy 512-byte boot sector signature, not bootability. */
int mulib_bootmgr_sector_valid(const void *sector, size_t bytes);
typedef struct { uint64_t physical_base, entry, image_bytes; size_t segments; } mulib_bootmgr_elf_t;
/** @brief Validate a static little-endian x86-64 ELF executable and load PT_LOAD
 * segments into a caller-owned contiguous physical window. No relocations.
 * NULL destination performs validation only. Entry must lie in executable data.
 * File and destination must not overlap. All segments are validated before writes.
 */
int mulib_bootmgr_elf_load(const void *file, size_t bytes, void *destination,
                         size_t capacity, uint64_t physical_base, mulib_bootmgr_elf_t *result);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BOOTMGR) && !defined(MULIB_BOOTMGR_IMPLEMENTED)
#define MULIB_BOOTMGR_IMPLEMENTED
static uint64_t mulib_bootmgr_le_(const uint8_t *p, unsigned n) {
    uint64_t v = 0; unsigned i; for (i = 0; i < n; ++i) v |= (uint64_t)p[i] << (8 * i); return v;
}
int mulib_bootmgr_elf_load(const void *file, size_t bytes, void *destination,
                         size_t capacity, uint64_t base, mulib_bootmgr_elf_t *result) {
    const uint8_t *f = (const uint8_t *)file, *p, *q; uint8_t *d = (uint8_t *)destination;
    uint64_t phoff, entry, off, pa, va, filesz, memsz, align, span = 0, previous, previous_size, previous_va;
    size_t i, j, count, stride, segments = 0; int executable = 0;
    if (!f || !result || bytes < 64 || f[0] != 127 || f[1] != 'E' || f[2] != 'L' || f[3] != 'F' ||
        f[4] != 2 || f[5] != 1 || f[6] != 1 || mulib_bootmgr_le_(f + 16, 2) != 2 ||
        mulib_bootmgr_le_(f + 18, 2) != 62 || mulib_bootmgr_le_(f + 20, 4) != 1 ||
        mulib_bootmgr_le_(f + 52, 2) != 64) return -1;
    entry = mulib_bootmgr_le_(f + 24, 8); phoff = mulib_bootmgr_le_(f + 32, 8);
    stride = (size_t)mulib_bootmgr_le_(f + 54, 2); count = (size_t)mulib_bootmgr_le_(f + 56, 2);
    if (stride != 56 || !count || count == 65535 || phoff > bytes || count > (bytes - (size_t)phoff) / stride) return -1;
    if (d) {
        uintptr_t a = (uintptr_t)f, b = (uintptr_t)d;
        if (bytes > UINTPTR_MAX - a || capacity > UINTPTR_MAX - b ||
            (a < b + capacity && b < a + bytes)) return -1;
    }
    for (i = 0; i < count; ++i) {
        p = f + (size_t)phoff + i * stride;
        /* Dynamic and interpreter headers imply unsupported loader work. */
        if (mulib_bootmgr_le_(p, 4) == 2 || mulib_bootmgr_le_(p, 4) == 3) return -1;
        if (mulib_bootmgr_le_(p, 4) != 1) continue;
        off = mulib_bootmgr_le_(p + 8, 8); va = mulib_bootmgr_le_(p + 16, 8);
        pa = mulib_bootmgr_le_(p + 24, 8); filesz = mulib_bootmgr_le_(p + 32, 8);
        memsz = mulib_bootmgr_le_(p + 40, 8); align = mulib_bootmgr_le_(p + 48, 8);
        if (filesz > memsz || off > bytes || filesz > bytes - off || pa < base ||
            memsz > UINT64_MAX - pa || memsz > UINT64_MAX - va ||
            pa - base > capacity || memsz > capacity - (pa - base) ||
            (align > 1 && ((align & (align - 1)) || (va & (align - 1)) != (off & (align - 1))))) return -1;
        for (j = 0; j < i; ++j) {
            q = f + (size_t)phoff + j * stride;
            if (mulib_bootmgr_le_(q, 4) != 1) continue;
            previous = mulib_bootmgr_le_(q + 24, 8); previous_size = mulib_bootmgr_le_(q + 40, 8);
            previous_va = mulib_bootmgr_le_(q + 16, 8);
            if (memsz && previous_size && pa < previous + previous_size && previous < pa + memsz) return -1;
            if (memsz && previous_size && va < previous_va + previous_size && previous_va < va + memsz) return -1;
        }
        if ((p[4] & 1) && entry >= va && entry - va < filesz) executable = 1;
        if (pa - base + memsz > span) span = pa - base + memsz;
        ++segments;
    }
    if (!segments || !executable) return -1;
    if (d) for (i = 0; i < count; ++i) {
        p = f + (size_t)phoff + i * stride;
        if (mulib_bootmgr_le_(p, 4) != 1) continue;
        off = mulib_bootmgr_le_(p + 8, 8); pa = mulib_bootmgr_le_(p + 24, 8) - base;
        filesz = mulib_bootmgr_le_(p + 32, 8); memsz = mulib_bootmgr_le_(p + 40, 8);
        for (j = 0; j < (size_t)filesz; ++j) d[(size_t)pa + j] = f[(size_t)off + j];
        for (; j < (size_t)memsz; ++j) d[(size_t)pa + j] = 0;
    }
    result->physical_base = base; result->entry = entry; result->image_bytes = span; result->segments = segments;
    return 0;
}
size_t mulib_bootmgr_select(const mulib_bootmgr_entry_t *e, size_t n) {
    size_t i, best = SIZE_MAX; if (!e) return best;
    for (i = 0; i < n; ++i) if (e[i].enabled && (best == SIZE_MAX || e[i].priority > e[best].priority)) best = i;
    return best;
}
int mulib_bootmgr_boot(const mulib_bootmgr_entry_t *e, size_t n, size_t preferred,
                      const mulib_bootmgr_backend_t *b, size_t *selected) {
    size_t step, i; int rc = -1; void *image;
    if (selected) *selected = SIZE_MAX;
    if (!e || !n || !b || !b->load || !b->start || !b->unload) return -1;
    if (preferred == SIZE_MAX) preferred = mulib_bootmgr_select(e, n);
    if (preferred >= n) return -1;
    for (step = 0; step < n; ++step) {
        i = step == 0 ? preferred : (step <= preferred ? step - 1 : step);
        if (!e[i].enabled) continue;
        if (!e[i].path || e[i].kind < 1 || e[i].kind > 3) { rc = -1; continue; }
        if (selected) *selected = i;
        image = NULL; rc = b->load(b->context, &e[i], &image); if (rc) continue;
        rc = b->start(b->context, image, &e[i]); b->unload(b->context, image);
        if (!rc) return 0;
    }
    return rc;
}
int mulib_bootmgr_sector_valid(const void *sector, size_t bytes) {
    const uint8_t *p = (const uint8_t *)sector;
    return p && bytes >= 512 && p[510] == 0x55 && p[511] == 0xaa;
}
#endif
