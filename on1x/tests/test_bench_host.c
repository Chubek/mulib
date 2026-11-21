#define _POSIX_C_SOURCE 200809L
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../benchmark/native/bench_host.h"
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "child")) {
        assert(getchar() == EOF);
        puts(argv[2]);
        fputs("child stderr\n", stderr);
        return 23;
    }
    if (argc > 1 && !strcmp(argv[1], "signal")) {
        raise(SIGTERM);
        return 1;
    }
    if (argc > 1 && !strcmp(argv[1], "timeout")) {
        pid_t descendant = fork();
        assert(descendant >= 0);
        if (descendant) {
            FILE *pidfile = fopen(argv[2], "w");
            assert(pidfile);
            fprintf(pidfile, "%ld\n", (long)descendant);
            assert(!fclose(pidfile));
        }
        for (;;) pause();
    }
    assert(argc == 2); /* caller-provided temporary output directory */
    char out[4096], err[4096], pidfile[4096], text[128];
    assert(snprintf(out, sizeof out, "%s/host.out", argv[1]) < (int)sizeof out);
    assert(snprintf(err, sizeof err, "%s/host.err", argv[1]) < (int)sizeof err);
    assert(snprintf(pidfile, sizeof pidfile, "%s/host.pid", argv[1]) < (int)sizeof pidfile);
    on1x_bench_result result;
    const char *args[] = {argv[0], "child", "$(printf unsafe); literal", NULL};
    assert(!on1x_bench_run(args, out, err, 1000, &result));
    assert(result.exit_code == 23 && !result.signal_number && !result.timed_out);
    assert(result.elapsed_ns > 0);
    FILE *f = fopen(out, "r");
    assert(f && fgets(text, sizeof text, f));
    assert(!strcmp(text, "$(printf unsafe); literal\n"));
    assert(!fclose(f));
    f = fopen(err, "r");
    assert(f && fgets(text, sizeof text, f));
    assert(!strcmp(text, "child stderr\n"));
    assert(!fclose(f));
    args[1] = "signal";
    assert(!on1x_bench_run(args, out, err, 1000, &result));
    assert(result.exit_code == -1 && result.signal_number == SIGTERM);
    assert(!result.timed_out);
    args[1] = "timeout"; args[2] = pidfile;
    assert(!on1x_bench_run(args, out, err, 200, &result));
    assert(result.timed_out && result.exit_code == -1 && result.signal_number == SIGKILL);
    assert(result.elapsed_ns >= 200000000.0 && result.elapsed_ns < 2000000000.0);
    f = fopen(pidfile, "r");
    long descendant;
    assert(f && fscanf(f, "%ld", &descendant) == 1);
    assert(!fclose(f));
#ifdef __linux__
    /* An orphan may briefly remain a zombie until init reaps it; it must not
     * remain executable after the process-group deadline. */
    char proc[128], state = 0;
    snprintf(proc, sizeof proc, "/proc/%ld/stat", descendant);
    for (int attempt = 0; attempt < 100; ++attempt) {
        f = fopen(proc, "r");
        if (!f) break;
        assert(fscanf(f, "%*d %*s %c", &state) == 1);
        fclose(f);
        if (state == 'Z' || state == 'X') break;
        struct timespec pause_time = {0, 1000000};
        nanosleep(&pause_time, NULL);
    }
    assert(!f || state == 'Z' || state == 'X');
#endif
    assert(on1x_bench_run(args, out, err, 0, &result) == -1 && errno == EINVAL);
    assert(on1x_bench_run(NULL, out, err, 1, &result) == -1 && errno == EINVAL);
    const char *too_many[4098];
    for (int i = 0; i < 4097; ++i) too_many[i] = argv[0];
    too_many[4097] = NULL;
    assert(on1x_bench_run(too_many, out, err, 1, &result) == -1 && errno == E2BIG);
    args[0] = "/no/such/on1x-benchmark-executable";
    assert(on1x_bench_run(args, out, err, 1, &result) == -1 && errno == ENOENT);
    args[0] = argv[0];
    assert(on1x_bench_run(args, "/no/such/directory/out", err, 1, &result) == -1 && errno == ENOENT);
    puts("benchmark host: argv, redirection, stdin, exit, signal, group timeout, negative inputs PASS");
    return 0;
}
