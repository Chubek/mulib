#define MULIB_IMPL_FRAME_LAYOUT
#include "mulib-frame-layout.h"
#include <assert.h>
int main(void) {
    mulib_frame_layout_t frame={0};size_t id,size;
    assert(mulib_frame_layout_add(&frame,3,1,&id));
    assert(mulib_frame_layout_add(&frame,8,8,&id)&&frame.slots[id].offset==8);
    assert(mulib_frame_layout_finish(&frame,16,&size)&&size==16);
    mulib_frame_layout_free(&frame);return 0;
}
