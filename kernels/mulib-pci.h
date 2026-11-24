/** @file mulib-pci.h
 * @brief Conventional PCI configuration discovery with a supplied read backend.
 * Define MULIB_IMPL_PCI once. No writes, BAR probing or bus reconfiguration.
 */
#ifndef MULIB_PCI_H
#define MULIB_PCI_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*mulib_pci_read_t)(void *, unsigned bus, unsigned device, unsigned function, unsigned offset, uint32_t *);
typedef struct { unsigned bus, device, function; uint16_t vendor, product; uint8_t class_code, subclass, interface_code, revision, header; } mulib_pci_device_t;
typedef int (*mulib_pci_visit_t)(void *, const mulib_pci_device_t *);
/** @brief Scan inclusive bus range 0..255, respecting multifunction header bits.
 * Returns zero on completion, backend/visitor status on failure or early stop.
 * Backend returns 0xffffffff for absent functions. Enumeration never enables devices.
 */
int mulib_pci_scan(mulib_pci_read_t read, void *read_context, unsigned first_bus,
                   unsigned last_bus, mulib_pci_visit_t visit, void *visit_context);
/** @brief Compute segment-relative ECAM offset; UINT64_MAX on invalid BDF/offset. */
uint64_t mulib_pci_ecam(unsigned bus, unsigned device, unsigned function, unsigned offset);
/** @brief Locate conventional PCI capability, following a bounded cycle-checked
 * list. 0 found, 1 absent, -1 malformed; read errors propagate. offset is written
 * only on success. Supports header type 0/1, not CardBus or extended capabilities.
 */
int mulib_pci_capability(mulib_pci_read_t read, void *context, unsigned bus,
                         unsigned device, unsigned function, uint8_t id, unsigned *offset);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCI) && !defined(MULIB_PCI_IMPLEMENTED)
#define MULIB_PCI_IMPLEMENTED
int mulib_pci_capability(mulib_pci_read_t read, void *ctx, unsigned b,
                         unsigned d, unsigned f, uint8_t id, unsigned *offset) {
    uint32_t v; uint64_t seen = 0; unsigned at; int rc;
    if (!read || !offset || b > 255 || d > 31 || f > 7) return -1;
    rc = read(ctx, b, d, f, 0, &v); if (rc) return rc;
    if ((v & 65535) == 65535) return 1;
    rc = read(ctx, b, d, f, 4, &v); if (rc) return rc;
    if (!(v & UINT32_C(0x100000))) return 1;
    rc = read(ctx, b, d, f, 12, &v); if (rc) return rc;
    if (((v >> 16) & 127) > 1) return -1;
    rc = read(ctx, b, d, f, 0x34, &v); if (rc) return rc;
    at = v & 255;
    while (at) {
        if (at < 0x40 || (at & 3) || (seen & (UINT64_C(1) << (at / 4)))) return -1;
        seen |= UINT64_C(1) << (at / 4);
        rc = read(ctx, b, d, f, at, &v); if (rc) return rc;
        if ((v & 255) == id) { *offset = at; return 0; }
        at = (v >> 8) & 255;
    }
    return 1;
}
uint64_t mulib_pci_ecam(unsigned b, unsigned d, unsigned f, unsigned o) {
    if (b > 255 || d > 31 || f > 7 || o > 4092 || (o & 3)) return UINT64_MAX;
    return ((uint64_t)b << 20) | ((uint64_t)d << 15) | ((uint64_t)f << 12) | o;
}
int mulib_pci_scan(mulib_pci_read_t read, void *ctx, unsigned first, unsigned last, mulib_pci_visit_t visit, void *vctx) {
    unsigned b, d, f, count; uint32_t id, cls, hdr; int rc; mulib_pci_device_t dev;
    if (!read || !visit || first > last || last > 255) return -1;
    for (b = first; b <= last; ++b) for (d = 0; d < 32; ++d) {
        count = 1;
        for (f = 0; f < count; ++f) {
            rc = read(ctx, b, d, f, 0, &id); if (rc) return rc;
            if ((id & 65535) == 65535) continue;
            rc = read(ctx, b, d, f, 8, &cls); if (rc) return rc;
            rc = read(ctx, b, d, f, 12, &hdr); if (rc) return rc;
            if (!f && (hdr & UINT32_C(0x800000))) count = 8;
            dev.bus = b; dev.device = d; dev.function = f;
            dev.vendor = (uint16_t)id; dev.product = (uint16_t)(id >> 16);
            dev.class_code = (uint8_t)(cls >> 24); dev.subclass = (uint8_t)(cls >> 16);
            dev.interface_code = (uint8_t)(cls >> 8); dev.revision = (uint8_t)cls; dev.header = (uint8_t)(hdr >> 16);
            rc = visit(vctx, &dev); if (rc) return rc;
        }
    }
    return 0;
}
#endif
