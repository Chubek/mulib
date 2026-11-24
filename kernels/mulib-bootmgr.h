/** @file mulib-bootmgr.h
 * @brief Bounded boot selection and explicit-backend image launch with fallback.
 * Define MULIB_IMPL_BOOTMGR plus EFI, PE, COFF, ENDIAN, BIOS, UEFIUTILS and UEFI.
 * Firmware policy authenticates UEFI images; structural checks do not authenticate.
 */
#ifndef MULIB_BOOTMGR_H
#define MULIB_BOOTMGR_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-efi.h"
#include "mulib-bios.h"
#include "mulib-uefiutils.h"
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
/** Select eligible firmware entries; kernel backend is optional on either. */
typedef struct {
    unsigned firmware;
    const mulib_bootmgr_backend_t *uefi, *bios, *kernel;
} mulib_bootmgr_platform_t;
/** @brief Choose highest priority enabled entry; ties use original order. SIZE_MAX if none. */
size_t mulib_bootmgr_select(const mulib_bootmgr_entry_t *entries, size_t count);
/** @brief Launch preferred first, then remaining enabled entries in array order.
 * Any non-NULL load handle transfers ownership, even on failure. unload releases
 * it after failed load or returning start. A successful load must return a handle.
 * start returning zero means success; nonzero errors trigger fallback. Outputs attempted index.
 */
int mulib_bootmgr_boot(const mulib_bootmgr_entry_t *entries, size_t count, size_t preferred,
                      const mulib_bootmgr_backend_t *backend, size_t *selected);
/** @brief Boot only compatible entries. Auto choice is highest eligible priority;
 * preferred must be eligible; fallback retains original array order. */
int mulib_bootmgr_boot_platform(const mulib_bootmgr_entry_t *entries, size_t count, size_t preferred,
                              const mulib_bootmgr_platform_t *platform, size_t *selected);
/** @brief Load/start an in-memory EFI application through firmware. Binary load
 * options are borrowed until return. Optional path supplies firmware provenance.
 * Returns exact EFI status; denied images are never started. Requires
 * HandleProtocol, Load/Start/UnloadImage and FreePool services. */
mulib_uefi_status_t mulib_bootmgr_uefi_application(mulib_uefi_t *fw, void *file, size_t bytes,
    mulib_uefi_device_path_t *path, void *options, size_t option_bytes);
/** @brief Validate sector then invoke caller's real-mode chainloader. Handoff
 * must copy 512 bytes to physical 0x7c00, establish real mode and DL=drive. */
