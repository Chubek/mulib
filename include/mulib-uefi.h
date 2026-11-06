/** @file mulib-uefi.h
 * @brief UEFI native service bindings, status values and boot-services lifecycle.
 * Define MULIB_IMPL_UEFI once. No allocation or hosted OS calls. The binding
 * bundle is NOT an EFI_BOOT_SERVICES layout; assign services from your SDK.
 */
#ifndef MULIB_UEFI_H
#define MULIB_UEFI_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif
#ifndef MULIB_UEFI_API
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
#define MULIB_UEFI_API __attribute__((ms_abi))
#elif defined(__i386__) && (defined(__GNUC__) || defined(__clang__))
#define MULIB_UEFI_API __attribute__((cdecl))
#elif defined(_MSC_VER)
#define MULIB_UEFI_API __cdecl
#else
/* Other targets must be built with their platform's UEFI ABI. */
#define MULIB_UEFI_API
#endif
#endif
typedef uintptr_t mulib_uefi_status_t;
typedef uint16_t mulib_uefi_char16_t;
typedef struct { uint32_t a; uint16_t b, c; uint8_t d[8]; } mulib_uefi_guid_t;
typedef struct { uint32_t type, reserved; uint64_t physical, virtual_address, pages, attributes; } mulib_uefi_memory_t;
typedef struct { uint64_t signature; uint32_t revision, header_size, crc32, reserved; } mulib_uefi_table_header_t;
typedef struct { uint8_t type, subtype, length[2]; } mulib_uefi_device_path_t;
#define MULIB_UEFI_ERROR_BIT ((uintptr_t)1 << (sizeof(uintptr_t) * CHAR_BIT - 1))
#define MULIB_UEFI_ERROR(code) (MULIB_UEFI_ERROR_BIT | (uintptr_t)(code))
#define MULIB_UEFI_SUCCESS ((uintptr_t)0)
#define MULIB_UEFI_LOAD_ERROR MULIB_UEFI_ERROR(1)
#define MULIB_UEFI_INVALID_PARAMETER MULIB_UEFI_ERROR(2)
#define MULIB_UEFI_UNSUPPORTED MULIB_UEFI_ERROR(3)
#define MULIB_UEFI_BUFFER_TOO_SMALL MULIB_UEFI_ERROR(5)
#define MULIB_UEFI_DEVICE_ERROR MULIB_UEFI_ERROR(7)
#define MULIB_UEFI_OUT_OF_RESOURCES MULIB_UEFI_ERROR(9)
#define MULIB_UEFI_NOT_FOUND MULIB_UEFI_ERROR(14)
#define MULIB_UEFI_ACCESS_DENIED MULIB_UEFI_ERROR(15)
#define MULIB_UEFI_SECURITY_VIOLATION MULIB_UEFI_ERROR(26)
#define MULIB_UEFI_SYSTEM_SIGNATURE UINT64_C(0x5453595320494249)
#define MULIB_UEFI_BOOT_SIGNATURE UINT64_C(0x56524553544f4f42)
#define MULIB_UEFI_RUNTIME_SIGNATURE UINT64_C(0x56524553544e5552)
enum { MULIB_UEFI_BOOT = 0, MULIB_UEFI_EXIT_PENDING = 1, MULIB_UEFI_RUNTIME = 2 };
enum { MULIB_UEFI_RESERVED_MEMORY = 0, MULIB_UEFI_LOADER_CODE = 1,
       MULIB_UEFI_LOADER_DATA = 2, MULIB_UEFI_BOOT_CODE = 3, MULIB_UEFI_BOOT_DATA = 4,
       MULIB_UEFI_RUNTIME_CODE = 5, MULIB_UEFI_RUNTIME_DATA = 6,
       MULIB_UEFI_CONVENTIONAL_MEMORY = 7, MULIB_UEFI_UNUSABLE_MEMORY = 8,
       MULIB_UEFI_ACPI_RECLAIM = 9, MULIB_UEFI_ACPI_NVS = 10,
       MULIB_UEFI_MMIO = 11, MULIB_UEFI_MMIO_PORT = 12,
       MULIB_UEFI_PAL_CODE = 13, MULIB_UEFI_PERSISTENT_MEMORY = 14,
       MULIB_UEFI_UNACCEPTED_MEMORY = 15 };
