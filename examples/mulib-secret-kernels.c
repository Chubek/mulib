/* Synthetic demonstration: authenticate an in-memory development record. */
#include <stdlib.h>
#include <string.h>

#define MULIB_IMPL_ENTROPY
#define MULIB_IMPL_AUTHCRYPTO
#define MULIB_IMPL_HKDF
#define MULIB_IMPL_SHA
#define MULIB_IMPL_AES_GCM
#define MULIB_IMPL_SECRET_CORE
#define MULIB_IMPL_SECRET_CRYPTO
#define MULIB_IMPL_SECRET_CRYPTO_NATIVE
#define MULIB_IMPL_SECRET_RECORD
#include "mulib-secret-crypto-native.h"
#include "mulib-secret-record.h"

int main(void)
{
#if !defined(__unix__) && !defined(__APPLE__) && !defined(__linux__)
    return 77; /* Native entropy is unavailable on this platform. */
#else
    static const unsigned char fixture[] = "synthetic record, not a credential";
    mulib_scr_provider crypto;
    mulib_sr_meta meta = {{0}, 1, 1};
    unsigned char root[32], wire[256], opened[sizeof fixture];
    size_t wire_size = 0, opened_size = 0;
    mulib_sc_view text = {fixture, sizeof fixture - 1};
    mulib_sc_buf encoded = {wire, sizeof wire};
    mulib_sc_buf plain = {opened, sizeof opened};

    meta.id[0] = 1;
    mulib_scr_native(&crypto);
    int status = mulib_scr_random(&crypto, root, sizeof root);
    if (!status)
        status = mulib_sr_seal(&crypto, MULIB_SCR_AES128_GCM, root, &meta,
                               text, encoded, &wire_size);
    if (!status) {
        mulib_sc_view view = {wire, wire_size};
        status = mulib_sr_open_pinned(&crypto, MULIB_SCR_AES128_GCM, root, &meta,
                                      view, plain, &opened_size);
    }
    int matches = !status && opened_size == text.len
        && memcmp(opened, fixture, text.len) == 0;
    mulib_sc_wipe(opened, sizeof opened);
    mulib_sc_wipe(root, sizeof root);
    return matches ? EXIT_SUCCESS : EXIT_FAILURE;
#endif
}
