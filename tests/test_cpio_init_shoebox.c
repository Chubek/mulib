#define MULIB_IMPL_CPIO
#define MULIB_IMPL_INITSYSUTILS
#define MULIB_IMPL_SHOEBOX_CONTRACT
#include "mulib-initsysutils.h"
#include "mulib-shoebox-contract.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t archive_make(uint8_t *buffer, size_t capacity, unsigned format) {
    mulib_cpio_writer_t w; mulib_cpio_entry_t e = {0};
    assert(!mulib_cpio_writer_init(&w, buffer, capacity, format));
    e.name = "init"; e.name_bytes = 4; e.data = "#!/bin/sh\n"; e.size = 10;
    e.mode = MULIB_CPIO_REGULAR | 0755; e.nlink = 1; e.ino = 1;
    assert(!mulib_initsysutils_add(&w, &e));
    e.name = "dev"; e.name_bytes = 3; e.mode = MULIB_CPIO_DIRECTORY | 0755; e.size = 0; e.ino = 2;
    assert(!mulib_initsysutils_add(&w, &e));
    e.name = "dev/console"; e.name_bytes = 11; e.mode = MULIB_CPIO_CHAR | 0600;
    e.rdev_major = 5; e.rdev_minor = 1; e.ino = 3;
    assert(!mulib_initsysutils_add(&w, &e));
    assert(!mulib_cpio_finish(&w, 512));
    assert(mulib_cpio_finish(&w, 512) == MULIB_CPIO_FINISHED);
    return w.size;
}
static int log_steps[8], log_count;
static int apply(void *p) { int n = *(int *)p; log_steps[log_count++] = n; return n == 3 ? -9 : 0; }
static int undo(void *p) { int n = *(int *)p; log_steps[log_count++] = -n; return n == 2 ? -8 : 0; }
static void archives(void) {
    uint8_t buffer[4096], copy[4096], tiny[128]; size_t size, cursor, i, count, fail; int init, rc, undo_error;
    mulib_cpio_entry_t e, saved; mulib_cpio_writer_t w;
    mulib_initsysutils_cursor_t c = {0, 0};
    int ids[] = {1,2,3}; mulib_initsysutils_step_t steps[] = {
        {"one", ids, apply, undo}, {"two", ids + 1, apply, undo}, {"three", ids + 2, apply, undo}};
    size = archive_make(buffer, sizeof(buffer), MULIB_CPIO_CRC);
    assert(!mulib_initsysutils_validate(buffer, size, &count, &init) && count == 3 && init);
    cursor = 0; assert(!mulib_cpio_next(buffer, size, &cursor, &e)); saved = e;
    assert(e.name_bytes == 4 && !strcmp(e.name, "init") && e.checksum == mulib_cpio_sum(e.data, e.size));
    for (i = 0; i < cursor; ++i) {
        size_t at = 0; assert(mulib_cpio_next(buffer, i, &at, &e) < 0 && at == 0);
    }
    memcpy(copy, buffer, size); copy[(const uint8_t *)saved.data - buffer] ^= 1;
    i = 0; assert(mulib_cpio_next(copy, size, &i, &e) == MULIB_CPIO_CHECKSUM && i == 0);
    memcpy(copy, buffer, size); copy[6] = 'g'; i = 0;
    assert(mulib_cpio_next(copy, size, &i, &e) == MULIB_CPIO_FORMAT);
    memcpy(copy, buffer, size); memset(copy + 94, 'f', 8); i = 0;
    assert(mulib_cpio_next(copy, size, &i, &e) == MULIB_CPIO_BOUNDS);
    assert(mulib_cpio_next("070707", 6, &i, &e) == MULIB_CPIO_UNSUPPORTED);
    assert(!mulib_cpio_writer_init(&w, tiny, sizeof(tiny), MULIB_CPIO_NEWC));
    memset(tiny, 0xa5, sizeof(tiny)); e = saved; e.size = 100;
    assert(mulib_cpio_add(&w, &e) == MULIB_CPIO_BOUNDS && !w.size && tiny[0] == 0xa5);
    assert(mulib_cpio_finish(&w, 512) == MULIB_CPIO_BOUNDS && !w.size);
    assert(mulib_cpio_record_size(SIZE_MAX, 0, &i) == MULIB_CPIO_INVALID);
    memcpy(buffer + size, buffer, size);
    assert(!mulib_initsysutils_validate(buffer, size * 2, &count, &init) && count == 6);
    while ((rc = mulib_initsysutils_next(buffer, size * 2, &c, &e)) == 0) {}
    assert(rc == MULIB_INITSYSUTILS_END && c.archive == 2);
    assert(!mulib_initsysutils_validate(NULL, 0, &count, &init) && !count && !init);
    assert(!mulib_initsysutils_validate(buffer, cursor, &count, &init) && count == 1);
    assert(mulib_initsysutils_path("../escape", 9) == MULIB_INITSYSUTILS_PATH);
    assert(mulib_initsysutils_path("/init", 5) == MULIB_INITSYSUTILS_PATH);
    assert(!mulib_initsysutils_path("./init", 6));
    assert(mulib_initsysutils_run(steps, 3, &fail, &undo_error) == -9 && fail == 2 && undo_error == -8);
    assert(log_count == 5 && log_steps[3] == -2 && log_steps[4] == -1);
    /* Every single-byte mutation must remain bounded and keep failed cursors. */
    for (i = 0; i < size; ++i) {
        size_t at = 0; memcpy(copy, buffer, size); copy[i] ^= 0xff;
        rc = mulib_cpio_next(copy, size, &at, &e);
        if (rc < 0) assert(!at);
        (void)mulib_initsysutils_validate(copy, size, &count, &init);
    }
}
static char output[32]; static size_t output_size; static int io_mode;
static int write_partial(void *ctx, unsigned stream, const void *p, size_t n, size_t *done) {
    (void)ctx; assert(stream == MULIB_SHOEBOX_STDOUT);
    if (io_mode) { *done = 0; return 0; }
    *done = n > 2 ? 2 : n; memcpy(output + output_size, p, *done); output_size += *done; return 0;
}
/* Contract probe only: not a Shoebox utility. */
static int probe(const mulib_shoebox_context_t *c, int argc, const char *const *argv) {
    assert(argc == 2 && !strcmp(argv[1], "arg") && c); return 23;
}
static void contract(void) {
    const char *env[] = {"PATH=/bin", "A=1"};
    const char *args[] = {"/sbin/shoebox", "probe", "arg"};
    const char *direct[] = {"/bin/probe", "arg"};
    mulib_shoebox_context_t c = {0};
    mulib_shoebox_applet_t a[2] = {{1, sizeof(*a), "probe", "test", "", probe}, {1, sizeof(*a), "probe", "", "", probe}};
    int status; size_t n;
    c.abi_version = 1; c.struct_size = sizeof(c); c.io.write = write_partial; c.environment = env; c.env_count = 2;
    assert(!mulib_shoebox_dispatch(a, 1, "shoebox", &c, 3, args, &status) && status == 23);
    assert(!mulib_shoebox_dispatch(a, 1, "shoebox", &c, 2, direct, &status) && status == 23);
    assert(mulib_shoebox_validate(a, 2) == MULIB_SHOEBOX_DUPLICATE);
    assert(mulib_shoebox_dispatch(NULL, 0, "shoebox", &c, 2, direct, &status) == MULIB_SHOEBOX_NOT_FOUND && status == 127);
    assert(!strcmp(mulib_shoebox_getenv(&c, "PATH"), "/bin") && !mulib_shoebox_getenv(&c, "PAT"));
    assert(!mulib_shoebox_write_all(&c, 1, "abcde", 5, &n) && n == 5 && !memcmp(output, "abcde", 5));
    io_mode = 1; assert(mulib_shoebox_write_all(&c, 1, "x", 1, &n) == MULIB_SHOEBOX_IO && n == 0);
    c.abi_version = 2; assert(mulib_shoebox_dispatch(a, 1, "shoebox", &c, 2, direct, &status) == MULIB_SHOEBOX_ABI && status == 125);
}
int main(int argc, char **argv) {
    if (argc == 3) {
        uint8_t data[4096]; size_t n, count; int init; FILE *f;
        if (!strcmp(argv[1], "emit")) {
            n = archive_make(data, sizeof(data), MULIB_CPIO_CRC); f = fopen(argv[2], "wb"); assert(f);
            assert(fwrite(data, 1, n, f) == n); assert(!fclose(f));
        } else {
            f = fopen(argv[2], "rb"); assert(f); n = fread(data, 1, sizeof(data), f); assert(feof(f)); fclose(f);
            assert(!mulib_initsysutils_validate(data, n, &count, &init) && count && init);
        }
        return 0;
    }
    archives(); contract(); return 0;
}