#define MULIB_UEFI_MEMORY_RUNTIME UINT64_C(0x8000000000000000)
#define MULIB_UEFI_GLOBAL_VARIABLE_GUID {0x8be4df61,0x93ca,0x11d2,{0xaa,0x0d,0,0xe0,0x98,0x03,0x2b,0x8c}}
enum { MULIB_UEFI_VARIABLE_NON_VOLATILE = 1, MULIB_UEFI_VARIABLE_BOOTSERVICE_ACCESS = 2,
       MULIB_UEFI_VARIABLE_RUNTIME_ACCESS = 4, MULIB_UEFI_VARIABLE_APPEND_WRITE = 64 };
typedef mulib_uefi_status_t (MULIB_UEFI_API *mulib_uefi_get_map_fn)(size_t *, mulib_uefi_memory_t *, size_t *, size_t *, uint32_t *);
typedef mulib_uefi_status_t (MULIB_UEFI_API *mulib_uefi_exit_fn)(void *, size_t);
typedef mulib_uefi_status_t (MULIB_UEFI_API *mulib_uefi_locate_fn)(mulib_uefi_guid_t *, void *, void **);
typedef mulib_uefi_status_t (MULIB_UEFI_API *mulib_uefi_handle_fn)(void *, mulib_uefi_guid_t *, void **);
typedef mulib_uefi_status_t (MULIB_UEFI_API *mulib_uefi_load_fn)(uint8_t, void *, mulib_uefi_device_path_t *, void *, size_t, void **);
typedef mulib_uefi_status_t (MULIB_UEFI_API *mulib_uefi_start_fn)(void *, size_t *, uint16_t **);
typedef mulib_uefi_status_t (MULIB_UEFI_API *mulib_uefi_unload_fn)(void *);
typedef mulib_uefi_status_t (MULIB_UEFI_API *mulib_uefi_free_fn)(void *);
typedef mulib_uefi_status_t (MULIB_UEFI_API *mulib_uefi_get_variable_fn)(uint16_t *, mulib_uefi_guid_t *, uint32_t *, size_t *, void *);
typedef mulib_uefi_status_t (MULIB_UEFI_API *mulib_uefi_set_variable_fn)(uint16_t *, mulib_uefi_guid_t *, uint32_t, size_t, void *);
/** Borrowed function bundle; zero-initialize then assign supported services.
 * The SDK adapter must use compatible native types and calling convention.
 * Runtime pointers must be converted by the caller if virtual mapping changes. */
typedef struct {
    void *image;
    mulib_uefi_get_map_fn get_memory_map;
    mulib_uefi_exit_fn exit_boot_services;
    mulib_uefi_locate_fn locate_protocol;
    mulib_uefi_handle_fn handle_protocol;
    mulib_uefi_load_fn load_image;
    mulib_uefi_start_fn start_image;
    mulib_uefi_unload_fn unload_image;
    mulib_uefi_free_fn free_pool;
    mulib_uefi_get_variable_fn get_variable;
    mulib_uefi_set_variable_fn set_variable;
} mulib_uefi_native_t;
typedef struct {
    const mulib_uefi_native_t *native;
    mulib_uefi_status_t last_status;
    uint32_t descriptor_version;
    unsigned phase;
} mulib_uefi_t;
/** @brief True only for EFI errors; warnings are not errors. */
int mulib_uefi_is_error(mulib_uefi_status_t status);
/** @brief Bind caller-owned immutable service bundle; requires image handle. */
mulib_uefi_status_t mulib_uefi_bind(mulib_uefi_t *fw, const mulib_uefi_native_t *native);
/** @brief Compare GUID fields; NULL is unequal. */
int mulib_uefi_guid_equal(const mulib_uefi_guid_t *a, const mulib_uefi_guid_t *b);
/** @brief Validate little-endian table header signature, length and IEEE CRC32
 * (CRC field treated as zero). 0 valid, -1 invalid; bounded by supplied bytes. */
