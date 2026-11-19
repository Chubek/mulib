/* Signed multipart bytes only; no dependency on ZeroMQ or a running kernel. */
#define MULIB_IMPL_AUTHCRYPTO
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#include "mulib-jupyter-wire.h"
#include <assert.h>

#define PART(s) { (const unsigned char *)(s), sizeof(s) - 1 }

int main(void)
{
    /* Expected signature from Python hmac.digest over exactly these four JSON frames. */
    static const char signature[] =
        "5f8c7badbb7af617ec7006d49fc232da1de9cd95102967930c202f0712d4a595";
    mulib_jupyter_part parts[] = {
        PART("client"), PART("<IDS|MSG>"), PART(signature),
        PART("{\"msg_id\":\"fixture\",\"msg_type\":\"kernel_info_request\"}"),
        PART("{}"), PART("{\"note\":\"raw bytes\"}"), PART("{ }"),
        PART("\0binary")
    };
    enum { count = sizeof parts / sizeof parts[0] };
    size_t bytes = 0;
    for (size_t i = 0; i < count; ++i)
        bytes += parts[i].n;

    mulib_jupyter_hmac_key key = {"fixture\0key", 11}, wrong = {"fixture", 7};
    mulib_jupyter_wire wire = {0};
    assert(!mulib_jupyter_parse(parts, count, bytes - 1, &wire));
    assert(!wire.identities);
    assert(mulib_jupyter_parse(parts, count, bytes, &wire));
    assert(wire.identities == 1 && wire.identity == parts);
    assert(wire.nbuf == 1 && wire.buffers == parts + 7);
    assert(mulib_jupyter_verify(&wire, mulib_jupyter_hmac_sha256, &key, 0));
    assert(!mulib_jupyter_verify(&wire, mulib_jupyter_hmac_sha256, &wrong, 0));
    assert(!mulib_jupyter_verify(&wire, NULL, NULL, 1));
    char signed_hex[65];
    assert(mulib_jupyter_sign(wire.json, mulib_jupyter_hmac_sha256, &key, signed_hex));
    assert(!strcmp(signed_hex, signature));

    size_t json_bytes = 0;
    for (size_t i = 0; i < 4; ++i)
        json_bytes += wire.json[i].n;
    mulib_json_t *json[4] = {0};
    assert(!mulib_jupyter_decode_json(&wire, json_bytes - 1, json));
    assert(mulib_jupyter_decode_json(&wire, json_bytes, json));
    assert(!strcmp(mulib_json_text(mulib_json_get(json[0], "msg_id"), NULL), "fixture"));
    for (size_t i = 0; i < 4; ++i) {
        assert(mulib_json_typeof(json[i]) == MULIB_JSON_OBJECT);
        mulib_json_destroy(json[i]);
        json[i] = NULL;
    }
    mulib_jupyter_wire malformed = wire;
    malformed.json[3] = (mulib_jupyter_part)PART("[ ]");
    assert(!mulib_jupyter_decode_json(&malformed, json_bytes, json));
    malformed.json[3] = (mulib_jupyter_part)PART("{\0}");
    assert(!mulib_jupyter_decode_json(&malformed, json_bytes, json));
    assert(!json[0] && !json[1] && !json[2] && !json[3]);

    char altered_signature[sizeof signature];
    memcpy(altered_signature, signature, sizeof signature);
    altered_signature[63] = '4';
    wire.signature.p = (const unsigned char *)altered_signature;
    assert(!mulib_jupyter_verify(&wire, mulib_jupyter_hmac_sha256, &key, 1));

    parts[7] = (mulib_jupyter_part)PART("other");
    assert(mulib_jupyter_parse(parts, count, bytes, &wire));
    assert(mulib_jupyter_verify(&wire, mulib_jupyter_hmac_sha256, &key, 0));
    parts[6] = (mulib_jupyter_part)PART("{}");
    assert(mulib_jupyter_parse(parts, count, bytes, &wire));
    assert(!mulib_jupyter_verify(&wire, mulib_jupyter_hmac_sha256, &key, 1));
    parts[2] = (mulib_jupyter_part){0};
    assert(mulib_jupyter_parse(parts, count, bytes, &wire));
    assert(!mulib_jupyter_verify(&wire, mulib_jupyter_hmac_sha256, &key, 1));
    assert(!mulib_jupyter_verify(&wire, NULL, NULL, 0));
    assert(mulib_jupyter_verify(&wire, NULL, NULL, 1));

    parts[4] = (mulib_jupyter_part){NULL, 1};
    assert(!mulib_jupyter_parse(parts, count, bytes, &wire));
    wire.json[0].p = NULL;
    assert(!mulib_jupyter_sign(wire.json, mulib_jupyter_hmac_sha256, &key, signed_hex));
    return 0;
}
