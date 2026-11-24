/** @file mulib-uefiutils.h
 * @brief UEFI device paths, load options, Boot#### names and Shell parameters.
 * Define MULIB_IMPL_UEFIUTILS and MULIB_IMPL_UEFI once. Allocation-free.
 */
#ifndef MULIB_UEFIUTILS_H
#define MULIB_UEFIUTILS_H
#include "mulib-uefi.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { MULIB_UEFIUTILS_OK = 0, MULIB_UEFIUTILS_INVALID = -1,
       MULIB_UEFIUTILS_FORMAT = -2, MULIB_UEFIUTILS_BUFFER = -3 };
/** Wire views use byte pointers: UTF-16LE fields may be unaligned. */
typedef struct {
    uint32_t attributes;
    const uint8_t *description, *path, *optional;
    size_t description_units, path_bytes, optional_bytes;
} mulib_uefiutils_option_t;
/** Native EFI_SHELL_PARAMETERS_PROTOCOL layout; strings and handles borrowed. */
typedef struct {
    uint16_t **argv; size_t argc; void *stdin_handle, *stdout_handle, *stderr_handle;
} mulib_uefiutils_shell_t;
/** Native EFI_LOADED_IMAGE_PROTOCOL layout. load_options is binary and not
 * necessarily a string. image_size is UINT64, not UINTN. */
typedef struct {
    uint32_t revision; void *parent_handle, *system_table, *device_handle;
    mulib_uefi_device_path_t *file_path; void *reserved;
    uint32_t load_options_size; void *load_options, *image_base;
    uint64_t image_size; uint32_t image_code_type, image_data_type;
    mulib_uefi_unload_fn unload;
} mulib_uefiutils_loaded_image_t;
/** @brief Validate exactly one complete device path, including multi-instance
 * paths and final End Entire. Unknown node types preserved, not interpreted. */
int mulib_uefiutils_path_check(const void *path, size_t bytes);
/** @brief Build FilePath node + End Entire from nonempty native CHAR16 path.
 * units excludes NUL. NULL output sizes only; required always set for valid input.
 * Input/output must not overlap. */
int mulib_uefiutils_file_path(const uint16_t *path, size_t units, void *out, size_t capacity, size_t *required);
/** @brief Decode EFI_LOAD_OPTION, including terminated description, complete
 * file-path list and arbitrary optional bytes. Atomic borrowed output. */
int mulib_uefiutils_option_open(mulib_uefiutils_option_t *out, const void *data, size_t bytes);
/** @brief Serialize EFI_LOAD_OPTION. description excludes NUL; path may be empty.
 * NULL output sizes only. No writes on errors; inputs must not overlap output. */
int mulib_uefiutils_option_build(uint32_t attributes, const uint16_t *description, size_t units,
    const void *path, size_t path_bytes, const void *optional, size_t optional_bytes,
    void *out, size_t capacity, size_t *required);
/** @brief Write native CHAR16 Boot#### with uppercase hex and trailing NUL. */
int mulib_uefiutils_boot_name(uint16_t number, uint16_t out[9]);
/** @brief Decode a BootOrder/BootNext UINT16 from little-endian bytes. */
int mulib_uefiutils_boot_number(const void *data, size_t bytes, size_t index, uint16_t *out);
/** @brief Retrieve Shell parameters on the current image using HandleProtocol. */
mulib_uefi_status_t mulib_uefiutils_shell(mulib_uefi_t *fw, mulib_uefiutils_shell_t **out);
/** @brief Retrieve LoadedImage on a handle; output borrowed from firmware. */
mulib_uefi_status_t mulib_uefiutils_loaded_image(mulib_uefi_t *fw, void *image, mulib_uefiutils_loaded_image_t **out);
/** @brief Encode bounded native UTF-16 as UTF-8 plus NUL. units excludes NUL;
 * rejects embedded NUL and unpaired surrogates. NULL output sizes only. */
