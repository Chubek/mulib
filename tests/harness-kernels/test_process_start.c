#include "mulib-process-stream.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

static int delay_exec;
int __real_execvp(const char *file, char *const argv[]);
int __wrap_execvp(const char *file, char *const argv[])
{
    if (delay_exec) {
        struct timespec delay = {3, 0};
        while (nanosleep(&delay, &delay) < 0 && errno == EINTR)
            ;
    }
    return __real_execvp(file, argv);
}

static int collect(void *user, int stream, const unsigned char *data, size_t n)
{
    size_t *received = user;
    assert(stream == 1 && n <= 4 - *received);
    assert(!memcmp(data, &"okay"[*received], n));
    *received += n;
    return 1;
}

int main(void)
{
    mulib_ps_t p = MULIB_PS_INIT;
    const char *const argv[] = {"/bin/sh", "-c", "printf okay", NULL};
    mulib_ps_spec_t spec = {"/bin/sh", argv, NULL, NULL, 0, 0, 0, 0, 100, 16};
    delay_exec = 1;
    assert(mulib_ps_start(&p, &spec) == MULIB_PS_TIMEOUT);
    assert(p.pid == 0 && p.input == -1 && p.output == -1 && p.errors == -1);
    int status;
    assert(waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD);

    delay_exec = 0;
    spec.timeout_ms = 2000;
    for (int fd = 0; fd <= 2; ++fd)
        close(fd);
    assert(mulib_ps_start(&p, &spec) == MULIB_PS_RUNNING);
    size_t received = 0;
    assert(mulib_ps_wait(&p, collect, &received) == MULIB_PS_DONE);
    assert(received == 4 && p.exit_code == 0);
    for (int fd = 0; fd <= 2; ++fd)
        assert(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
    mulib_ps_destroy(&p);
}
