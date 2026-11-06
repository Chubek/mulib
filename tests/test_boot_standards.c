#define MULIB_IMPL_BOOTMGR
#define MULIB_IMPL_EFI
#define MULIB_IMPL_PE
#define MULIB_IMPL_COFF
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_BIOS
#define MULIB_IMPL_UEFI
#define MULIB_IMPL_UEFIUTILS
#include "mulib-bootmgr.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(mulib_uefi_guid_t) == 16, "EFI_GUID");
_Static_assert(sizeof(mulib_uefi_table_header_t) == 24, "EFI_TABLE_HEADER");
_Static_assert(offsetof(mulib_uefi_memory_t, physical) == 8, "EFI_MEMORY_DESCRIPTOR");
_Static_assert(sizeof(mulib_uefi_memory_t) == 40, "descriptor prefix");
_Static_assert(offsetof(mulib_uefiutils_shell_t, argc) == sizeof(void *), "Shell Argv before Argc");

static void put(uint8_t *p, uint64_t v, unsigned n) {
    unsigned i; for (i = 0; i < n; ++i) p[i] = (uint8_t)(v >> (8 * i));
}
/* Independently laid-out PE32+ fixture, one .text section and a certificate. */
static void image_fixture(uint8_t p[1040]) {
    memset(p, 0, 1040); p[0] = 'M'; p[1] = 'Z'; put(p + 60, 128, 4);
    memcpy(p + 128, "PE\0\0", 4); put(p + 132, 0x8664, 2); put(p + 134, 1, 2);
    put(p + 148, 240, 2); put(p + 150, 0x22, 2); put(p + 152, 0x20b, 2);
    put(p + 168, 4096, 4); put(p + 176, 0x100000, 8);
    put(p + 184, 4096, 4); put(p + 188, 512, 4); put(p + 208, 8192, 4);
    put(p + 212, 512, 4); put(p + 220, 10, 2); put(p + 260, 16, 4);
    put(p + 296, 1024, 4); put(p + 300, 16, 4);
    memcpy(p + 392, ".text", 5); put(p + 400, 1, 4); put(p + 404, 4096, 4);
    put(p + 408, 512, 4); put(p + 412, 512, 4); put(p + 428, 0x60000020, 4); p[512] = 0xc3;
    put(p + 1024, 9, 4); put(p + 1028, 0x200, 2); put(p + 1030, 2, 2); p[1032] = 0x42;
}
static unsigned map_calls, exit_calls, handle_calls, loads, starts, unloads, frees, deny, resident;
static unsigned variable_calls;
static mulib_uefiutils_loaded_image_t loaded;
static mulib_uefiutils_shell_t shell;
static uint16_t exit_message[] = {'x', 0};
static mulib_uefi_status_t MULIB_UEFI_API map_get(size_t *n, mulib_uefi_memory_t *p, size_t *key, size_t *stride, uint32_t *version) {
    ++map_calls; *stride = 48; *version = 1; *key = map_calls;
    if (*n < 48) { *n = 48; return MULIB_UEFI_BUFFER_TOO_SMALL; }
    memset(p, 0, 48); p->type = 7; p->pages = 1; *n = 48; return 0;
}
static mulib_uefi_status_t MULIB_UEFI_API exit_boot(void *image, size_t key) {
    assert(image && key == map_calls); ++exit_calls;
    return exit_calls < 3 ? MULIB_UEFI_INVALID_PARAMETER : 0;
}
static mulib_uefi_status_t MULIB_UEFI_API handle(void *image, mulib_uefi_guid_t *g, void **out) {
    assert(image); ++handle_calls;
    if (g->a == 0x752f3136) { *out = &shell; return 0; }
    assert(g->a == 0x5b1b31a1);
    if (!resident) return MULIB_UEFI_INVALID_PARAMETER;
    *out = &loaded; return 0;
}
static mulib_uefi_status_t MULIB_UEFI_API load_image(uint8_t policy, void *parent,
    mulib_uefi_device_path_t *path, void *source, size_t bytes, void **image) {
    (void)path; assert(policy == 1 && parent && source && bytes == 1040);
    ++loads; resident = 1; *image = &loaded; return deny ? MULIB_UEFI_SECURITY_VIOLATION : 0;
}
static mulib_uefi_status_t MULIB_UEFI_API start_image(void *image, size_t *bytes, uint16_t **data) {
    assert(image == &loaded && loaded.load_options_size == 2 && loaded.load_options);
    ++starts; resident = 0; *bytes = sizeof(exit_message); *data = exit_message; return 0;
}
static mulib_uefi_status_t MULIB_UEFI_API unload_image(void *image) {
    assert(image == &loaded && resident); resident = 0; ++unloads; return 0;
}
static mulib_uefi_status_t MULIB_UEFI_API free_pool(void *p) { assert(p == exit_message); ++frees; return 0; }
static mulib_uefi_status_t MULIB_UEFI_API get_variable(uint16_t *name, mulib_uefi_guid_t *g,
    uint32_t *attributes, size_t *bytes, void *data) {
    assert(name[0] == 'B' && g->a == 0x8be4df61); ++variable_calls;
    if (attributes) *attributes = 7;
    if (*bytes < 2) { *bytes = 2; return MULIB_UEFI_BUFFER_TOO_SMALL; }
    assert(data); ((uint8_t *)data)[0] = 42; ((uint8_t *)data)[1] = 0; *bytes = 2; return 0;
}
static mulib_uefi_status_t MULIB_UEFI_API set_variable(uint16_t *name, mulib_uefi_guid_t *g,
    uint32_t attributes, size_t bytes, void *data) {
    assert(name[0] == 'B' && g->a == 0x8be4df61 && !attributes && !bytes && !data);
    ++variable_calls; return MULIB_UEFI_ACCESS_DENIED;
}
static int chain(void *context, const void *sector, uint8_t drive) {
    assert(context == sector && drive == 0x80); return 17;
}
static int bios_e820(void *context, uint8_t interrupt, mulib_bios_regs_t *r) {
    uint8_t *p = (uint8_t *)context;
    assert(interrupt == 0x15 && r->eax == 0xe820 && r->edx == 0x534d4150 && r->ecx == 24);
    assert(r->es == 0x700 && r->edi == 0 && p[20] == 1);
    put(p, 0x100000, 8); put(p + 8, 0x200000, 8); put(p + 16, 1, 4);
    r->eax = 0x534d4150; r->ebx = 0; r->flags = 0; return 0;
}
static unsigned backend_calls, cleanup_calls;
static int backend_load(void *ctx, const mulib_bootmgr_entry_t *e, void **image) {
    (void)ctx; assert(e->kind == MULIB_BOOTMGR_BIOS); ++backend_calls; *image = (void *)e; return -7;
}
static int backend_start(void *ctx, void *image, const mulib_bootmgr_entry_t *e) {
    (void)ctx; (void)image; (void)e; assert(0); return 0;
}
static void backend_unload(void *ctx, void *image) { (void)ctx; assert(image); ++cleanup_calls; }
static void test_firmware(void) {
    _Alignas(8) uint8_t map[96]; uint8_t p[1040], sector[512] = {0}, dap[16], scratch[24];
    mulib_uefi_native_t n = {0}; mulib_uefi_t fw; mulib_uefi_memory_t memory;
    mulib_uefiutils_shell_t *sp; size_t bytes = 1, stride = 0, chosen; uint32_t token = 0;
    const mulib_uefi_guid_t global = MULIB_UEFI_GLOBAL_VARIABLE_GUID;
    uint16_t variable_name[] = {'B', 'o', 'o', 't', 'N', 'e', 'x', 't', 0};
    size_t variable_bytes = 0; uint32_t attributes;
    mulib_bios_t bios = {scratch, bios_e820}; mulib_bios_memory_t bm; mulib_bios_partition_t partition;
    mulib_bootmgr_entry_t entries[] = {{"efi", "a", "", 1, 1, 99}, {"bios", "b", "", 2, 1, 1}};
    mulib_bootmgr_backend_t backend = {NULL, backend_load, backend_start, backend_unload};
    mulib_bootmgr_platform_t platform = {MULIB_BOOTMGR_BIOS, NULL, &backend, NULL};
    n.image = &fw; n.get_memory_map = map_get; n.exit_boot_services = exit_boot;
    n.handle_protocol = handle; n.load_image = load_image; n.start_image = start_image;
    n.unload_image = unload_image; n.free_pool = free_pool;
    n.get_variable = get_variable; n.set_variable = set_variable;
    assert(!mulib_uefi_bind(&fw, &n));
    assert(!mulib_uefiutils_shell(&fw, &sp) && sp == &shell && handle_calls == 1);
    image_fixture(p);
    assert(!mulib_bootmgr_uefi_application(&fw, p, sizeof(p), NULL, p, 2));
    assert(loads == 1 && starts == 1 && unloads == 0 && frees == 1);
    deny = 1;
    assert(mulib_bootmgr_uefi_application(&fw, p, sizeof(p), NULL, p, 2) == MULIB_UEFI_SECURITY_VIOLATION);
    assert(starts == 1 && unloads == 1);
    assert(mulib_uefi_exit(&fw, map, &bytes, &stride, 1) == MULIB_UEFI_BUFFER_TOO_SMALL);
    assert(bytes == 48 && fw.phase == MULIB_UEFI_BOOT && exit_calls == 0);
    bytes = sizeof(map);
    assert(mulib_uefi_exit(&fw, map, &bytes, &stride, 1) == MULIB_UEFI_INVALID_PARAMETER);
    assert(fw.phase == MULIB_UEFI_EXIT_PENDING && exit_calls == 1);
    assert(mulib_uefi_get_variable(&fw, variable_name, &global, NULL, &variable_bytes, NULL) == MULIB_UEFI_INVALID_PARAMETER);
    assert(variable_calls == 0);
    assert(mulib_uefiutils_shell(&fw, &sp) == MULIB_UEFI_INVALID_PARAMETER && !sp);
    bytes = sizeof(map); assert(!mulib_uefi_exit(&fw, map, &bytes, &stride, 2));
    assert(fw.phase == MULIB_UEFI_RUNTIME && exit_calls == 3);
    assert(mulib_uefi_get_variable(&fw, variable_name, &global, &attributes, &variable_bytes, NULL) == MULIB_UEFI_BUFFER_TOO_SMALL);
    assert(variable_bytes == 2 && attributes == 7);
    assert(!mulib_uefi_get_variable(&fw, variable_name, &global, NULL, &variable_bytes, scratch) && scratch[0] == 42);
    assert(mulib_uefi_set_variable(&fw, variable_name, &global, 0, 0, NULL) == MULIB_UEFI_ACCESS_DENIED);
    assert(fw.last_status == MULIB_UEFI_ACCESS_DENIED && variable_calls == 3);
    assert(!mulib_uefi_memory_at(map, bytes, stride, 0, &memory) && memory.pages == 1);
    assert(!mulib_uefi_is_error(1) && mulib_uefi_is_error(MULIB_UEFI_LOAD_ERROR));
    map[8] = 1; assert(mulib_uefi_memory_at(map, bytes, stride, 0, &memory) == -1);
    assert(!mulib_bios_e820_next(&bios, &token, scratch, 0x7000, &bm));
    assert(!token && bm.base == 0x100000 && bm.length == 0x200000 && bm.attributes == 1);
    assert(!mulib_bios_dap(dap, sizeof(dap), UINT64_C(0x123456789), 2, 0x1000, 0, 512));
    assert(dap[0] == 16 && dap[2] == 2 && dap[8] == 0x89 && dap[12] == 1);
    assert(mulib_bios_dap(dap, 16, 0, 2, 0, 65535, 512) == MULIB_BIOS_INVALID);
    sector[510] = 0x55; sector[511] = 0xaa; sector[446] = 0x80; sector[450] = 0x83;
    put(sector + 454, 2048, 4); put(sector + 458, 4096, 4);
    assert(!mulib_bios_partition(sector, 512, 0, &partition) && partition.first_lba == 2048);
    assert(mulib_bootmgr_bios_chain(sector, sizeof(sector), 0x80, chain, sector) == 17);
    assert(mulib_bootmgr_boot_platform(entries, 2, SIZE_MAX, &platform, &chosen) == -7);
    assert(chosen == 1 && backend_calls == 1 && cleanup_calls == 1);
    assert(mulib_bootmgr_boot_platform(entries, 2, 0, &platform, &chosen) == -1 && chosen == SIZE_MAX);
}
static void test_formats(void) {
    uint8_t p[1040], path[64], option[128], table[24] = {0}; mulib_efi_image_t v;
    const uint8_t *payload; size_t i, cursor = 0, bytes, path_bytes, option_bytes;
    uint16_t revision, type, name[9], text[] = {'x', 0xd83d, 0xde00};
    const uint16_t filename[] = {'\\', 'a', '.', 'e', 'f', 'i'};
    mulib_uefiutils_option_t opt; char utf8[6];
    put(table, MULIB_UEFI_SYSTEM_SIGNATURE, 8); put(table + 8, 0x2000a, 4); put(table + 12, 24, 4);
    put(table + 16, 0x6f6424d3, 4); /* independently computed with Python zlib.crc32 */
    assert(!mulib_uefi_table_check(table, 24, MULIB_UEFI_SYSTEM_SIGNATURE, 24));
    table[8] ^= 1; assert(mulib_uefi_table_check(table, 24, MULIB_UEFI_SYSTEM_SIGNATURE, 24) == -1);
    image_fixture(p); assert(!mulib_efi_open(&v, p, sizeof(p)));
    put(p + 328, 4096, 4); /* GlobalPtr: nonzero RVA with zero size is legal. */
    assert(!mulib_efi_open(&v, p, sizeof(p)));
    put(p + 328, 0, 4);
    assert(!mulib_efi_certificate(&v, &cursor, &revision, &type, &payload, &bytes));
    assert(cursor == 16 && revision == 0x200 && type == 2 && bytes == 1 && payload[0] == 0x42);
    assert(mulib_efi_certificate(&v, &cursor, &revision, &type, &payload, &bytes) == 1);
    for (i = 0; i < sizeof(p); ++i) assert(mulib_efi_open(&v, p, i) < 0);
    p[1024] = 7; assert(mulib_efi_open(&v, p, sizeof(p)) < 0); p[1024] = 9;
    p[220] = 3; assert(mulib_efi_open(&v, p, sizeof(p)) < 0); p[220] = 10;
    put(p + 168, 4097, 4); assert(mulib_efi_open(&v, p, sizeof(p)) < 0);
    assert(!mulib_uefiutils_file_path(filename, 6, path, sizeof(path), &path_bytes));
    assert(!mulib_uefiutils_path_check(path, path_bytes));
    for (i = 0; i < path_bytes; ++i) assert(mulib_uefiutils_path_check(path, i) < 0);
    assert(!mulib_uefiutils_option_build(1, filename, 6, path, path_bytes, "\0x", 2, option, sizeof(option), &option_bytes));
    assert(!mulib_uefiutils_option_open(&opt, option, option_bytes));
    assert(opt.description_units == 6 && opt.path_bytes == path_bytes && opt.optional_bytes == 2);
    assert(!mulib_uefiutils_boot_name(0xabcd, name) && name[4] == 'A' && name[7] == 'D' && !name[8]);
    assert(!mulib_uefiutils_utf8(text, 3, utf8, sizeof(utf8), &bytes) && bytes == 6);
    assert(!memcmp(utf8, "x\xf0\x9f\x98\x80", 6));
    text[2] = 'x'; assert(mulib_uefiutils_utf8(text, 3, utf8, sizeof(utf8), &bytes) == MULIB_UEFIUTILS_FORMAT);
}
static void hostile(void) {
    uint8_t image[1040], copy[1040]; mulib_efi_image_t v; mulib_uefiutils_option_t option;
    mulib_bios_memory_t memory; uint32_t state = 1; size_t i, size;
    image_fixture(image);
    for (i = 0; i < 2048; ++i) {
        memcpy(copy, image, sizeof(copy));
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        copy[state % sizeof(copy)] ^= (uint8_t)(1 + (state >> 24));
        size = i % sizeof(copy);
        (void)mulib_efi_open(&v, copy, sizeof(copy));
        (void)mulib_efi_open(&v, copy, size);
        (void)mulib_uefiutils_path_check(copy, size);
        (void)mulib_uefiutils_option_open(&option, copy, size);
        (void)mulib_bios_e820(copy, size, &memory);
    }
}
int main(int argc, char **argv) {
    if (argc == 2) {
        uint8_t *data; long size; FILE *f = fopen(argv[1], "rb"); mulib_efi_image_t v; int rc;
        assert(f && !fseek(f, 0, SEEK_END)); size = ftell(f); assert(size > 0 && size <= 16 * 1024 * 1024);
        rewind(f); data = (uint8_t *)malloc((size_t)size); assert(data);
        assert(fread(data, 1, (size_t)size, f) == (size_t)size); fclose(f);
        rc = mulib_efi_open(&v, data, (size_t)size); free(data); assert(!rc); return 0;
    }
    test_formats(); test_firmware(); hostile(); return 0;
}
