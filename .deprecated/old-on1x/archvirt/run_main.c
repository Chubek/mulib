#include "runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <unistd.h>

static int managed(const char *parent, const char *descriptor, const char *guest)
{
    char *end;
    long owner = strtol(parent, &end, 10);
    if (!*parent || *end || owner < 2 || owner > INT_MAX) return 2;
    long ready = strtol(descriptor, &end, 10);
    struct stat st;
    if (!*descriptor || *end || ready < 3 || ready > INT_MAX ||
        fstat((int)ready, &st) || !S_ISFIFO(st.st_mode)) return 2;
    if (getppid() != owner || prctl(PR_SET_PDEATHSIG, SIGKILL) || getppid() != owner) {
        fputs("On1x ArchVirt: managed session owner is unavailable\n", stderr);
        return 125;
    }
    return on1x_archvirt_run_managed_path(guest, (int)ready);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--interactive") && strcmp(argv[1], "--check"))
        return on1x_archvirt_run_path(argv[1]);
    if (argc == 3 && !strcmp(argv[1], "--check")) return on1x_archvirt_check_path(argv[2]);
    if (argc == 3 && !strcmp(argv[1], "--interactive"))
        return on1x_archvirt_run_interactive_path(argv[2]);
    if (argc == 5 && !strcmp(argv[1], "--managed"))
        return managed(argv[2], argv[3], argv[4]);
    fputs("usage: on1x-archvirt-run [--check | --interactive] GUEST_ELF\n", stderr);
    return 2;
}
