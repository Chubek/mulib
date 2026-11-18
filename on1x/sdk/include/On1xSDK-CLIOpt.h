/* An opt-in parser for registered long options; no dispatch or argv mutation. */
#ifndef ON1X_SDK_CLIOPT_H
#define ON1X_SDK_CLIOPT_H

#include "On1xSDK-Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_MAX_CLIOPT_HELP_BYTES 1024u
#define ON1X_SDK_MAX_CLIOPT_VALUE_BYTES 4096u

enum {
    ON1X_SDK_CLIOPT_FLAG = 1,
    ON1X_SDK_CLIOPT_I64 = 2,
    ON1X_SDK_CLIOPT_STRING = 3
};

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    const char *name;   /* No "--" prefix; [a-z][a-z0-9]*(-[a-z0-9]+)*. */
    uint32_t kind;
    const char *help;   /* Required, nonempty printable ASCII. */
} On1xSDK_CLIOpt;

typedef struct {
    const On1xSDK_CLIOpt *option; /* Registry view. */
    int64_t integer;             /* Set for I64; 1 for FLAG; otherwise 0. */
    const char *text;            /* STRING only: borrows the NUL-terminated token suffix. */
    size_t text_size;            /* Byte length of text, excluding the NUL. */
} On1xSDK_CLIOptValue;

On1xSDK_Status on1x_sdk_cliopt_register(On1xSDK_Registry *registry,
                                       const On1xSDK_CLIOpt *option);
const On1xSDK_CLIOpt *on1x_sdk_cliopt_find(const On1xSDK_Registry *registry,
                                          const char *name);
size_t on1x_sdk_cliopt_count(const On1xSDK_Registry *registry);
const On1xSDK_CLIOpt *on1x_sdk_cliopt_at(const On1xSDK_Registry *registry, size_t index);

/*
 * Parses exactly one token: --name for FLAG, --name=value for I64 or STRING.
 * I64 requires [-]digits in base ten and rejects overflow. STRING accepts an
 * empty value and preserves bytes literally, rejecting ASCII control bytes.
 * No shell parsing, callbacks, or changes to on1xc's command-line parser.
 * Unknown well-formed names return UNSUPPORTED, malformed values/syntax
 * return INVALID_ARGUMENT, and over-budget names/tokens/values return LIMIT.
 * Output is cleared on failure. Output storage must not overlap the token.
 */
On1xSDK_Status on1x_sdk_cliopt_parse(const On1xSDK_Registry *registry,
                                    const char *token, On1xSDK_CLIOptValue *out);

#ifdef __cplusplus
}
#endif

#endif
