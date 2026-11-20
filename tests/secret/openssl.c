#include <stdio.h>
#include <string.h>
#include "headers.h"

#define MULIB_IMPL_SECRET_CRYPTO_OPENSSL
#include "mulib-secret-crypto-openssl.h"

static int record_round_trip(const mulib_scr_provider *writer,
                             const mulib_scr_provider *reader)
{
    static const unsigned char root[32] = {7};
    static const unsigned char fixture[] = {0, 1, 255, 0, 42};
    const mulib_sr_meta meta = {{1}, 2, 3};
    unsigned char wire[MULIB_SR_OVERHEAD + sizeof fixture], plain[sizeof fixture];
    mulib_sc_view input = {fixture, sizeof fixture};
    mulib_sc_buf sealed = {wire, sizeof wire}, opened = {plain, sizeof plain};
    size_t size = 0, written = 0;
    if (mulib_sr_seal(writer, MULIB_SCR_AES128_GCM, root, &meta, input,
                      sealed, &size) || size != sizeof wire)
        return 1;
    mulib_sc_view encoded = {wire, size};
    return mulib_sr_open_pinned(reader, MULIB_SCR_AES128_GCM, root, &meta,
                                 encoded, opened, &written) ||
        written != sizeof fixture || memcmp(plain, fixture, written);
}

int main(void)
{
    mulib_scr_provider provider, native;
    mulib_scr_openssl(&provider);
    for (unsigned alg = MULIB_SCR_AES128_GCM; alg <= MULIB_SCR_AES256_GCM; ++alg) {
        int result = mulib_secret_selftest(&provider, alg);
        if (result) {
            fprintf(stderr, "OpenSSL secret selftest (AEAD %u) failed at case %d\n", alg, result);
            return 1;
        }
    }
    mulib_scr_native(&native);
    int cross_result = record_round_trip(&provider, &native);
    /* Native entropy intentionally fails on some non-POSIX hosts. */
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
    cross_result |= record_round_trip(&native, &provider);
#endif
    if (cross_result) {
        fputs("Native/OpenSSL secret record interoperability check failed\n", stderr);
        return 1;
    }
    return 0;
}
