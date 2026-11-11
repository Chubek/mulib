/* Convenience implementation switches. Include before mulib-tls.h or muw3.h.
 * Socket/URI/HTTP dependencies remain selected by the embedding application. */
#define MULIB_IMPL_ENTROPY
#define MULIB_IMPL_SHA
#define MULIB_IMPL_HKDF
#define MULIB_IMPL_X25519
#define MULIB_IMPL_AES_GCM
#define MULIB_IMPL_DER
#define MULIB_IMPL_SIGNATURE
#define MULIB_IMPL_X509
#define MULIB_IMPL_BASE64
#define MULIB_IMPL_TLS
