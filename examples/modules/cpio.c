#define MULIB_IMPL_CPIO
#include "mulib-cpio.h"
#include <assert.h>
int main(void) {
    uint8_t data[512]; mulib_cpio_writer_t writer; mulib_cpio_entry_t entry = {0}, view;
    size_t cursor = 0;
    entry.name = "hello"; entry.name_bytes = 5; entry.data = "hello\n"; entry.size = 6;
    entry.mode = MULIB_CPIO_REGULAR | 0644; entry.nlink = 1; entry.ino = 1;
    assert(!mulib_cpio_writer_init(&writer, data, sizeof(data), MULIB_CPIO_NEWC));
    assert(!mulib_cpio_add(&writer, &entry)); assert(!mulib_cpio_finish(&writer, 512));
    assert(!mulib_cpio_next(data, writer.size, &cursor, &view) && view.size == 6);
    assert(mulib_cpio_next(data, writer.size, &cursor, &view) == MULIB_CPIO_TRAILER);
    return 0;
}
