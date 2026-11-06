#define MULIB_IMPL_CPIO
#define MULIB_IMPL_INITSYSUTILS
#include "mulib-initsysutils.h"
#include <assert.h>
int main(void) {
    uint8_t data[512]; mulib_cpio_writer_t writer; mulib_cpio_entry_t entry = {0};
    size_t count; int has_init;
    entry.name = "init"; entry.name_bytes = 4; entry.data = "#!/bin/sh\n"; entry.size = 10;
    entry.mode = MULIB_CPIO_REGULAR | 0755; entry.nlink = 1;
    assert(!mulib_cpio_writer_init(&writer, data, sizeof(data), MULIB_CPIO_NEWC));
    assert(!mulib_initsysutils_add(&writer, &entry)); assert(!mulib_cpio_finish(&writer, 4));
    assert(!mulib_initsysutils_validate(data, writer.size, &count, &has_init));
    assert(count == 1 && has_init); /* /bin/sh must be provided in a real image. */
    return 0;
}
