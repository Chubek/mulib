#include "runner.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc == 2) return on1x_archvirt_run_path(argv[1]);
    if (argc == 3 && !strcmp(argv[1], "--check")) return on1x_archvirt_check_path(argv[2]);
    fputs("usage: on1x-archvirt-run [--check] GUEST_ELF\n", stderr);
    return 2;
}