int mulib_uefiutils_utf8(const uint16_t *text, size_t units, char *out, size_t capacity, size_t *required);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UEFIUTILS) && !defined(MULIB_UEFIUTILS_IMPLEMENTED)
#define MULIB_UEFIUTILS_IMPLEMENTED
#include <string.h>
static unsigned mulib_uefiutils_u16_(const uint8_t *p) { return p[0] | (unsigned)p[1] << 8; }
static void mulib_uefiutils_put_(uint8_t *p, uint32_t v, unsigned n) {
    unsigned i; for (i = 0; i < n; ++i) p[i] = (uint8_t)(v >> (8 * i));
}
int mulib_uefiutils_path_check(const void *path, size_t bytes) {
    const uint8_t *p = (const uint8_t *)path; size_t at = 0, n, i;
    if (!p || bytes < 4) return MULIB_UEFIUTILS_INVALID;
    while (at < bytes) {
        if (bytes - at < 4) return MULIB_UEFIUTILS_FORMAT;
        n = mulib_uefiutils_u16_(p + at + 2);
        if (n < 4 || n > bytes - at) return MULIB_UEFIUTILS_FORMAT;
        if (p[at] == 0x7f) {
            if (n != 4 || (p[at + 1] != 1 && p[at + 1] != 0xff)) return MULIB_UEFIUTILS_FORMAT;
            if (p[at + 1] == 0xff) return at + 4 == bytes ? 0 : MULIB_UEFIUTILS_FORMAT;
        }
        if (p[at] == 4 && p[at + 1] == 4) {
            if (n < 6 || (n & 1) || mulib_uefiutils_u16_(p + at + n - 2)) return MULIB_UEFIUTILS_FORMAT;
            for (i = 4; i + 2 < n; i += 2) if (!mulib_uefiutils_u16_(p + at + i)) return MULIB_UEFIUTILS_FORMAT;
        }
        at += n;
    }
    return MULIB_UEFIUTILS_FORMAT;
}
int mulib_uefiutils_file_path(const uint16_t *path, size_t units, void *out, size_t capacity, size_t *required) {
    uint8_t *p = (uint8_t *)out; size_t i, n;
    if (!path || !units || !required || units > (65535u - 6) / 2) return MULIB_UEFIUTILS_INVALID;
    for (i = 0; i < units; ++i) if (!path[i]) return MULIB_UEFIUTILS_FORMAT;
    n = 6 + units * 2; *required = n + 4;
    if (!out) return 0;
    if (capacity < n + 4) return MULIB_UEFIUTILS_BUFFER;
    p[0] = 4; p[1] = 4; mulib_uefiutils_put_(p + 2, (uint32_t)n, 2);
    for (i = 0; i < units; ++i) mulib_uefiutils_put_(p + 4 + 2 * i, path[i], 2);
    p[n - 2] = p[n - 1] = 0; p[n] = 0x7f; p[n + 1] = 0xff; p[n + 2] = 4; p[n + 3] = 0;
    return 0;
}
/* FilePathList may contain multiple complete device paths. */
static int mulib_uefiutils_path_list_(const uint8_t *p, size_t bytes) {
    size_t at = 0, start = 0, n;
    while (at < bytes) {
        if (bytes - at < 4) return MULIB_UEFIUTILS_FORMAT;
        n = mulib_uefiutils_u16_(p + at + 2);
        if (n < 4 || n > bytes - at) return MULIB_UEFIUTILS_FORMAT;
        at += n;
        if (p[at - n] == 0x7f && p[at - n + 1] == 0xff) {
            if (mulib_uefiutils_path_check(p + start, at - start)) return MULIB_UEFIUTILS_FORMAT;
            start = at;
        }
    }
    return start == bytes ? 0 : MULIB_UEFIUTILS_FORMAT;
}
int mulib_uefiutils_option_open(mulib_uefiutils_option_t *out, const void *data, size_t bytes) {
    const uint8_t *p = (const uint8_t *)data; size_t at = 6, n; mulib_uefiutils_option_t v;
    if (!out || !p) return MULIB_UEFIUTILS_INVALID;
    if (bytes < 8) return MULIB_UEFIUTILS_FORMAT;
    v.attributes = (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
    n = mulib_uefiutils_u16_(p + 4); v.description = p + 6;
    while (bytes - at >= 2 && mulib_uefiutils_u16_(p + at)) at += 2;
    if (bytes - at < 2) return MULIB_UEFIUTILS_FORMAT;
    v.description_units = (at - 6) / 2; at += 2;
    if (n > bytes - at || mulib_uefiutils_path_list_(p + at, n)) return MULIB_UEFIUTILS_FORMAT;
    v.path = p + at; v.path_bytes = n; at += n;
    v.optional = p + at; v.optional_bytes = bytes - at; *out = v; return 0;
}
int mulib_uefiutils_option_build(uint32_t attributes, const uint16_t *description, size_t units,
    const void *path, size_t path_bytes, const void *optional, size_t optional_bytes,
    void *out, size_t capacity, size_t *required) {
    uint8_t *p = (uint8_t *)out; size_t i, n;
    if (!required || (!description && units) || (!path && path_bytes) || (!optional && optional_bytes) ||
        path_bytes > 65535 || units > (SIZE_MAX - 8) / 2) return MULIB_UEFIUTILS_INVALID;
    n = 8 + 2 * units;
    if (path_bytes > SIZE_MAX - n || optional_bytes > SIZE_MAX - n - path_bytes) return MULIB_UEFIUTILS_INVALID;
    for (i = 0; i < units; ++i) if (!description[i]) return MULIB_UEFIUTILS_FORMAT;
    if (mulib_uefiutils_path_list_((const uint8_t *)path, path_bytes)) return MULIB_UEFIUTILS_FORMAT;
    *required = n + path_bytes + optional_bytes;
    if (!p) return 0;
    if (capacity < *required) return MULIB_UEFIUTILS_BUFFER;
    mulib_uefiutils_put_(p, attributes, 4); mulib_uefiutils_put_(p + 4, (uint32_t)path_bytes, 2);
    for (i = 0; i < units; ++i) mulib_uefiutils_put_(p + 6 + 2 * i, description[i], 2);
    p[n - 2] = p[n - 1] = 0;
    if (path_bytes) memcpy(p + n, path, path_bytes);
    if (optional_bytes) memcpy(p + n + path_bytes, optional, optional_bytes);
    return 0;
}
int mulib_uefiutils_boot_name(uint16_t number, uint16_t out[9]) {
    static const char hex[] = "0123456789ABCDEF"; unsigned i;
    if (!out) return MULIB_UEFIUTILS_INVALID;
    out[0] = 'B'; out[1] = 'o'; out[2] = 'o'; out[3] = 't';
    for (i = 0; i < 4; ++i) out[4 + i] = (uint16_t)hex[(number >> (12 - 4 * i)) & 15];
    out[8] = 0; return 0;
}
int mulib_uefiutils_boot_number(const void *data, size_t bytes, size_t index, uint16_t *out) {
    if (!data || !out || (bytes & 1) || index >= bytes / 2) return MULIB_UEFIUTILS_INVALID;
    *out = (uint16_t)mulib_uefiutils_u16_((const uint8_t *)data + index * 2); return 0;
}
mulib_uefi_status_t mulib_uefiutils_shell(mulib_uefi_t *fw, mulib_uefiutils_shell_t **out) {
    const mulib_uefi_guid_t g = {0x752f3136, 0x4e16, 0x4fdc, {0xa2,0x2a,0xe5,0xf4,0x68,0x12,0xf4,0xca}};
    void *p = NULL; mulib_uefi_status_t s;
    if (!out) return MULIB_UEFI_INVALID_PARAMETER;
    *out = NULL;
    if (!fw || !fw->native) return MULIB_UEFI_INVALID_PARAMETER;
    s = mulib_uefi_handle(fw, fw->native->image, &g, &p);
    if (!mulib_uefi_is_error(s)) *out = (mulib_uefiutils_shell_t *)p;
    return s;
}
mulib_uefi_status_t mulib_uefiutils_loaded_image(mulib_uefi_t *fw, void *image, mulib_uefiutils_loaded_image_t **out) {
    const mulib_uefi_guid_t g = {0x5b1b31a1, 0x9562, 0x11d2, {0x8e,0x3f,0,0xa0,0xc9,0x69,0x72,0x3b}};
    void *p = NULL; mulib_uefi_status_t s;
    if (!out) return MULIB_UEFI_INVALID_PARAMETER;
    *out = NULL; s = mulib_uefi_handle(fw, image, &g, &p);
    if (!mulib_uefi_is_error(s)) *out = (mulib_uefiutils_loaded_image_t *)p;
    return s;
}
int mulib_uefiutils_utf8(const uint16_t *text, size_t units, char *out, size_t capacity, size_t *required) {
    size_t i, n = 1, at = 0; unsigned pass, k, count; uint32_t cp;
    if (!required || (!text && units)) return MULIB_UEFIUTILS_INVALID;
    for (pass = 0; pass < 2; ++pass) {
        for (i = 0; i < units; ++i) {
            cp = text[i];
            if (!cp || (cp >= 0xdc00 && cp <= 0xdfff)) return MULIB_UEFIUTILS_FORMAT;
            if (cp >= 0xd800 && cp <= 0xdbff) {
                if (++i == units || text[i] < 0xdc00 || text[i] > 0xdfff) return MULIB_UEFIUTILS_FORMAT;
                cp = 0x10000 + ((cp - 0xd800) << 10) + (text[i] - 0xdc00);
            }
            count = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
            if (!pass) { if (n > SIZE_MAX - count) return MULIB_UEFIUTILS_INVALID; n += count; }
            else {
                if (count == 1) out[at] = (char)cp;
                else {
                    out[at] = (char)((count == 2 ? 0xc0 : count == 3 ? 0xe0 : 0xf0) | (cp >> (6 * (count - 1))));
                    for (k = 1; k < count; ++k) out[at + k] = (char)(0x80 | ((cp >> (6 * (count - k - 1))) & 63));
                }
                at += count;
            }
        }
        if (!pass) { *required = n; if (!out) return 0; if (capacity < n) return MULIB_UEFIUTILS_BUFFER; }
    }
    out[at] = 0; return 0;
}
#endif
