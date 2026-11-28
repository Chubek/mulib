#ifndef ON1X_SDK_PROGRAM_INTERNAL_HPP
#define ON1X_SDK_PROGRAM_INTERNAL_HPP

#include "On1xSDK-Types.h"

#include <string_view>

namespace on1x { struct Unit; namespace mir { struct Module; } }
struct On1xSDK_Backend;
struct On1xSDK_Artifact;
struct On1xSDK_DiagnosticSinkV2;
struct On1xSDK_MIRSnapshot;

namespace on1x::sdk {

// Compiler bridge only: call after canonical MIR verification and serialization.
// Copies the versioned serialization, not the compiler's mutable IR objects.
// This wrapper does not parse or verify MIR and is not part of the installed ABI.
On1xSDK_Status make_verified_mir(std::string_view canonical_text,
                                 On1xSDK_Program **out) noexcept;
// Private, synchronous built-in views. Never exposed to extension providers.
On1xSDK_Status make_checked_hir(const Unit &, std::string_view source_name,
                                On1xSDK_Program **out) noexcept;
const Unit *checked_hir(const On1xSDK_Program *, std::string_view *source_name) noexcept;
On1xSDK_Status make_builtin_mir(const mir::Module &, On1xSDK_Program **out) noexcept;
const mir::Module *builtin_mir(const On1xSDK_Program *) noexcept;
On1xSDK_Status register_checked_hir_backend(On1xSDK_Registry *, const On1xSDK_Backend *) noexcept;
On1xSDK_Status run_builtin_backend(const On1xSDK_Registry *, const char *,
                                   const On1xSDK_Program *, On1xSDK_Artifact **) noexcept;
// Structured SDK 2 facet; only the compiler host schedules a verified snapshot.
On1xSDK_Status run_analyzer_v2(const On1xSDK_Registry *, std::string_view name,
                               const On1xSDK_MIRSnapshot *,
                               const On1xSDK_DiagnosticSinkV2 &) noexcept;

} // namespace on1x::sdk

#endif
