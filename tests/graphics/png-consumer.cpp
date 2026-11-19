/* Strict C++17 inclusion and linkage against png-producer.c. No GLX dependency. */
#include "mulib-imagepng.h"
#include "mulib-imagepng.h"
#include <cassert>
#include <cstring>

int
main()
{
    const unsigned char rgb[] = {12, 34, 56, 78, 90, 123};
    const unsigned char expected[] = {12, 34, 56, 255, 78, 90, 123, 255};
    mulib_typecore_buf_t encoded{};
    mulib_imagepng_rgba_t image{};
    mulib_imagepng_limits_t limits = mulib_imagepng_limits_default();

    /* Both the pre-existing encoder and the additive inline decoder call
     * independently compiled C implementations, not C++ implementation macros. */
    assert(mulib_imagepng_encode_rgb(rgb, 2, 1, &encoded));
    assert(mulib_imagepng_decode_rgba(encoded.data, encoded.size, &image, &limits));
    assert(image.width == 2 && image.height == 1 && image.stride == 8);
    assert(!std::memcmp(image.pixels, expected, sizeof expected));
    assert(!mulib_imagepng_decode_rgba(encoded.data, encoded.size, &image, &limits));
    assert(!std::memcmp(image.pixels, expected, sizeof expected));

    mulib_typecore_free(&encoded);
    assert(!std::memcmp(image.pixels, expected, sizeof expected));
    mulib_imagepng_rgba_free(&image);
    assert(!image.pixels && !image.width && !image.height && !image.stride);
    mulib_imagepng_rgba_free(&image);
    return 0;
}
