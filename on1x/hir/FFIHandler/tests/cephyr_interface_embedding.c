/* Exercise the public C embedding entry point against the CLI fixtures. */
#include <cephyr/Cephyr.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
    static char source[65536];
    const size_t size = fread(source, 1, sizeof source, stdin);
    if (size == sizeof source || ferror(stdin))
        return 2;

    cephyr_request request = {
        .struct_size = sizeof request,
        .argc = (size_t)(argc - 1),
        .argv = (const char *const *)(argv + 1),
        .stdin_data = source,
        .stdin_size = size,
    };
    cephyr_result *result = NULL;
    int status = cephyr_emit_interface(&request, &result);
    if (result) {
        size_t diagnostic_size, output_size;
        const char *diagnostics = cephyr_result_diagnostics(result, &diagnostic_size);
        const void *output = cephyr_result_output(result, &output_size);
        if ((diagnostic_size && fwrite(diagnostics, 1, diagnostic_size, stderr) != diagnostic_size) ||
            (status == CEPHYR_OK && output_size &&
             fwrite(output, 1, output_size, stdout) != output_size))
            status = CEPHYR_INTERNAL_ERROR;
        cephyr_result_destroy(result);
    } else if (status == CEPHYR_OK) {
        status = CEPHYR_INTERNAL_ERROR;
    }
    return status == CEPHYR_OK ? 0 : 1;
}
