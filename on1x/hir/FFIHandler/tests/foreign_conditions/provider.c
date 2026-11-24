/* This test bridge owns the native result. The generated classifier does not. */
#include <cephyr/Cephyr.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

_Static_assert(INT_MAX == INT32_MAX && INT_MIN == INT32_MIN, "C int status layout");
_Static_assert(_Generic(&cephyr_emit_interface,
                       int (*)(const cephyr_request *, cephyr_result **): 1,
                       default: 0), "native prototype");

static int64_t calls;

int64_t fixture_projection_calls(void)
{
    return calls;
}

int64_t fixture_projection_status(int64_t mode)
{
    static const char *const args[] = {
        "--target=x86_64-linux-gnu", "--stdlib=cephyr", "-x", "c", "-"
    };
    const char *source = mode == 1 ? "invalid !\n" : "int sample(void);\n";
    const cephyr_request request = {
        .struct_size = sizeof request,
        .argc = sizeof args / sizeof args[0],
        .argv = args,
        .stdin_data = source,
        .stdin_size = strlen(source),
    };
    cephyr_result *result = NULL;
    ++calls;
    const int status = cephyr_emit_interface(mode == 2 ? NULL : &request, &result);
    cephyr_result_destroy(result);
    return status;
}
