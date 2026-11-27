# Embedded standard library modules

Select the standard library for a compiler session **explicitly** before creating
a workspace or compilation. For a relocated On1x installation, pass the
directory containing its standard library `On1xfile` (normally
`<prefix>/share/on1x/stdlib`):

```c
#include <On1xSDK-Options.h>
#include <string.h>

On1xSDK_Status select_stdlib(On1xSDK_Options *options, const char *root) {
    /* Add this first, before ordinary library paths. */
    On1xSDK_Status status = on1x_sdk_options_add_search_path(options, root, strlen(root));
    if (status != ON1X_SDK_OK) return status;
    return on1x_sdk_options_set_boolean(options, ON1X_SDK_OPTION_FIRST_SEARCH_PATH_IS_STDLIB, 1);
}
```

The selected path is routed separately from ordinary library search paths. To
remove the selection, set the option to `0` or clear all search paths.
Compilation and workspace creation snapshot the options; changing the caller's
options does not retarget an existing snapshot.
The embedded compiler does not discover trust roots through environment
variables or the host executable's installation path.

Physical compilation goes through the same C++ loader that resolves standard
identities and legacy aliases, validates interface declarations, and seals the
selected module boundary. Native builds use the private bundled ASDL checks at
the loader's canonical entry points; the bootstrap build does not require Lua.
There is no SDK ASDL schema or Lua API. The C ABI exposes the existing immutable
dependency and candidate views, not signature internals, representation pointers,
or a checked certificate. An opaque load is unsupported rather than exposing
its representation through these full snapshots. Dependencies list physical
implementation snapshots once in dependency order, not non-executable
interface files. Refresh reloads and revalidates the graph; it does not reuse a
successful interface check.

Token and source-only parsing still do not resolve modules. Compilation does
not execute source PPX or grant foreign capabilities. Its AST remains a
candidate and cannot be promoted by `on1x_sdk_elaborate`. Unit-v1's wire
contract is unchanged.

### Canonical graph observations (v1)

Set `ON1X_SDK_OPTION_CAPTURE_MODULE_GRAPH` before creating a workspace or
compilation. Call `on1x_sdk_compilation_graph_json_v1` with `logical` 0
(import requests) or 1 (module dependencies). On success the returned UTF-8
bytes and byte count belong to the compilation until release. Refresh returns
a new compilation; it does not change previous bytes. Source overrides are
captured in the same loader transaction as the frontend AST. The existing
`on1x_sdk_compilation_dependency_count` counts physical sources. Virtual-only
entries and opaque frontend snapshots are unsupported. An observation does
not establish type checking or grant permission to compile or execute.
