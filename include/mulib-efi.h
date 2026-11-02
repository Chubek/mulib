/** @file mulib-efi.h
 * @brief Allocation-free firmware adapters for BIOS thunks and EFI boot services.
 * Define MULIB_IMPL_EFI in one C translation unit. No firmware ABI is assumed:
 * adapters translate native EFIAPI calls or real-mode BIOS transitions.
 */
#ifndef MULIB_EFI_H
#define MULIB_EFI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint32_t a; uint16_t b, c; uint8_t d[8]; } mulib_efi_guid_t;
typedef struct { uint32_t type; uint64_t physical, virtual_address, pages, attributes; } mulib_efi_memory_t;
typedef struct { uint32_t eax, ebx, ecx, edx, esi, edi, ebp; uint16_t ds, es, flags; } mulib_efi_bios_regs_t;
/* Native x64 EFI uses the Microsoft ABI even in a SysV-built kernel. */
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
#define MULIB_EFI_API __attribute__((ms_abi))
#elif defined(_MSC_VER)
#define MULIB_EFI_API __cdecl
#else
#define MULIB_EFI_API
#endif
typedef uintptr_t mulib_efi_status_t;
typedef mulib_efi_status_t (MULIB_EFI_API *mulib_efi_get_map_fn)(size_t *, void *, size_t *, size_t *, uint32_t *);
typedef mulib_efi_status_t (MULIB_EFI_API *mulib_efi_exit_fn)(void *, size_t);
typedef mulib_efi_status_t (MULIB_EFI_API *mulib_efi_locate_fn)(const mulib_efi_guid_t *, void *, void **);
/** Native boot-service binding. Assign function pointers from your firmware SDK.
 * Context must outlive fw. last_status preserves the full EFI status value.
 */
typedef struct {
    void *image;
    mulib_efi_get_map_fn get_memory_map;
    mulib_efi_exit_fn exit_boot_services;
    mulib_efi_locate_fn locate_protocol;
    mulib_efi_status_t last_status;
    uint32_t descriptor_version;
} mulib_efi_native_t;
/** @brief Backend statuses: zero success, 1 buffer too small, 2 stale map key. */
enum { MULIB_EFI_OK = 0, MULIB_EFI_BUFFER = 1, MULIB_EFI_STALE = 2,
       MULIB_EFI_INVALID = -1, MULIB_EFI_UNSUPPORTED = -2 };
typedef struct {
    void *context;
    int (*memory_map)(void *, void *, size_t *, uint64_t *, size_t *);
    int (*exit_boot_services)(void *, uint64_t);
    int (*locate_protocol)(void *, const mulib_efi_guid_t *, void **);
    int (*bios_interrupt)(void *, uint8_t, mulib_efi_bios_regs_t *);
    unsigned exited;
} mulib_efi_t;
/** @brief Bind native EFI ABI calls directly; no handwritten trampoline needed. */
int mulib_efi_bind(mulib_efi_t *fw, mulib_efi_native_t *native);
/** @brief Compare GUIDs fieldwise, without padding assumptions. */
int mulib_efi_guid_equal(const mulib_efi_guid_t *a, const mulib_efi_guid_t *b);
/** @brief Decode a standard EFI memory descriptor at a supplied stride. */
int mulib_efi_memory_at(const void *map, size_t bytes, size_t stride, size_t index, mulib_efi_memory_t *out);
/** @brief Get map then exit, retrying stale keys without allocating between calls.
 * bytes is capacity on entry and required/used bytes on return. attempts is bounded.
 * All service calls through this context are refused after successful exit.
 */
