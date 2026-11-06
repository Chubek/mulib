#define MULIB_IMPL_EFI
#define MULIB_IMPL_PE
#define MULIB_IMPL_COFF
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_BIOS
#define MULIB_IMPL_UEFI
#define MULIB_IMPL_UEFIUTILS
#define MULIB_IMPL_BOOTMGR
#define MULIB_IMPL_MEMTEST
#define MULIB_IMPL_DESCRITORS
#define MULIB_IMPL_GPUTEST
#define MULIB_IMPL_PAGING
#define MULIB_IMPL_PCI
#define MULIB_IMPL_ACPI
#define MULIB_IMPL_FRAMEBUFFER
#define MULIB_IMPL_HWINFO
#define MULIB_IMPL_IO
#include "mulib-efi.h"
#include "mulib-bootmgr.h"
#include "mulib-memtest.h"
#include "mulib-descritors.h"
#include "mulib-gputest.h"
#include "mulib-paging.h"
#include "mulib-pci.h"
#include "mulib-acpi.h"
#include "mulib-framebuffer.h"
#include "mulib-hwinfo.h"
#include "mulib-io.h"
#include <assert.h>
#include <string.h>

static void put(uint8_t *p, uint64_t v, unsigned bytes) {
    unsigned i; for (i = 0; i < bytes; ++i) p[i] = (uint8_t)(v >> (8 * i));
}
static mulib_uefi_status_t MULIB_UEFI_API get_map(size_t *n, mulib_uefi_memory_t *map, size_t *key, size_t *stride, uint32_t *version) {
    *stride = 40; *version = 1; *key = 17;
    if (*n < 40) { *n = 40; return ((uintptr_t)1 << (sizeof(uintptr_t) * 8 - 1)) | 5; }
    memset(map, 0, 40); *n = 40; return 0;
}
static mulib_uefi_status_t MULIB_UEFI_API exit_fw(void *image, size_t key) {
    assert(image && key == 17); return 0;
}
static int table_read(void *ctx, uint64_t table, unsigned index, uint64_t *entry) {
    uint64_t *tables = (uint64_t *)ctx;
    if (table < 0x1000 || table > 0x4000 || (table & 4095)) return -9;
    *entry = tables[(table / 4096 - 1) * 512 + index]; return 0;
}
static int cap_read(void *ctx, unsigned b, unsigned d, unsigned f, unsigned offset, uint32_t *v) {
    (void)b; (void)d; (void)f; *v = ((uint32_t *)ctx)[offset / 4]; return 0;
}
static int corrupt_march(void *ctx, unsigned pass, unsigned sweep) {
    (void)pass; if (sweep == 0) ((uint64_t *)ctx)[3] = 1; return 0;
}
static int cache_query(void *ctx, uint32_t leaf, uint32_t index, uint32_t r[4]) {
    (void)ctx; memset(r, 0, 16);
    if (!leaf) r[0] = 4;
    if (leaf == 4 && !index) { r[0] = 3 | (2 << 5); r[1] = 63 | (7u << 22); r[2] = 511; }
    return 0;
}
int main(void) {
    uint8_t elf[256] = {0}, target[32], saved[32], tss[104], madt[52] = {0};
    _Alignas(8) uint8_t map[40];
    uint64_t tables[2048] = {0}, ram[16], ist[7] = {0};
    uint32_t config[64] = {0}, pixel, gpu[3];
    mulib_bootmgr_elf_t image;
    mulib_uefi_native_t native = {0};
    mulib_uefi_t fw;
    mulib_memtest_result_t memory;
    mulib_memtest_hooks_t hooks = {ram, corrupt_march, NULL};
    mulib_paging_translation_t tr;
    mulib_hwinfo_cache_t cache;
    size_t bytes = 1, stride, cursor, record_size, i; unsigned offset, sum = 0;
    const uint8_t *record;
    native.image = target; native.get_memory_map = get_map; native.exit_boot_services = exit_fw;
    assert(!mulib_uefi_bind(&fw, &native));
    assert(mulib_uefi_exit(&fw, map, &bytes, &stride, 1) == MULIB_UEFI_BUFFER_TOO_SMALL && bytes == 40);
    assert(!mulib_uefi_exit(&fw, map, &bytes, &stride, 1) && fw.descriptor_version == 1);

    memcpy(elf, "\177ELF", 4); elf[4] = 2; elf[5] = 1; elf[6] = 1;
    put(elf + 16, 2, 2); put(elf + 18, 62, 2); put(elf + 20, 1, 4);
    put(elf + 24, 0x1000, 8); put(elf + 32, 64, 8); put(elf + 52, 64, 2);
    put(elf + 54, 56, 2); put(elf + 56, 1, 2);
    put(elf + 64, 1, 4); put(elf + 68, 5, 4); put(elf + 72, 128, 8);
    put(elf + 80, 0x1000, 8); put(elf + 88, 0x1000, 8);
    put(elf + 96, 4, 8); put(elf + 104, 16, 8); put(elf + 112, 1, 8);
    memcpy(elf + 128, "CODE", 4); memset(target, 0xa5, sizeof(target));
    assert(!mulib_bootmgr_elf_load(elf, sizeof(elf), target, sizeof(target), 0x1000, &image));
    assert(image.entry == 0x1000 && image.image_bytes == 16 && image.segments == 1);
    assert(!memcmp(target, "CODE", 4) && target[4] == 0 && target[15] == 0 && target[16] == 0xa5);
    memcpy(saved, target, sizeof(target));
    for (i = 0; i < 132; ++i) assert(mulib_bootmgr_elf_load(elf, i, target, 32, 0x1000, &image) == -1);
    assert(!memcmp(saved, target, sizeof(target)));
    put(elf + 104, UINT64_MAX, 8);
    assert(mulib_bootmgr_elf_load(elf, sizeof(elf), target, 32, 0x1000, &image) == -1);
    assert(!memcmp(saved, target, sizeof(target))); put(elf + 104, 16, 8);
    put(elf + 24, 0x1004, 8); /* Entry in BSS is not executable file data. */
    assert(mulib_bootmgr_elf_load(elf, sizeof(elf), target, 32, 0x1000, &image) == -1);
    put(elf + 24, 0x1000, 8);
    assert(mulib_bootmgr_elf_load(elf, sizeof(elf), elf, sizeof(elf), 0x1000, &image) == -1);

    assert(!mulib_memtest_march(ram, 16, 2, NULL, &memory));
    assert(memory.reads == 160 && memory.writes == 160);
    assert(mulib_memtest_march(ram, 16, 1, &hooks, &memory) == 2 && memory.first_word == 3 && memory.errors == 1);
    assert(mulib_memtest_march(ram, SIZE_MAX, 1, NULL, &memory) == -1);
    ist[0] = UINT64_C(0xffff800000008000);
    assert(!mulib_descritors_tss_init(tss, 0x1000, ist, 48));
    assert(tss[5] == 0x10 && tss[37] == 0x80 && tss[43] == 0xff && tss[102] == 104);
    assert(mulib_descritors_tss_init(tss, UINT64_C(0x800000000000), ist, 48) == -1);

    assert(mulib_gputest_reference(1, 0, 1) == UINT32_C(1753845952));
    for (i = 0; i < 3; ++i) gpu[i] = mulib_gputest_reference(7, (uint32_t)i, 8);
    assert(!mulib_gputest_verify(gpu, 3, 7, 8)); gpu[2] ^= 1;
    assert(mulib_gputest_verify(gpu, 3, 7, 8) == 1);
    assert(strstr(mulib_gputest_shader(), "local_size_x=64"));

    tables[0] = 0x2007; tables[512] = 0x3007; tables[1024] = 0x4003;
    tables[1537] = 0x9007 | MULIB_PAGING_NX;
    assert(!mulib_paging_translate(table_read, tables, 0x1000, 4, 48, 0x1234, &tr));
    assert(tr.physical == 0x9234 && tr.page_bytes == 4096 && !tr.user && !tr.executable && tr.writable);
    assert(mulib_paging_translate(table_read, tables, 0x1000, 4, 48, 0x2234, &tr) == 1);
    tables[1024] = 0x200087;
    assert(!mulib_paging_translate(table_read, tables, 0x1000, 4, 48, 0x1234, &tr));
    assert(tr.physical == 0x201234 && tr.page_bytes == 0x200000);
    tables[1024] |= 0x2000;
    assert(mulib_paging_translate(table_read, tables, 0x1000, 4, 48, 0x1234, &tr) == -1);

    config[0] = 0x12348086; config[1] = 0x100000; config[0x34 / 4] = 0x40;
    config[0x40 / 4] = 0x4801; config[0x48 / 4] = 5;
    assert(!mulib_pci_capability(cap_read, config, 0, 0, 0, 5, &offset) && offset == 0x48);
    config[0x48 / 4] = 0x4005;
    assert(mulib_pci_capability(cap_read, config, 0, 0, 0, 9, &offset) == -1);
    memcpy(madt, "APIC", 4); madt[4] = 52; madt[45] = 8;
    for (i = 0; i < sizeof(madt); ++i) sum += madt[i];
    madt[9] = (uint8_t)(0u - sum);
    cursor = 44;
    assert(!mulib_acpi_madt_next(madt, sizeof(madt), &cursor, &record, &record_size));
    assert(record == madt + 44 && record_size == 8 && cursor == 52);
    assert(mulib_acpi_madt_next(madt, sizeof(madt), &cursor, &record, &record_size) == 1);
    cursor = 45; assert(mulib_acpi_madt_next(madt, sizeof(madt), &cursor, &record, &record_size) == -1);
    assert(!mulib_framebuffer_rgb(255, 128, 0, 0xf800, 0x7e0, 0x1f, &pixel) && pixel == 0xfc00);
    assert(mulib_framebuffer_rgb(0, 0, 0, 5, 0x70, 0x700, &pixel) == -1);
    assert(!mulib_hwinfo_cache(cache_query, NULL, 4, 0, &cache));
    assert(cache.bytes == 262144 && cache.ways == 8 && cache.level == 2 && cache.line_bytes == 64);
    assert(mulib_hwinfo_cache(cache_query, NULL, 4, 1, &cache) == 1);
    assert(mulib_hwinfo_cache(cache_query, NULL, 5, 0, &cache) == -1);
    assert(mulib_io_in32(0xcfc, &pixel) == -1 && mulib_io_out32(0xcf8, 0) == -1);
    return 0;
}
