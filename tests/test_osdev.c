#define MULIB_IMPL_EFI
#define MULIB_IMPL_BOOTMGR
#define MULIB_IMPL_HWINFO
#define MULIB_IMPL_MEMTEST
#define MULIB_IMPL_GPUTEST
#define MULIB_IMPL_DESCRITORS
#define MULIB_IMPL_IO
#define MULIB_IMPL_PAGING
#define MULIB_IMPL_PCI
#define MULIB_IMPL_ACPI
#define MULIB_IMPL_FRAMEBUFFER
#include "mulib-efi.h"
#include "mulib-bootmgr.h"
#include "mulib-hwinfo.h"
#include "mulib-memtest.h"
#include "mulib-gputest.h"
#include "mulib-descritors.h"
#include "mulib-io.h"
#include "mulib-paging.h"
#include "mulib-pci.h"
#include "mulib-acpi.h"
#include "mulib-framebuffer.h"
#include <assert.h>
#include <string.h>

static unsigned exits, maps, cleanups, launches, visits;
static int map_get(void *ctx, void *map, size_t *n, uint64_t *key, size_t *stride) {
    (void)ctx; (void)map; ++maps; *stride = 40; *key = maps;
    if (*n < 40) { *n = 40; return MULIB_EFI_BUFFER; } *n = 40; return 0;
}
static int fw_exit(void *ctx, uint64_t key) {
    (void)ctx; assert(key == maps); return ++exits == 1 ? MULIB_EFI_STALE : 0;
}
static int load(void *ctx, const mulib_bootmgr_entry_t *e, void **image) {
    (void)ctx; *image = (void *)e; return 0;
}
static int start(void *ctx, void *image, const mulib_bootmgr_entry_t *e) {
    (void)ctx; assert(image == e); return ++launches == 1 ? -9 : 0;
}
static void unload(void *ctx, void *image) { (void)ctx; assert(image); ++cleanups; }
static int corrupt(void *ctx, unsigned pass, unsigned pattern) {
    (void)pass; if (pattern == 0) ((uint64_t *)ctx)[1] ^= 1; return 0;
}
static int cancel(void *ctx, unsigned pass, unsigned pattern) {
    (void)ctx; (void)pass; (void)pattern; return 1;
}
static int submit(void *ctx, uint32_t seed, uint32_t work) {
    (void)ctx; assert(seed && work); return 0;
}
static int wait_gpu(void *ctx, uint32_t timeout) { (void)ctx; assert(timeout); return 0; }
static int temp_gpu(void *ctx, unsigned *temp) { *temp = *(unsigned *)ctx; return 0; }
static void cleanup_gpu(void *ctx) { (void)ctx; ++cleanups; }
static int bad_wait(void *ctx, uint32_t timeout) { (void)ctx; (void)timeout; return 7; }
static int bad_work(void *ctx, uint32_t seed, uint32_t count) {
    (void)ctx; (void)seed; (void)count; return 8;
}
static int stale_exit(void *ctx, uint64_t key) { (void)ctx; (void)key; ++exits; return MULIB_EFI_STALE; }
static int bios_thunk(void *ctx, uint8_t interrupt, mulib_efi_bios_regs_t *regs) {
    (void)ctx; assert(interrupt == 0x10); regs->eax = 0x004f; regs->flags = 0; return 0;
}
static int bad_load(void *ctx, const mulib_bootmgr_entry_t *e, void **image) {
    (void)ctx; (void)e; (void)image; return -8;
}
static int pci_stop(void *ctx, const mulib_pci_device_t *d) { (void)ctx; (void)d; return 42; }
static int cpuid_fake(void *ctx, uint32_t leaf, uint32_t sub, uint32_t r[4]) {
    (void)ctx; (void)sub; memset(r, 0, 16);
    if (!leaf) { r[0] = 1; r[1] = 0x756e6547; r[3] = 0x49656e69; r[2] = 0x6c65746e; }
    if (leaf == 1) r[0] = 0x000306a9;
    return 0;
}
static int pci_read(void *ctx, unsigned b, unsigned d, unsigned f, unsigned o, uint32_t *v) {
    (void)ctx; (void)b;
    *v = UINT32_MAX;
    if (d == 0 && f < 2) {
        if (o == 0) *v = 0x12348086;
        if (o == 8) *v = 0x03000001;
        if (o == 12) *v = 0x00800000;
    }
    return 0;
}
static int pci_visit(void *ctx, const mulib_pci_device_t *d) {
    (void)ctx; assert(d->vendor == 0x8086 && d->class_code == 3); ++visits; return 0;
}
static void checksum(uint8_t *p, size_t n, size_t at) {
    size_t i; uint8_t sum = 0; p[at] = 0;
    for (i = 0; i < n; ++i) sum = (uint8_t)(sum + p[i]);
    p[at] = (uint8_t)(0 - sum);
}
/* Exercise every truncated length and deterministic arbitrary firmware bytes. */
static void hostile_firmware(void) {
    uint8_t buffer[96]; uint32_t state = 1; unsigned iteration; size_t i, n, length;
    uint64_t root; unsigned width; mulib_efi_memory_t memory;
    for (iteration = 0; iteration < 256; ++iteration) {
        for (i = 0; i < sizeof(buffer); ++i) {
            state = state * UINT32_C(1664525) + UINT32_C(1013904223);
            buffer[i] = (uint8_t)(state >> 24);
        }
        for (n = 0; n <= sizeof(buffer); ++n) {
            (void)mulib_acpi_rsdp(buffer, n, &root, &width);
            (void)mulib_acpi_sdt(buffer, n, NULL, &length);
            (void)mulib_acpi_root_at(buffer, n, SIZE_MAX, &root);
            (void)mulib_efi_memory_at(buffer, n, 40, SIZE_MAX, &memory);
        }
    }
}
int main(void) {
    uint8_t map[80] = {0}, bytes[512] = {0}, desc[16], rsdp[36] = {0}, xsdt[44] = {0};
    uint64_t ram[8], address, entry; uint32_t pixels[12];
    size_t n = 1, stride = 0, selected, length; unsigned width, temperature = 50000;
    mulib_efi_t fw = {NULL, map_get, fw_exit, NULL, NULL, 0};
    mulib_efi_memory_t memory;
    mulib_efi_guid_t guid = {0, 0, 0, {0}};
    mulib_efi_bios_regs_t regs = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    void *protocol = &fw;
    mulib_bootmgr_entry_t entries[] = {{"first", "a", "", 1, 1, 10}, {"second", "b", "", 1, 1, 1}};
    mulib_bootmgr_backend_t boot = {NULL, load, start, unload};
    mulib_memtest_result_t mr;
    mulib_memtest_hooks_t hooks = {ram, corrupt, NULL};
    mulib_gputest_backend_t gpu = {&temperature, submit, wait_gpu, submit, temp_gpu, cleanup_gpu};
    mulib_gputest_config_t cfg = {3, 128, 100, 90000};
    mulib_gputest_result_t gr;
    mulib_hwinfo_cpu_t cpu;
    mulib_framebuffer_t fb = {pixels, 12, 3, 3, 4};
    assert(mulib_efi_guid_equal(&guid, &guid)); assert(!mulib_efi_guid_equal(NULL, &guid));
    assert(mulib_efi_exit(&fw, map, &n, &stride, 2) == MULIB_EFI_BUFFER && n == 40);
    n = sizeof(map); assert(!mulib_efi_exit(&fw, map, &n, &stride, 2));
    assert(exits == 2 && fw.exited); assert(mulib_efi_exit(&fw, map, &n, &stride, 2) == -1);
    assert(mulib_efi_bios(&fw, 0x10, &regs) == -1);
    fw.exited = 0;
    assert(mulib_efi_bios(&fw, 0x10, &regs) == MULIB_EFI_UNSUPPORTED);
    fw.bios_interrupt = bios_thunk; assert(!mulib_efi_bios(&fw, 0x10, &regs) && regs.eax == 0x4f);
    assert(mulib_efi_locate(&fw, &guid, &protocol) == MULIB_EFI_UNSUPPORTED && !protocol);
    fw.exit_boot_services = stale_exit; n = sizeof(map);
    assert(mulib_efi_exit(&fw, map, &n, &stride, 1) == MULIB_EFI_STALE && exits == 3 && !fw.exited);
    map[0] = 7; map[24] = 4;
    assert(!mulib_efi_memory_at(map, 40, 40, 0, &memory) && memory.type == 7 && memory.pages == 4);
    assert(mulib_efi_memory_at(map, 39, 40, SIZE_MAX, &memory) == -1);
    assert(!mulib_bootmgr_boot(entries, 2, SIZE_MAX, &boot, &selected) && selected == 1 && cleanups == 2);
    boot.load = bad_load;
    assert(mulib_bootmgr_boot(entries, 2, SIZE_MAX, &boot, &selected) == -8 && cleanups == 2);
    entries[0].enabled = 0; entries[1].enabled = 0;
    assert(mulib_bootmgr_boot(entries, 2, SIZE_MAX, &boot, &selected) == -1 && selected == SIZE_MAX);
    assert(mulib_bootmgr_select(NULL, 0) == SIZE_MAX);
    bytes[510] = 0x55; bytes[511] = 0xaa; assert(mulib_bootmgr_sector_valid(bytes, 512));
    assert(!mulib_bootmgr_sector_valid(bytes, 511));
    assert(!mulib_hwinfo_cpu(cpuid_fake, NULL, &cpu));
    assert(!strcmp(cpu.vendor, "GenuineIntel") && cpu.family == 6 && cpu.model == 58 && cpu.stepping == 9);
    assert(!mulib_memtest_run(ram, 8, 1, NULL, &mr) && mr.reads == 536 && mr.writes == 536);
    assert(mulib_memtest_run(ram, 8, 1, &hooks, &mr) == 2 && mr.errors == 1 && mr.first_word == 1);
    hooks.checkpoint = cancel; assert(mulib_memtest_run(ram, 8, 1, &hooks, &mr) == 1);
    assert(mulib_memtest_run(NULL, 1, 1, NULL, &mr) == -1);
    assert(mulib_memtest_run(ram, SIZE_MAX, 1, NULL, &mr) == -1 && !mr.writes);
    assert(!mulib_memtest_run(NULL, 0, 1, NULL, &mr));
    assert(!mulib_gputest_run(&gpu, &cfg, &gr) && gr.completed == 3);
    assert(cleanups == 3);
    temperature = 90000; assert(mulib_gputest_run(&gpu, &cfg, &gr) == -3 && gr.completed == 0);
    temperature = 50000; gpu.wait = bad_wait;
    assert(mulib_gputest_run(&gpu, &cfg, &gr) == -2 && gr.backend_status == 7);
    gpu.wait = wait_gpu; gpu.verify = bad_work;
    assert(mulib_gputest_run(&gpu, &cfg, &gr) == -2 && gr.backend_status == 8);
    gpu.submit = bad_work;
    assert(mulib_gputest_run(&gpu, &cfg, &gr) == -2 && gr.backend_status == 8 && cleanups == 7);
    gpu.temperature = NULL; assert(mulib_gputest_run(&gpu, &cfg, &gr) == -1);
    assert(cleanups == 7);
    assert(!mulib_descritors_segment(desc, 0, 0xfffff, 0x9a, 10) && desc[5] == 0x9a && desc[6] == 0xaf);
    assert(mulib_descritors_segment(desc, 0, 0, 0x9a, 6) == -1);
    assert(!mulib_descritors_gate(desc, UINT64_C(0x123456789abcdef0), 8, 1, 3, 14));
    assert(desc[0] == 0xf0 && desc[5] == 0xee && desc[8] == 0x78 && desc[11] == 0x12 && !desc[12]);
    assert(!mulib_descritors_tss(desc, UINT64_C(0x123456789abcdef0), 103) && desc[8] == 0x78);
    assert(!mulib_descritors_pointer(desc, 0, 65536) && desc[0] == 255 && desc[1] == 255);
    assert(mulib_descritors_pointer(desc, 0, 65537) == -1);
    assert(mulib_io_in8(0x80, bytes) == -1 && mulib_io_out8(0x80, 0) == -1);
    assert(mulib_io_load_table(desc, 0) == -1);
    assert(mulib_paging_canonical(UINT64_C(0xffff800000000000), 48));
    assert(!mulib_paging_canonical(UINT64_C(0x0000800000000000), 48));
    assert(mulib_paging_index(0x12345000, 1) == 0x145);
    assert(!mulib_paging_entry(0x1000, 48, MULIB_PAGING_PRESENT | MULIB_PAGING_NX, &entry));
    assert(mulib_paging_entry(1, 48, 0, &entry) == -1);
    assert(mulib_paging_entry(UINT64_C(1) << 48, 48, 0, &entry) == -1);
    assert(!mulib_pci_scan(pci_read, NULL, 0, 0, pci_visit, NULL) && visits == 2);
    assert(mulib_pci_scan(pci_read, NULL, 0, 0, pci_stop, NULL) == 42);
    assert(mulib_pci_scan(pci_read, NULL, 1, 0, pci_stop, NULL) == -1);
    assert(mulib_pci_ecam(256, 0, 0, 0) == UINT64_MAX);
    assert(mulib_pci_ecam(1, 2, 3, 4) == 0x113004);
    memcpy(rsdp, "RSD PTR ", 8); rsdp[15] = 2; rsdp[20] = 36; rsdp[24] = 0x40;
    checksum(rsdp, 20, 8); checksum(rsdp, 36, 32);
    assert(!mulib_acpi_rsdp(rsdp, 36, &address, &width) && address == 0x40 && width == 8);
    assert(mulib_acpi_rsdp(rsdp, 35, &address, &width) == -1);
    memcpy(xsdt, "XSDT", 4); xsdt[4] = 44; xsdt[36] = 0x80; checksum(xsdt, 44, 9);
    assert(!mulib_acpi_sdt(xsdt, 44, "XSDT", &length) && length == 44);
    assert(!mulib_acpi_root_at(xsdt, 44, 0, &address) && address == 0x80);
    assert(mulib_acpi_root_at(xsdt, 44, 1, &address) == -1);
    xsdt[10] ^= 1; assert(mulib_acpi_sdt(xsdt, 44, NULL, &length) == -1);
    memset(pixels, 0, sizeof(pixels)); pixels[3] = 42;
    assert(!mulib_framebuffer_pattern(&fb, 1, 2, 1));
    assert(pixels[3] == 42 && !mulib_framebuffer_verify(&fb, 1, 2, 1));
    pixels[0] = 4; assert(mulib_framebuffer_verify(&fb, 1, 2, 1) == 1);
    fb.height = SIZE_MAX; assert(!mulib_framebuffer_valid(&fb));
    hostile_firmware();
    return 0;
}
