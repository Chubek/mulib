#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#define MULIB_IMPL_ENTROPY
#define MULIB_IMPL_SHA
#define MULIB_IMPL_HKDF
#define MULIB_IMPL_X25519
#define MULIB_IMPL_AES_GCM
#define MULIB_IMPL_DER
#define MULIB_IMPL_SIGNATURE
#define MULIB_IMPL_X509
#define MULIB_IMPL_BASE64
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#include "mulib-x509.h"
#include "mulib-hkdf.h"
#include "mulib-x25519.h"
#include "mulib-aes-gcm.h"
