/*
 * The compiler frontend already owns ordinary mulib_sha_* symbols in some
 * aggregate links. Prefix this SDK-private one-TU instance so standalone SDK
 * consumers and the full compiler can coexist without duplicate definitions.
 */
#define mulib_sha_digest_size on1x_sdk_mu_sha_digest_size
#define mulib_sha_init on1x_sdk_mu_sha_init
#define mulib_sha_init_intrinsics on1x_sdk_mu_sha_init_intrinsics
#define mulib_sha_update on1x_sdk_mu_sha_update
#define mulib_sha_final on1x_sdk_mu_sha_final
#define mulib_sha_squeeze on1x_sdk_mu_sha_squeeze
#define mulib_sha on1x_sdk_mu_sha
#define mulib_sha_intrinsics on1x_sdk_mu_sha_intrinsics
#define mulib_sha0 on1x_sdk_mu_sha0
#define mulib_sha1 on1x_sdk_mu_sha1
#define mulib_sha224 on1x_sdk_mu_sha224
#define mulib_sha256 on1x_sdk_mu_sha256
#define mulib_sha384 on1x_sdk_mu_sha384
#define mulib_sha512 on1x_sdk_mu_sha512
#define mulib_sha512_224 on1x_sdk_mu_sha512_224
#define mulib_sha512_256 on1x_sdk_mu_sha512_256
#define mulib_sha3_224 on1x_sdk_mu_sha3_224
#define mulib_sha3_256 on1x_sdk_mu_sha3_256
#define mulib_sha3_384 on1x_sdk_mu_sha3_384
#define mulib_sha3_512 on1x_sdk_mu_sha3_512
#define mulib_shake128 on1x_sdk_mu_shake128
#define mulib_shake256 on1x_sdk_mu_shake256

#define MULIB_IMPL_SHA
#include "mulib-sha.h"
