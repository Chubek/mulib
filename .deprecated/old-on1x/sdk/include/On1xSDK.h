#ifndef ON1X_SDK_H
#define ON1X_SDK_H

/* Shared ABI 2 contracts and the frozen ABI-v1 compatibility types. */
#include "On1xSDK-Types.h"
#include "On1xSDK-ABI.h"
#include "On1xSDK-Version.h"
#include "On1xSDK-Status.h"
#include "On1xSDK-Allocator.h"
#include "On1xSDK-String.h"
#include "On1xSDK-Span.h"
#include "On1xSDK-Cancellation.h"
#include "On1xSDK-Budget.h"
#include "On1xSDK-Capability.h"
#include "On1xSDK-Context.h"
#include "On1xSDK-Handle.h"
#include "On1xSDK-Diagnostic.h"
#include "On1xSDK-Host.h"
#include "On1xSDK-Lifecycle.h"
#include "On1xSDK-Event.h"
#include "On1xSDK-Metadata.h"
#include "On1xSDK-Configuration.h"
#include "On1xSDK-Provenance.h"
#include "On1xSDK-Serialization.h"
#include "On1xSDK-Transport.h"
#include "On1xSDK-SharedMemory.h"
#include "On1xSDK-Sandbox.h"

/* Registries, catalogues, extensions, and plugin execution profiles. */
#include "On1xSDK-Extension.h"
#include "On1xSDK-Registry.h"
#include "On1xSDK-Catalogue.h"
#include "On1xSDK-Plugin.h"
#include "On1xSDK-PluginDependency.h"
#include "On1xSDK-PluginDiscovery.h"
#include "On1xSDK-PluginEvent.h"
#include "On1xSDK-PluginHost.h"
#include "On1xSDK-PluginManifest.h"
#include "On1xSDK-PluginProtocol.h"
#include "On1xSDK-PluginSignature.h"
#include "On1xSDK-PluginTrust.h"
#include "On1xSDK-PluginWorker.h"

/* Canonical compiler and frontend adapters. */
#include "On1xSDK-Compiler.h"
#include "On1xSDK-Session.h"
#include "On1xSDK-Options.h"
#include "On1xSDK-Source.h"
#include "On1xSDK-SourceMap.h"
#include "On1xSDK-Dependency.h"
#include "On1xSDK-Compilation.h"
#include "On1xSDK-Incremental.h"
#include "On1xSDK-Cache.h"
#include "On1xSDK-Workspace.h"
#include "On1xSDK-VFS.h"
#include "On1xSDK-Lexer.h"
#include "On1xSDK-Token.h"
#include "On1xSDK-Parser.h"
#include "On1xSDK-Grammar.h"
#include "On1xSDK-AST.h"
#include "On1xSDK-ASTBuilder.h"
#include "On1xSDK-ASTVisitor.h"
#include "On1xSDK-Syntax.h"
#include "On1xSDK-Attribute.h"
#include "On1xSDK-Directive.h"
#include "On1xSDK-Macro.h"
#include "On1xSDK-Hygiene.h"
#include "On1xSDK-PPX.h"
#include "On1xSDK-Staging.h"
#include "On1xSDK-Elaborator.h"
#include "On1xSDK-Frontend.h"

/* Canonical semantic, HIR, and MIR inspection and candidate APIs. */
#include "On1xSDK-SemanticTypes.h"
#include "On1xSDK-Type.h"
#include "On1xSDK-HIR.h"
#include "On1xSDK-MIR.h"
#include "On1xSDK-IRSerialize.h"
#include "On1xSDK-IRTransaction.h"
#include "On1xSDK-Semantics.h"

/* Revision-aware analysis and transactional transformation. */
#include "On1xSDK-Analyzer.h"
#include "On1xSDK-Transformer.h"
#include "On1xSDK-AnalysisManager.h"
#include "On1xSDK-Query.h"
#include "On1xSDK-CFG.h"
#include "On1xSDK-Dominator.h"
#include "On1xSDK-Loop.h"
#include "On1xSDK-Liveness.h"
#include "On1xSDK-EffectAnalysis.h"
#include "On1xSDK-Pass.h"
#include "On1xSDK-Pipeline.h"
#include "On1xSDK-PassInstrumentation.h"
#include "On1xSDK-PassManager.h"
#include "On1xSDK-TranslationValidation.h"

/* Backends, target descriptions, artifacts, and authorized toolchains. */
#include "On1xSDK-Backend.h"
#include "On1xSDK-Target.h"
#include "On1xSDK-TargetMachine.h"
#include "On1xSDK-DataLayout.h"
#include "On1xSDK-CallingConvention.h"
#include "On1xSDK-ObjectABI.h"
#include "On1xSDK-MachineIR.h"
#include "On1xSDK-Artifact.h"
#include "On1xSDK-Toolchain.h"
#include "On1xSDK-ToolchainResolver.h"
#include "On1xSDK-AOT.h"
#include "On1xSDK-JIT.h"

/* Structured foreign descriptions and explicit host admission. */
#include "On1xSDK-Contract.h"
#include "On1xSDK-FFI.h"
#include "On1xSDK-FFIDescriptor.h"
#include "On1xSDK-ForeignType.h"
#include "On1xSDK-CDecl.h"
#include "On1xSDK-ContractCheck.h"
#include "On1xSDK-Exotract.h"
#include "On1xSDK-ExoML.h"
#include "On1xSDK-ExoLib.h"
#include "On1xSDK-ContractReview.h"
#include "On1xSDK-KernelBind.h"
#include "On1xSDK-KernelDiscovery.h"
#include "On1xSDK-Marshalling.h"
#include "On1xSDK-ForeignResource.h"
#include "On1xSDK-ForeignCallback.h"
#include "On1xSDK-ForeignModule.h"
#include "On1xSDK-BindingGenerator.h"

/* ABI-v1 metadata plus ABI-2 project and developer-tool services. */
#include "On1xSDK-Module.h"
#include "On1xSDK-CLIOpt.h"
#include "On1xSDK-ExecEnv.h"
#include "On1xSDK-Project.h"
#include "On1xSDK-Manifest.h"
#include "On1xSDK-BuildGraph.h"
#include "On1xSDK-PackageResolver.h"
#include "On1xSDK-Process.h"
#include "On1xSDK-Reproducibility.h"
#include "On1xSDK-Formatter.h"
#include "On1xSDK-CodeAction.h"
#include "On1xSDK-Command.h"
#include "On1xSDK-DiagnosticCatalog.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Entry point for linked or isolated shared-library providers. Define it in the
 * extension, not the SDK, and return a descriptor with static lifetime. */
const On1xSDK_Extension *on1x_sdk_linked_extension(void);

#ifdef __cplusplus
}
#endif

#endif