int mulib_uefi_table_check(const void *table, size_t bytes, uint64_t signature, size_t minimum);
/** @brief Decode descriptor version 1 prefix at firmware-provided stride.
 * Validates page alignment/range. 0 valid, -1 invalid; atomic output. */
int mulib_uefi_memory_at(const void *map, size_t bytes, size_t stride, size_t index, mulib_uefi_memory_t *out);
/** @brief Get map and ExitBootServices, retrying INVALID_PARAMETER keys only.
 * bytes is capacity/required size. Map must be 8-byte aligned. No allocations
 * or other services occur between calls. After the first exit attempt, only
 * this function may use boot services through fw, even if it fails. */
mulib_uefi_status_t mulib_uefi_exit(mulib_uefi_t *fw, void *map, size_t *bytes, size_t *stride, unsigned attempts);
/** @brief Locate protocol before any exit attempt; output cleared on failure. */
mulib_uefi_status_t mulib_uefi_locate(mulib_uefi_t *fw, const mulib_uefi_guid_t *guid, void **protocol);
/** @brief Get protocol installed on a specific handle (including Shell parameters). */
mulib_uefi_status_t mulib_uefi_handle(mulib_uefi_t *fw, void *handle, const mulib_uefi_guid_t *guid, void **protocol);
/** @brief Load through firmware policy. On SECURITY_VIOLATION a non-NULL handle
 * can be returned: caller owns it and must unload it without starting it. */
mulib_uefi_status_t mulib_uefi_load(mulib_uefi_t *fw, uint8_t boot_policy,
    mulib_uefi_device_path_t *path, void *source, size_t bytes, void **image);
/** @brief Start image. Exit data is firmware-allocated; caller frees with free_pool.
 * A returning EFI application is already unloaded by firmware. */
mulib_uefi_status_t mulib_uefi_start(mulib_uefi_t *fw, void *image, size_t *exit_bytes, uint16_t **exit_data);
/** @brief Unload an unstarted image or a returning resident driver. */
mulib_uefi_status_t mulib_uefi_unload(mulib_uefi_t *fw, void *image);
/** @brief Release firmware pool allocation before exit. */
mulib_uefi_status_t mulib_uefi_free_pool(mulib_uefi_t *fw, void *allocation);
/** @brief Get native CHAR16 variable. NULL data with zero size queries required
 * storage. Firmware owns variable policy. Allowed in boot and runtime phases. */
mulib_uefi_status_t mulib_uefi_get_variable(mulib_uefi_t *fw, uint16_t *name,
    const mulib_uefi_guid_t *guid, uint32_t *attributes, size_t *bytes, void *data);
/** @brief Set/delete variable using native UEFI attribute semantics. Zero bytes
 * may delete a variable; authentication payloads remain caller-owned. */
mulib_uefi_status_t mulib_uefi_set_variable(mulib_uefi_t *fw, uint16_t *name,
    const mulib_uefi_guid_t *guid, uint32_t attributes, size_t bytes, void *data);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UEFI) && !defined(MULIB_UEFI_IMPLEMENTED)
