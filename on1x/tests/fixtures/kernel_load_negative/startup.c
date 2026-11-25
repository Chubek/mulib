#include <stdio.h>

void __real_on1x_environ_init(int argc, char **argv);

/* Observe the generated application's argv initializer. A failed import
 * transaction must not reach this adapter or any user main instructions. */
void __wrap_on1x_environ_init(int argc, char **argv)
{
    __real_on1x_environ_init(argc, argv);
    puts("123");
}
