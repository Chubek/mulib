#define _POSIX_C_SOURCE 200809L
#include "vmm.h"
#include <archvirt.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(expr) do { \
    if (!(expr)) { fprintf(stderr, "line %d: %s\n", __LINE__, #expr); exit(1); } \
} while (0)

static const uint8_t commands[] = {'u', 'n', 'a', 'm', 'e', '\n', 'e', 'x', 'i', 't', '\n'};

static On1xVmmConfigV1 defaults(const char *machine) {
    return (On1xVmmConfigV1){
        .struct_size = sizeof(On1xVmmConfigV1), .machine = machine, .backend = "software",
        .memory_mb = 40, .instruction_limit = 3000000, .flags = ON1X_VMM_DOMIX | ON1X_VMM_DOMIOS
    };
}

static int captured(const On1xVmmReportV1 *r, const char *text) {
    const size_t n = strlen(text);
    for (size_t i = 0; i + n <= r->serial_output_size; ++i)
        if (!memcmp(r->serial_output + i, text, n)) return 1;
    return 0;
}

static void serial_boot(const On1xVmmConfigV1 *c) {
    On1xVmmReportV1 r;
    On1xVmmStatus status = on1x_vmm_run_v1(c, commands, sizeof commands, &r);
    if (status != ON1X_VMM_OK)
        fprintf(stderr, "%s boot (%d): %s\n", c->machine, status, r.error);
    CHECK(status == ON1X_VMM_OK);
    CHECK(r.serial_input_used == sizeof commands);
    CHECK(!r.serial_output_truncated && !r.error_truncated && !r.error[0]);
    char banner[32];
    CHECK(snprintf(banner, sizeof banner, "Domix %s\n", c->machine) > 0);
    CHECK(captured(&r, banner) && captured(&r, "Domix shutdown"));
}

static void make_path(char *buf, size_t capacity, const char *dir, const char *file) {
    int n = snprintf(buf, capacity, "%s/%s", dir, file);
    CHECK(n > 0 && (size_t)n < capacity);
}

static void boot_paths(const char *data_dir) {
    On1xVmmConfigV1 c = defaults("riscv64");
    char profile[4097], kernel[4097], image[4097], firmware[4097];
    make_path(profile, sizeof profile, data_dir, "machines/riscv64.aml");
    make_path(kernel, sizeof kernel, data_dir, "domix/riscv64/kernel");
    make_path(image, sizeof image, data_dir, "domix/riscv64/domix.img");
    make_path(firmware, sizeof firmware, data_dir, "domios/domios.img");
    c.archml_path = profile;
    c.kernel_path = kernel;
    c.image_path = image;
    c.firmware_path = firmware;
    c.cmdline = "on1x-vmm fixture";
    c.backend = "auto";
    const uint8_t input[] = "\0cmdline\nexit\n";
    On1xVmmReportV1 r;
    const On1xVmmStatus status = on1x_vmm_run_v1(&c, input, sizeof input - 1, &r);
    if (status != ON1X_VMM_OK) fprintf(stderr, "explicit paths (%d): %s\n", status, r.error);
    CHECK(status == ON1X_VMM_OK && r.serial_input_used == sizeof input - 1);
    CHECK(captured(&r, c.cmdline) && captured(&r, "Domix shutdown"));
}

struct Concurrent {
    On1xVmmConfigV1 config;
    On1xVmmStatus status;
};

static void *run_with_fifo(void *data) {
    struct Concurrent *t = data;
    On1xVmmReportV1 r;
    t->status = on1x_vmm_run_v1(&t->config, NULL, 0, &r);
    return NULL;
}

static void reject_overlap(void) {
    char dir[] = "./on1x-vmm-fifo-XXXXXX";
    CHECK(mkdtemp(dir));
    char path[sizeof dir + 8];
    make_path(path, sizeof path, dir, "profile");
    CHECK(mkfifo(path, 0600) == 0);
    On1xVmmConfigV1 c = defaults("riscv64");
    struct Concurrent t = {c, ON1X_VMM_OK};
    t.config.archml_path = path;
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, run_with_fifo, &t) == 0);
    /* Opening the writer waits until upstream has entered init and opened
     * the reader. Keep it open so init cannot finish reading before the check. */
    const int fd = open(path, O_WRONLY);
    CHECK(fd >= 0);
    On1xVmmReportV1 r;
    const On1xVmmStatus status = on1x_vmm_run_v1(&c, NULL, 0, &r);
    CHECK(close(fd) == 0 && pthread_join(thread, NULL) == 0);
    CHECK(unlink(path) == 0 && rmdir(dir) == 0);
    CHECK(status == ON1X_VMM_BUSY && t.status == ON1X_VMM_INITIALIZE_FAILED);
    serial_boot(&c);
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    const char *version = archvirt_version();
    CHECK(version && !strcmp(version, "0.3.0"));
    On1xVmmConfigV1 c = defaults("riscv64");
    const On1xVmmConfigV1 orig = c;
    On1xVmmReportV1 r;
    CHECK(on1x_vmm_run_v1(&c, NULL, 0, NULL) == ON1X_VMM_INVALID_CONFIG);
    CHECK(on1x_vmm_run_v1(NULL, NULL, 0, &r) == ON1X_VMM_INVALID_CONFIG);
    CHECK(r.error[0] && !r.serial_output_size);
    c.struct_size = 0;
    CHECK(on1x_vmm_run_v1(&c, NULL, 0, &r) == ON1X_VMM_INVALID_CONFIG);
    c = orig;
    c.machine = "unknown";
    CHECK(on1x_vmm_run_v1(&c, NULL, 0, &r) == ON1X_VMM_INVALID_CONFIG);
    c = orig;
    c.backend = "kvm";
    CHECK(on1x_vmm_run_v1(&c, NULL, 0, &r) == ON1X_VMM_UNSUPPORTED_BACKEND);
    c = orig;
    c.memory_mb = UINT64_MAX;
    CHECK(on1x_vmm_run_v1(&c, NULL, 0, &r) == ON1X_VMM_INVALID_CONFIG);
    c = orig;
    c.instruction_limit = 0;
    CHECK(on1x_vmm_run_v1(&c, NULL, 0, &r) == ON1X_VMM_INVALID_CONFIG);
    c.instruction_limit = UINT64_MAX;
    CHECK(on1x_vmm_run_v1(&c, NULL, 0, &r) == ON1X_VMM_INVALID_CONFIG);
    c = orig;
    c.flags = UINT32_MAX;
    CHECK(on1x_vmm_run_v1(&c, NULL, 0, &r) == ON1X_VMM_INVALID_CONFIG);
    c = orig;
    c.flags = ON1X_VMM_DOMIX;
    c.firmware_path = "ignored-but-not-accepted";
    CHECK(on1x_vmm_run_v1(&c, NULL, 0, &r) == ON1X_VMM_INVALID_CONFIG);
    c = orig;
    c.flags = ON1X_VMM_DOMIOS;
    CHECK(on1x_vmm_run_v1(&c, NULL, 0, &r) == ON1X_VMM_INVALID_CONFIG);
    c = orig;
    CHECK(on1x_vmm_run_v1(&c, NULL, 1, &r) == ON1X_VMM_INVALID_CONFIG);
    CHECK(on1x_vmm_run_v1(&c, commands, SIZE_MAX, &r) == ON1X_VMM_INVALID_CONFIG);
    char oversized[ON1X_VMM_MAX_PATH_BYTES + 2];
    memset(oversized, 'x', sizeof oversized - 1);
    oversized[sizeof oversized - 1] = '\0';
    c.archml_path = oversized;
    CHECK(on1x_vmm_run_v1(&c, NULL, 0, &r) == ON1X_VMM_INVALID_CONFIG);
    oversized[sizeof oversized - 2] = '\0';
    CHECK(on1x_vmm_run_v1(&c, NULL, 0, &r) == ON1X_VMM_INITIALIZE_FAILED);
    CHECK(r.error_truncated && strlen(r.error) == sizeof r.error - 1);
    On1xVmmReportV1 saved = r;
    c = orig;
    c.instruction_limit = 1;
    CHECK(on1x_vmm_run_v1(&c, NULL, 0, &r) == ON1X_VMM_WAIT_FAILED);
    CHECK(strstr(r.error, "instruction limit") && !r.error_truncated);
    CHECK(saved.error_truncated && strstr(saved.error, "cannot open"));

    c = defaults("x86_64"); serial_boot(&c);
    c = defaults("aarch64"); serial_boot(&c);
    c = orig; serial_boot(&c);
    boot_paths(argv[1]);

    uint8_t noisy[ON1X_VMM_MAX_INPUT_BYTES];
    size_t n = 0;
    for (int i = 0; i < 100; ++i) {
        memcpy(noisy + n, "help\n", 5);
        n += 5;
    }
    memcpy(noisy + n, "exit\n", 5);
    n += 5;
    c.instruction_limit = 20000000;
    const On1xVmmStatus noisy_status = on1x_vmm_run_v1(&c, noisy, n, &r);
    if (noisy_status != ON1X_VMM_OK)
        fprintf(stderr, "capture fixture (%d, %zu input bytes): %s\n",
                noisy_status, r.serial_input_used, r.error);
    CHECK(noisy_status == ON1X_VMM_OK);
    CHECK(r.serial_input_used == n);
    CHECK(r.serial_output_truncated && r.serial_output_size == sizeof r.serial_output && !r.error[0]);
    reject_overlap();
    CHECK(!strcmp(version, "0.3.0"));
    puts("ArchVirt host bridge lifecycle, binary serial bounds, and failure recovery passed");
    return 0;
}
