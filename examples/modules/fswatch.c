#include <assert.h>
#include <errno.h>
#include <stdio.h>
#define MULIB_IMPL_FSWATCH
#define MULIB_IMPL_SELECT
#define MULIB_IMPL_VFS
#define MULIB_IMPL_DAEMON
#define MULIB_IMPL_PROCESS
#define MULIB_IMPL_MUNE
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_GLOB
#include "mulib-fswatch.h"
#include "mulib-select.h"
#if defined(__linux__)
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#endif
static int changed(void *user, const char *handler, const mulib_fswatch_change_t *event) {
    int *count = (int *)user;
    assert(!strcmp(handler, "rebuild") && !strcmp(event->path, "hello.c"));
    ++*count;
    return 0;
}
int main(void) {
    {
        const char source[] = "watch \"/src\" recursive { on create|modify \"*.c\" -> rebuild; }";
        mulib_fswatch_program_t *program = mulib_fswatch_parse(source, sizeof(source)-1, NULL);
        mulib_fswatch_runtime_t *runtime;
        mulib_vfs vfs;
        mulib_vfs_mem mem;
        mulib_vfs_ops ops = mulib_vfs_mem_ops();
        int count = 0;
        assert(program && !mulib_vfs_init(&vfs, 1) && !mulib_vfs_mem_init(&mem, 8, 1024));
        assert(!mulib_vfs_mount(&vfs, "/src", &ops, &mem));
        runtime = mulib_fswatch_runtime_create(program, mulib_fswatch_vfs_scan, &vfs, 32);
        assert(runtime && !mulib_fswatch_poll(runtime, changed, &count));
        assert(!mulib_vfs_mem_put(&mem, "/hello.c", "int main(void) {}", 17));
        assert(!mulib_fswatch_poll(runtime, changed, &count) && count == 1);
        mulib_fswatch_runtime_destroy(runtime); mulib_fswatch_program_destroy(program);
        mulib_vfs_free(&vfs); mulib_vfs_mem_free(&mem);
    }
#if defined(__linux__)
    int fd = mulib_fswatch_create(), watch, file, made = 0;
    unsigned attempt;
    char path[128], child[160], buffer[4096]; size_t length, offset = 0;
    mulib_select_entry entry; mulib_fswatch_event event;
    if (fd < 0) {
        if (errno == ENOSYS || errno == EPERM || errno == EACCES || errno == EMFILE || errno == ENFILE) {
            perror("SKIP inotify unavailable"); return 0;
        }
        assert(0);
    }
    for (attempt = 0; attempt < 100; ++attempt) {
        assert(snprintf(path, sizeof path, "mulib-fswatch-example-%ld-%u", (long)getpid(), attempt) > 0);
        if (mkdir(path, 0700) == 0) { made = 1; break; }
        if (errno == EEXIST) continue;
        if (errno == EACCES || errno == EPERM || errno == EROFS || errno == ENOSPC) {
            perror("SKIP writable local directory unavailable");
            assert(mulib_fswatch_close(fd) == 0); return 0;
        }
        assert(0);
    }
    if (!made) {
        fputs("SKIP filesystem example: test names occupied\n", stderr);
        assert(mulib_fswatch_close(fd) == 0); return 0;
    }
    watch = mulib_fswatch_add(fd, path, MULIB_FSWATCH_CREATE);
    if (watch < 0) {
        int error = errno;
        assert(mulib_fswatch_close(fd) == 0 && rmdir(path) == 0); errno = error;
        if (errno == EPERM || errno == EACCES || errno == ENOSPC || errno == ENOSYS) {
            perror("SKIP inotify watch unavailable"); return 0;
        }
        assert(0);
    }
    assert(snprintf(child, sizeof child, "%s/item", path) > 0);
    file = open(child, O_WRONLY | O_CREAT | O_EXCL, 0600); assert(file >= 0 && close(file) == 0);
    entry.fd = fd; entry.events = MULIB_SELECT_READ; entry.revents = 0;
    assert(mulib_select_wait(&entry, 1, 1000) == 1);
    assert(mulib_fswatch_read(fd, buffer, sizeof buffer, &length) == 0);
    assert(mulib_fswatch_next(buffer, length, &offset, &event) == 1);
    assert(event.watch == watch && (event.mask & MULIB_FSWATCH_CREATE) && event.name_length == 4);
    assert(mulib_fswatch_remove(fd, watch) == 0 && mulib_fswatch_close(fd) == 0);
    assert(unlink(child) == 0 && rmdir(path) == 0);
#else
    assert(mulib_fswatch_create() == -1 && errno == ENOSYS);
    fputs("SKIP inotify requires Linux\n", stderr);
#endif
    return 0;
}
