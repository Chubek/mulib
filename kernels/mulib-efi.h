/** @file mulib-efi.h
 * @brief Checked PE/COFF EFI application and driver files (not firmware services).
 * Define MULIB_IMPL_EFI, PE, COFF and ENDIAN in one translation unit.
 * Views borrow immutable input. Validation is structural, not authentication.
 */
#ifndef MULIB_EFI_H
#define MULIB_EFI_H
#include "mulib-pe.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { MULIB_EFI_APPLICATION = 10, MULIB_EFI_BOOT_DRIVER = 11,
       MULIB_EFI_RUNTIME_DRIVER = 12 };
typedef mulib_pe_t mulib_efi_image_t;
/** @brief Open a PE32/PE32+ EFI image. PE result codes; atomic output.
 * Supports IA32, x64, ARM Thumb, AArch64, EBC, RISC-V 32/64 and LoongArch
 * 32/64 headers. Other machines return EUNSUPPORTED. Does not load code.
 */
int mulib_efi_open(mulib_efi_image_t *out, const void *file, size_t bytes);
/** @brief Borrow file-backed entry point byte; never returns zero-fill. */
int mulib_efi_entry(const mulib_efi_image_t *image, const unsigned char **out);
/** @brief Iterate WIN_CERTIFICATE records. cursor starts at zero, relative to
 * certificate directory. 0 record, 1 end, negative PE error. Payload is borrowed;
 * unknown certificate types/revisions are exposed without interpretation.
 */
int mulib_efi_certificate(const mulib_efi_image_t *image, size_t *cursor,
    uint16_t *revision, uint16_t *type, const unsigned char **payload, size_t *bytes);
