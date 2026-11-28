#include "mulib-web-textcodec.h"
#include <stdio.h>

int main(void)
{
    mulib_web_textcodec_decoder decoder;
    muba_buf output = {0};
    /* Split the initial BOM and U+20AC across chunks. */
    const muba_span chunks[] = {
        muba_str("\xef"), muba_str("\xbb\xbf\xe2\x82"), muba_str("\xac\n")
    };
    mulib_web_textcodec_decoder_init(&decoder, 4, 1, 0);
    for (size_t i = 0; i < sizeof chunks / sizeof *chunks; ++i) {
        if (mulib_web_textcodec_decoder_feed(&decoder, chunks[i],
                                             i == 2, &output) != MULIB_WEB_TEXTCODEC_OK) {
            muba_free(&output);
            return 1;
        }
        if (output.len && fwrite(output.data, 1, output.len, stdout) != output.len) {
            muba_free(&output);
            return 1;
        }
    }
    muba_free(&output);
    return 0;
}
