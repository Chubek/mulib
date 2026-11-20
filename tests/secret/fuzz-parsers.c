/* Structural and canonical round-trip checks over synthetic inputs. */
#include <assert.h>
#include <stdint.h>
#include <string.h>

#define MULIB_IMPL_SECRET_CORE
#define MULIB_IMPL_SECRET_CODEC
#define MULIB_IMPL_SECRET_OTP_URI
#define MULIB_IMPL_SECRET_CATALOG
#define MULIB_IMPL_SECRET_SSH
#include "mulib-secret-otp-uri.h"
#include "mulib-secret-catalog.h"
#include "mulib-secret-ssh.h"

#define SECRET_FUZZ_MAX 4096u

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > SECRET_FUZZ_MAX)
        return 0;

    size_t n = 0, encoded_length = 0;
    unsigned char bytes[SECRET_FUZZ_MAX];
    char text[SECRET_FUZZ_MAX + 1];
    mulib_sc_view input = {data, size};
    mulib_sc_buf out = {bytes, sizeof bytes};
    for (unsigned format = MULIB_SCODE_BASE64URL; format <= MULIB_SCODE_HEX; ++format) {
        if (!mulib_scode_decode(format, (const char *)data, size, out, &n)) {
            assert(n <= sizeof bytes);
            mulib_sc_view decoded = {bytes, n};
            assert(!mulib_scode_encode(format, decoded, text, sizeof text, &encoded_length));
            assert(encoded_length == size && (!size || !memcmp(text, data, size)));
        }
    }

    mulib_scat_entry entries[SECRET_FUZZ_MAX / MULIB_SCAT_ENTRY_SIZE];
    if (!mulib_scat_decode(input, entries, sizeof entries / sizeof entries[0], &n)) {
        assert(n <= sizeof entries / sizeof entries[0]);
        assert(!mulib_scat_encode(entries, n, out, &encoded_length));
        assert(encoded_length == size && !memcmp(bytes, data, size));
    }

    mulib_sotp_uri uri = {0};
    if (!mulib_sotp_uri_parse((const char *)data, size, &uri)) {
        assert(uri.key_length && uri.key_length <= sizeof uri.key);
        assert(memchr(uri.label, 0, sizeof uri.label));
        assert(memchr(uri.issuer, 0, sizeof uri.issuer));
    }
    mulib_sotp_uri_clear(&uri);

    mulib_ssh_sign_request request;
    if (!mulib_ssh_secret_parse_sign(input, &request)) {
        assert(size >= 13);
        assert(request.public_blob.len && request.public_blob.len <= size - 13);
        assert(request.data.len == size - 13 - request.public_blob.len);
        assert(request.public_blob.ptr == data + 5);
        assert(request.data.ptr == data + 9 + request.public_blob.len);
    }
    return 0;
}