/** @brief Standard removable-media filename for a supported machine, or NULL. */
const char *mulib_efi_boot_name(uint16_t machine);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EFI) && !defined(MULIB_EFI_IMPLEMENTED)
#define MULIB_EFI_IMPLEMENTED
const char *mulib_efi_boot_name(uint16_t machine) {
    switch (machine) {
    case 0x014c: return "BOOTIA32.EFI";
    case 0x8664: return "BOOTX64.EFI";
    case 0x01c2: return "BOOTARM.EFI";
    case 0xaa64: return "BOOTAA64.EFI";
    case 0x0ebc: return "BOOTEBC.EFI";
    case 0x5032: return "BOOTRISCV32.EFI";
    case 0x5064: return "BOOTRISCV64.EFI";
    case 0x6232: return "BOOTLOONGARCH32.EFI";
    case 0x6264: return "BOOTLOONGARCH64.EFI";
    default: return NULL;
    }
}
int mulib_efi_entry(const mulib_efi_image_t *v, const unsigned char **out) {
    if (!v || !out) return MULIB_PE_EINVAL;
    if (!v->entry) return MULIB_PE_EFORMAT;
    return mulib_pe_rva(v, v->entry, 1, out);
}
int mulib_efi_open(mulib_efi_image_t *out, const void *file, size_t bytes) {
    mulib_pe_t v; mulib_pe_section_t s, previous;
    mulib_pe_directory_t dir; const unsigned char *p;
    size_t i, j; uint64_t end; int entry = 0, rc; unsigned bits;
    if (!out) return MULIB_PE_EINVAL;
    rc = mulib_pe_open(&v, file, bytes); if (rc) return rc;
    if (!mulib_efi_boot_name(v.coff.machine)) return MULIB_PE_EUNSUPPORTED;
    bits = (v.coff.machine == 0x014c || v.coff.machine == 0x01c2 ||
            v.coff.machine == 0x5032 || v.coff.machine == 0x6232) ? 32 : 64;
    if (v.bits != bits || v.subsystem < 10 || v.subsystem > 12 ||
        !(v.coff.characteristics & 2) || !v.coff.section_count ||
        !v.entry || v.image_size % v.section_alignment ||
        v.headers_size % v.file_alignment ||
        (v.section_alignment < 4096 ? v.file_alignment != v.section_alignment :
         (v.file_alignment < 512 || v.file_alignment > 65536))) return MULIB_PE_EFORMAT;
    end = v.headers_size;
    for (i = 0; i < v.coff.section_count; ++i) {
        rc = mulib_pe_section(&v, i, &s); if (rc) return rc;
        if (s.address % v.section_alignment || s.address < end ||
            s.size % v.file_alignment || (s.size &&
            (s.offset < v.headers_size || s.offset % v.file_alignment ||
             (v.section_alignment < 4096 && s.offset != s.address))) ||
            s.relocation_count) return MULIB_PE_EFORMAT;
        end = (uint64_t)s.address + (s.virtual_size > s.size ? s.virtual_size : s.size);
        for (j = 0; s.size && j < i; ++j) {
            rc = mulib_pe_section(&v, j, &previous); if (rc) return rc;
            if (previous.size && (uint64_t)s.offset < (uint64_t)previous.offset + previous.size &&
                (uint64_t)previous.offset < (uint64_t)s.offset + s.size) return MULIB_PE_EFORMAT;
        }
        if ((s.flags & UINT32_C(0x20000000)) && v.entry >= s.address &&
            v.entry - s.address < s.size && (!s.virtual_size || v.entry - s.address < s.virtual_size)) entry = 1;
    }
    if (!entry || mulib_efi_entry(&v, &p)) return MULIB_PE_EFORMAT;
    for (i = 0; i < v.directory_count; ++i) {
        rc = mulib_pe_directory(&v, i, &dir); if (rc) return rc;
        if (!dir.size) continue;
        if (i == 4) {
            if ((dir.address & 7) || dir.address < v.headers_size) return MULIB_PE_EFORMAT;
            for (j = 0; j < v.coff.section_count; ++j) {
                rc = mulib_pe_section(&v, j, &s); if (rc) return rc;
                if (s.size && (uint64_t)dir.address < (uint64_t)s.offset + s.size &&
                    (uint64_t)s.offset < (uint64_t)dir.address + dir.size) return MULIB_PE_EFORMAT;
            }
            {
                size_t cursor = 0, length; uint16_t rev, type;
                do { rc = mulib_efi_certificate(&v, &cursor, &rev, &type, &p, &length); } while (!rc);
                if (rc != 1) return rc;
            }
        } else if (mulib_pe_rva(&v, dir.address, dir.size, &p)) return MULIB_PE_EBOUNDS;
    }
    *out = v; return 0;
}
int mulib_efi_certificate(const mulib_efi_image_t *v, size_t *cursor,
    uint16_t *revision, uint16_t *type, const unsigned char **payload, size_t *bytes) {
    mulib_pe_directory_t d; const unsigned char *p; uint32_t n; size_t padded; int rc;
    if (!v || !cursor || !revision || !type || !payload || !bytes) return MULIB_PE_EINVAL;
    if (v->directory_count <= 4) return *cursor ? MULIB_PE_EBOUNDS : 1;
    rc = mulib_pe_directory(v, 4, &d); if (rc) return rc;
    if (*cursor > d.size || (*cursor & 7) || (d.address & 7)) return MULIB_PE_EFORMAT;
    if (*cursor == d.size) return 1;
    if (d.size - *cursor < 8) return MULIB_PE_EBOUNDS;
    rc = mulib_pe_bytes(v, (uint64_t)d.address + *cursor, d.size - *cursor, &p); if (rc) return rc;
    n = mulib_endian_load_le32(p);
    if (n < 8 || n > d.size - *cursor || (uint64_t)n + 7 > SIZE_MAX) return MULIB_PE_EFORMAT;
    padded = ((size_t)n + 7) & ~(size_t)7;
    if (padded > d.size - *cursor) return MULIB_PE_EBOUNDS;
    *revision = (uint16_t)(p[4] | (uint16_t)p[5] << 8);
    *type = (uint16_t)(p[6] | (uint16_t)p[7] << 8);
    *payload = p + 8; *bytes = n - 8; *cursor += padded; return 0;
}
#endif