int mulib_bootmgr_bios_chain(const void *sector, size_t bytes, uint8_t drive,
    int (*handoff)(void *, const void *, uint8_t), void *context);
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
    size_t i, best = SIZE_MAX; if (!e || n > SIZE_MAX / sizeof(*e)) return best;
    for (i = 0; i < n; ++i) if (e[i].enabled && (best == SIZE_MAX || e[i].priority > e[best].priority)) best = i;
    return best;
}
int mulib_bootmgr_boot(const mulib_bootmgr_entry_t *e, size_t n, size_t preferred,
                      const mulib_bootmgr_backend_t *b, size_t *selected) {
    size_t step, i; int rc = -1; void *image;
    if (selected) *selected = SIZE_MAX;
    if (!e || !n || n > SIZE_MAX / sizeof(*e) || !b || !b->load || !b->start || !b->unload) return -1;
    if (preferred == SIZE_MAX) preferred = mulib_bootmgr_select(e, n);
    if (preferred >= n) return -1;
    for (step = 0; step < n; ++step) {
        i = step == 0 ? preferred : (step <= preferred ? step - 1 : step);
        if (!e[i].enabled) continue;
        if (!e[i].path || !*e[i].path || e[i].kind < 1 || e[i].kind > 3) { rc = -1; continue; }
        if (selected) *selected = i;
        image = NULL; rc = b->load(b->context, &e[i], &image);
        if (rc) { if (image) b->unload(b->context, image); continue; }
        if (!image) { rc = -1; continue; }
        rc = b->start(b->context, image, &e[i]); b->unload(b->context, image);
        if (!rc) return 0;
    }
    return rc;
}
int mulib_bootmgr_sector_valid(const void *sector, size_t bytes) {
    return mulib_bios_sector_valid(sector, bytes);
}
static const mulib_bootmgr_backend_t *mulib_bootmgr_route_(const mulib_bootmgr_platform_t *p, unsigned kind) {
    if (kind == MULIB_BOOTMGR_KERNEL) return p->kernel;
    if (kind != p->firmware) return NULL;
    return kind == MULIB_BOOTMGR_EFI ? p->uefi : p->bios;
}
int mulib_bootmgr_boot_platform(const mulib_bootmgr_entry_t *e, size_t n, size_t preferred,
                              const mulib_bootmgr_platform_t *p, size_t *selected) {
    size_t i, step; const mulib_bootmgr_backend_t *b; int rc = -1;
    if (selected) *selected = SIZE_MAX;
    if (!e || !n || n > SIZE_MAX / sizeof(*e) || !p || (p->firmware != MULIB_BOOTMGR_EFI && p->firmware != MULIB_BOOTMGR_BIOS)) return -1;
    if (preferred == SIZE_MAX) {
        for (i = 0; i < n; ++i) if (e[i].enabled && mulib_bootmgr_route_(p, e[i].kind) &&
            (preferred == SIZE_MAX || e[i].priority > e[preferred].priority)) preferred = i;
    }
    if (preferred >= n || !e[preferred].enabled || !mulib_bootmgr_route_(p, e[preferred].kind)) return -1;
    for (step = 0; step < n; ++step) {
        i = step == 0 ? preferred : (step <= preferred ? step - 1 : step);
        b = mulib_bootmgr_route_(p, e[i].kind);
        if (!e[i].enabled || !b) continue;
        if (selected) *selected = i;
        rc = mulib_bootmgr_boot(e + i, 1, 0, b, NULL); if (!rc) return 0;
    }
    return rc;
}
int mulib_bootmgr_bios_chain(const void *sector, size_t bytes, uint8_t drive,
    int (*handoff)(void *, const void *, uint8_t), void *context) {
    if (!handoff || !mulib_bios_sector_valid(sector, bytes)) return -1;
    return handoff(context, sector, drive);
}
mulib_uefi_status_t mulib_bootmgr_uefi_application(mulib_uefi_t *fw, void *file, size_t bytes,
    mulib_uefi_device_path_t *path, void *options, size_t option_bytes) {
    mulib_efi_image_t v; mulib_uefiutils_loaded_image_t *loaded;
    void *image = NULL; uint16_t *exit_data = NULL; size_t exit_bytes = 0;
    mulib_uefi_status_t s, cleanup;
    if (!fw || !fw->native || fw->phase != MULIB_UEFI_BOOT || (!options && option_bytes) || option_bytes > UINT32_MAX)
        return MULIB_UEFI_INVALID_PARAMETER;
    if (!fw->native->load_image || !fw->native->start_image || !fw->native->unload_image ||
        !fw->native->handle_protocol || !fw->native->free_pool) return MULIB_UEFI_UNSUPPORTED;
    if (mulib_efi_open(&v, file, bytes) || v.subsystem != MULIB_EFI_APPLICATION) return MULIB_UEFI_LOAD_ERROR;
    s = mulib_uefi_load(fw, 1, path, file, bytes, &image);
    if (s != 0) { if (image) (void)mulib_uefi_unload(fw, image); fw->last_status = s; return s; }
    if (!image) return MULIB_UEFI_LOAD_ERROR;
    s = mulib_uefiutils_loaded_image(fw, image, &loaded);
    if (s != 0 || !loaded) {
        (void)mulib_uefi_unload(fw, image); return s ? s : MULIB_UEFI_LOAD_ERROR;
    }
    loaded->load_options = options; loaded->load_options_size = (uint32_t)option_bytes;
    s = mulib_uefi_start(fw, image, &exit_bytes, &exit_data);
    /* A returning application is unloaded by firmware. A rejected StartImage
     * can leave it loaded: query the handle before attempting cleanup. */
    if (mulib_uefiutils_loaded_image(fw, image, &loaded) == 0 && loaded) {
        cleanup = mulib_uefi_unload(fw, image); if (!s) s = cleanup;
    }
    if (exit_data) { cleanup = mulib_uefi_free_pool(fw, exit_data); if (!s) s = cleanup; }
    fw->last_status = s; return s;
}
#endif
