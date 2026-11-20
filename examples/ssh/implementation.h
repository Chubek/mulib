/* Include in only one translation unit per executable; link OpenSSL libcrypto. */
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_ENTROPY
#define MULIB_IMPL_AES_GCM
#define MULIB_IMPL_X25519
#define MULIB_IMPL_ED25519
#define MULIB_IMPL_SSHWIRE
#define MULIB_IMPL_SSHPROTO
#define MULIB_IMPL_SSHPACKET
#define MULIB_IMPL_SSHCRYPTO
#define MULIB_IMPL_SSHKEY
#define MULIB_IMPL_SSHKEX
#define MULIB_IMPL_SSHTRANSPORT
#define MULIB_IMPL_SSHAUTH
#define MULIB_IMPL_SSHCHANNEL
#define MULIB_IMPL_SSHCONNECTION
#include "mulib-sshconnection.h"
#include "mulib-sshclient.h"
