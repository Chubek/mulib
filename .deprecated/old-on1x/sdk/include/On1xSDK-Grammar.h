#ifndef ON1X_SDK_GRAMMAR_H
#define ON1X_SDK_GRAMMAR_H

#include "On1xSDK-Compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_GrammarInfo {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t grammar_version;
    uint32_t ppx_schema_version;
    size_t max_source_bytes;
    size_t max_syntax_depth;
    size_t max_ppx_wire_bytes;
    size_t max_ppx_wire_nodes;
    const char *scanner_profile;
    size_t scanner_profile_size;
} On1xSDK_GrammarInfo;

On1xSDK_Status on1x_sdk_grammar_info(On1xSDK_GrammarInfo *out_info);

#ifdef __cplusplus
}
#endif

#endif
