#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Build-host only. The driver supplies a fresh path, never taken from the AST. */
bool on1x_inline_c11_publish(const char *source)
{
    const char *path = getenv("ON1X_TRUSTED_INLINE_C11_OUTPUT");
    FILE *out = path && *path ? fopen(path, "wx") : NULL;
    if (!out) return false;
    bool ok = fputs(source, out) >= 0;
    return fclose(out) == 0 && ok;
}