#define MULIB_UEFI_IMPLEMENTED
#include <string.h>
static uint64_t mulib_uefi_le_(const uint8_t *p, unsigned n) {
    uint64_t v = 0; unsigned i; for (i = 0; i < n; ++i) v |= (uint64_t)p[i] << (8 * i); return v;
}
int mulib_uefi_is_error(mulib_uefi_status_t s) { return (s & MULIB_UEFI_ERROR_BIT) != 0; }
static int mulib_uefi_active_(const mulib_uefi_t *fw) { return fw && fw->native && fw->phase == MULIB_UEFI_BOOT; }
mulib_uefi_status_t mulib_uefi_bind(mulib_uefi_t *fw, const mulib_uefi_native_t *n) {
    if (!fw || !n || !n->image) return MULIB_UEFI_INVALID_PARAMETER;
    fw->native = n; fw->last_status = 0; fw->descriptor_version = 0; fw->phase = MULIB_UEFI_BOOT; return 0;
}
int mulib_uefi_guid_equal(const mulib_uefi_guid_t *a, const mulib_uefi_guid_t *b) {
    return a && b && a->a == b->a && a->b == b->b && a->c == b->c && !memcmp(a->d, b->d, 8);
}
int mulib_uefi_table_check(const void *table, size_t bytes, uint64_t signature, size_t minimum) {
    const uint8_t *p = (const uint8_t *)table; size_t i, n; unsigned bit; uint32_t crc = UINT32_MAX;
    if (!p || bytes < 24 || minimum < 24 || mulib_uefi_le_(p, 8) != signature) return -1;
    n = (size_t)mulib_uefi_le_(p + 12, 4);
    if (n < minimum || n > bytes || mulib_uefi_le_(p + 20, 4)) return -1;
    for (i = 0; i < n; ++i) {
        crc ^= i >= 16 && i < 20 ? 0 : p[i];
        for (bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & (0u - (crc & 1)));
    }
    return (crc ^ UINT32_MAX) == mulib_uefi_le_(p + 16, 4) ? 0 : -1;
}
int mulib_uefi_memory_at(const void *map, size_t bytes, size_t stride, size_t index, mulib_uefi_memory_t *out) {
    const uint8_t *p; mulib_uefi_memory_t v;
    if (!map || !out || stride < 40 || bytes % stride || index >= bytes / stride) return -1;
    p = (const uint8_t *)map + index * stride;
    v.type = (uint32_t)mulib_uefi_le_(p, 4); v.reserved = 0;
    v.physical = mulib_uefi_le_(p + 8, 8); v.virtual_address = mulib_uefi_le_(p + 16, 8);
    v.pages = mulib_uefi_le_(p + 24, 8); v.attributes = mulib_uefi_le_(p + 32, 8);
    if ((v.physical & 4095) || (v.virtual_address & 4095) || !v.pages ||
        v.pages - 1 > (UINT64_MAX - v.physical) / 4096 ||
        v.pages - 1 > (UINT64_MAX - v.virtual_address) / 4096) return -1;
    *out = v; return 0;
}
mulib_uefi_status_t mulib_uefi_exit(mulib_uefi_t *fw, void *map, size_t *bytes, size_t *stride, unsigned attempts) {
    size_t capacity, n, key = 0; mulib_uefi_status_t s;
    if (!fw || !fw->native || fw->phase == MULIB_UEFI_RUNTIME || !map ||
        ((uintptr_t)map & 7) || !bytes || !stride || !attempts) return MULIB_UEFI_INVALID_PARAMETER;
    if (!fw->native->get_memory_map || !fw->native->exit_boot_services) return MULIB_UEFI_UNSUPPORTED;
    capacity = *bytes;
    do {
        n = capacity; *stride = 0;
        s = fw->native->get_memory_map(&n, (mulib_uefi_memory_t *)map, &key, stride, &fw->descriptor_version);
        *bytes = n; fw->last_status = s; if (s != 0) return s;
        if (n > capacity || !n || *stride < 40 || n % *stride || fw->descriptor_version != 1)
            return MULIB_UEFI_INVALID_PARAMETER;
        fw->phase = MULIB_UEFI_EXIT_PENDING;
        s = fw->native->exit_boot_services(fw->native->image, key); fw->last_status = s;
        if (!s) { fw->phase = MULIB_UEFI_RUNTIME; return 0; }
    } while (s == MULIB_UEFI_INVALID_PARAMETER && --attempts);
    return s;
}
mulib_uefi_status_t mulib_uefi_locate(mulib_uefi_t *fw, const mulib_uefi_guid_t *guid, void **protocol) {
    mulib_uefi_guid_t g;
    if (protocol) *protocol = NULL;
    if (!mulib_uefi_active_(fw) || !guid || !protocol) return MULIB_UEFI_INVALID_PARAMETER;
    if (!fw->native->locate_protocol) return MULIB_UEFI_UNSUPPORTED;
    g = *guid; fw->last_status = fw->native->locate_protocol(&g, NULL, protocol);
    if (mulib_uefi_is_error(fw->last_status)) *protocol = NULL;
    return fw->last_status;
}
mulib_uefi_status_t mulib_uefi_handle(mulib_uefi_t *fw, void *handle, const mulib_uefi_guid_t *guid, void **protocol) {
    mulib_uefi_guid_t g;
    if (protocol) *protocol = NULL;
    if (!mulib_uefi_active_(fw) || !handle || !guid || !protocol) return MULIB_UEFI_INVALID_PARAMETER;
    if (!fw->native->handle_protocol) return MULIB_UEFI_UNSUPPORTED;
    g = *guid; fw->last_status = fw->native->handle_protocol(handle, &g, protocol);
    if (mulib_uefi_is_error(fw->last_status)) *protocol = NULL;
    return fw->last_status;
}
mulib_uefi_status_t mulib_uefi_load(mulib_uefi_t *fw, uint8_t policy,
    mulib_uefi_device_path_t *path, void *source, size_t bytes, void **image) {
    if (image) *image = NULL;
    if (!mulib_uefi_active_(fw) || !image || policy > 1 || (!path && !source) || (source && !bytes))
        return MULIB_UEFI_INVALID_PARAMETER;
    if (!fw->native->load_image) return MULIB_UEFI_UNSUPPORTED;
    return fw->last_status = fw->native->load_image(policy, fw->native->image, path, source, bytes, image);
}
mulib_uefi_status_t mulib_uefi_start(mulib_uefi_t *fw, void *image, size_t *bytes, uint16_t **data) {
    if (data) *data = NULL;
    if (bytes) *bytes = 0;
    if (!mulib_uefi_active_(fw) || !image || !bytes) return MULIB_UEFI_INVALID_PARAMETER;
    if (!fw->native->start_image) return MULIB_UEFI_UNSUPPORTED;
    return fw->last_status = fw->native->start_image(image, bytes, data);
}
mulib_uefi_status_t mulib_uefi_unload(mulib_uefi_t *fw, void *image) {
    if (!mulib_uefi_active_(fw) || !image) return MULIB_UEFI_INVALID_PARAMETER;
    if (!fw->native->unload_image) return MULIB_UEFI_UNSUPPORTED;
    return fw->last_status = fw->native->unload_image(image);
}
mulib_uefi_status_t mulib_uefi_free_pool(mulib_uefi_t *fw, void *p) {
    if (!mulib_uefi_active_(fw) || !p) return MULIB_UEFI_INVALID_PARAMETER;
    if (!fw->native->free_pool) return MULIB_UEFI_UNSUPPORTED;
    return fw->last_status = fw->native->free_pool(p);
}
mulib_uefi_status_t mulib_uefi_get_variable(mulib_uefi_t *fw, uint16_t *name,
    const mulib_uefi_guid_t *guid, uint32_t *attributes, size_t *bytes, void *data) {
    mulib_uefi_guid_t g;
    if (!fw || !fw->native || fw->phase == MULIB_UEFI_EXIT_PENDING || !name || !*name ||
        !guid || !bytes || (!data && *bytes)) return MULIB_UEFI_INVALID_PARAMETER;
    if (!fw->native->get_variable) return MULIB_UEFI_UNSUPPORTED;
    g = *guid; return fw->last_status = fw->native->get_variable(name, &g, attributes, bytes, data);
}
mulib_uefi_status_t mulib_uefi_set_variable(mulib_uefi_t *fw, uint16_t *name,
    const mulib_uefi_guid_t *guid, uint32_t attributes, size_t bytes, void *data) {
    mulib_uefi_guid_t g;
    if (!fw || !fw->native || fw->phase == MULIB_UEFI_EXIT_PENDING || !name || !*name ||
        !guid || (!data && bytes)) return MULIB_UEFI_INVALID_PARAMETER;
    if (!fw->native->set_variable) return MULIB_UEFI_UNSUPPORTED;
    g = *guid; return fw->last_status = fw->native->set_variable(name, &g, attributes, bytes, data);
}
#endif