int mulib_efi_exit(mulib_efi_t *fw, void *map, size_t *bytes, size_t *stride, unsigned attempts);
/** @brief Locate a protocol using the caller's native firmware adapter. */
int mulib_efi_locate(mulib_efi_t *fw, const mulib_efi_guid_t *guid, void **protocol);
/** @brief Execute a caller-supplied real-mode thunk; never issues int in long mode. */
int mulib_efi_bios(mulib_efi_t *fw, uint8_t interrupt, mulib_efi_bios_regs_t *regs);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EFI) && !defined(MULIB_EFI_IMPLEMENTED)
#define MULIB_EFI_IMPLEMENTED
static int mulib_efi_status_(mulib_efi_status_t s) {
    uintptr_t error = (uintptr_t)1 << (sizeof(uintptr_t) * 8 - 1);
    if (!(s & error)) return 0;
    if (s == (error | 5)) return MULIB_EFI_BUFFER;
    return -3;
}
static int mulib_efi_native_map_(void *ctx, void *map, size_t *bytes, uint64_t *key, size_t *stride) {
    mulib_efi_native_t *n = (mulib_efi_native_t *)ctx; size_t k = 0;
    n->last_status = n->get_memory_map(bytes, map, &k, stride, &n->descriptor_version);
    *key = k; return mulib_efi_status_(n->last_status);
}
static int mulib_efi_native_exit_(void *ctx, uint64_t key) {
    mulib_efi_native_t *n = (mulib_efi_native_t *)ctx;
    uintptr_t error = (uintptr_t)1 << (sizeof(uintptr_t) * 8 - 1);
    if (key > SIZE_MAX) return MULIB_EFI_INVALID;
    n->last_status = n->exit_boot_services(n->image, (size_t)key);
    return n->last_status == (error | 2) ? MULIB_EFI_STALE : mulib_efi_status_(n->last_status);
}
static int mulib_efi_native_locate_(void *ctx, const mulib_efi_guid_t *guid, void **protocol) {
    mulib_efi_native_t *n = (mulib_efi_native_t *)ctx;
    if (!n->locate_protocol) return MULIB_EFI_UNSUPPORTED;
    n->last_status = n->locate_protocol(guid, NULL, protocol);
    return mulib_efi_status_(n->last_status);
}
int mulib_efi_bind(mulib_efi_t *fw, mulib_efi_native_t *n) {
    if (!fw || !n || !n->image || !n->get_memory_map || !n->exit_boot_services) return MULIB_EFI_INVALID;
    fw->context = n; fw->memory_map = mulib_efi_native_map_;
    fw->exit_boot_services = mulib_efi_native_exit_; fw->locate_protocol = mulib_efi_native_locate_;
    fw->bios_interrupt = NULL; fw->exited = 0; n->last_status = 0; n->descriptor_version = 0;
    return 0;
}
static uint64_t mulib_efi_le_(const uint8_t *p, unsigned n) {
    uint64_t v = 0; unsigned i; for (i = 0; i < n; ++i) v |= (uint64_t)p[i] << (i * 8); return v;
}
int mulib_efi_guid_equal(const mulib_efi_guid_t *a, const mulib_efi_guid_t *b) {
    unsigned i; if (!a || !b || a->a != b->a || a->b != b->b || a->c != b->c) return 0;
    for (i = 0; i < 8; ++i) if (a->d[i] != b->d[i]) return 0;
    return 1;
}
int mulib_efi_memory_at(const void *map, size_t bytes, size_t stride, size_t index, mulib_efi_memory_t *out) {
    const uint8_t *p;
    if (!map || !out || stride < 40 || bytes % stride || index >= bytes / stride) return MULIB_EFI_INVALID;
    p = (const uint8_t *)map + index * stride;
    out->type = (uint32_t)mulib_efi_le_(p, 4); out->physical = mulib_efi_le_(p + 8, 8);
    out->virtual_address = mulib_efi_le_(p + 16, 8); out->pages = mulib_efi_le_(p + 24, 8);
    out->attributes = mulib_efi_le_(p + 32, 8); return 0;
}
int mulib_efi_exit(mulib_efi_t *fw, void *map, size_t *bytes, size_t *stride, unsigned attempts) {
    size_t capacity, n; uint64_t key; int rc;
    if (!fw || fw->exited || !map || !bytes || !stride || !attempts) return MULIB_EFI_INVALID;
    if (!fw->memory_map || !fw->exit_boot_services) return MULIB_EFI_UNSUPPORTED;
    capacity = *bytes;
    do {
        n = capacity; rc = fw->memory_map(fw->context, map, &n, &key, stride); *bytes = n;
        if (rc) return rc;
        if (n > capacity || *stride < 40 || n % *stride) return MULIB_EFI_INVALID;
        rc = fw->exit_boot_services(fw->context, key);
        if (!rc) { fw->exited = 1; return 0; }
    } while (rc == MULIB_EFI_STALE && --attempts);
    return rc;
}
int mulib_efi_locate(mulib_efi_t *fw, const mulib_efi_guid_t *guid, void **protocol) {
    if (!fw || fw->exited || !guid || !protocol) return MULIB_EFI_INVALID;
    *protocol = NULL;
    return fw->locate_protocol ? fw->locate_protocol(fw->context, guid, protocol) : MULIB_EFI_UNSUPPORTED;
}
int mulib_efi_bios(mulib_efi_t *fw, uint8_t interrupt, mulib_efi_bios_regs_t *regs) {
    if (!fw || fw->exited || !regs) return MULIB_EFI_INVALID;
    return fw->bios_interrupt ? fw->bios_interrupt(fw->context, interrupt, regs) : MULIB_EFI_UNSUPPORTED;
}
#endif
