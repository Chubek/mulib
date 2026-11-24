#include "runner.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fputs("usage: on1x-archvirt-run GUEST_ELF\n", stderr);
        return 2;
    }
    return on1x_archvirt_run_path(argv[1]);
}
