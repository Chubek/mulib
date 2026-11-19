# SDK language bindings

The supported bindings in this stage are:

- `On1xSDK.hpp`: C++17 move-only RAII owners for manifests, buffers and build
  graphs, using a caller-owned allocator domain.
- `python/on1x_sdk.py`: executable CPython `ctypes` access to manifest parsing
  and path normalization. Set `ON1X_SDK_PROJECT_LIBRARY` when the shared library
  is not in the platform loader path.

Both wrappers call the implemented C ABI 2 subset; neither loads extension code.
Other language bindings are intentionally absent until they can be compiled and
executed in the supported environment.

Python `Manifest` owners reject copying and serialization. Close them explicitly
or use a `with` block; borrowed properties must not outlive that owner.

After installation, C and C++ consumers use:

```cmake
find_package(On1xSDK 2 CONFIG REQUIRED)
target_link_libraries(my_project_tool PRIVATE On1x::Project)
```

The Python module is installed under the package library directory and locates
the relocated `libOn1xSDKProject` beside that tree. An explicit
`ON1X_SDK_PROJECT_LIBRARY` always takes precedence.
