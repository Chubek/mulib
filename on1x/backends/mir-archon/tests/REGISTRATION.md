# Worker 6 test registration (for build owner)

Add `on1x_archon_native_fixture` from `tests/NativeFixture.cpp`, linked PRIVATE
to `on1x_archon_bridge`, with C++20 and `-Wall -Wextra -Wpedantic -Werror`.

Register `on1x.backends.mir_archon_native_differential` running:

```
${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/native_differential.py
  --fixture-tool $<TARGET_FILE:on1x_archon_native_fixture>
  --archon-tool $<TARGET_FILE:on1x-archon-mir>
  --on1xc $<TARGET_FILE:on1xc>
  --cc ${CMAKE_C_COMPILER}
  --runtime $<TARGET_FILE:on1x_runtime>
  --scalar-abi $<TARGET_FILE:on1x_archon_scalar_abi>
  --runtime-include ${PROJECT_SOURCE_DIR}/runtime
  --work ${CMAKE_CURRENT_BINARY_DIR}/native-differential
```

Register `on1x.backends.mir_archon_native_limits` running
`on1x_archon_native_fixture --checks`; both tests require a real generated
AMD64 SysV backend and native Linux AMD64 execution. Set differential timeout
to 180 seconds. No CMake files are owned by this worker.

When `ON1X_ARCHON_ENABLE_SDK_ADAPTER` is enabled, also pass
`--sdk-target ${ON1X_ARCHON_TARGET_TRIPLE}` to the differential test. This
executes both ordinary and `--optimize-ir` SDK-native production for every
source fixture. Optimized MIR C11 and the 588-result seeded Python unsigned
oracle are always checked.

Benchmark workers may reuse `fixtures/*.on1x`, but trap fixtures intentionally
exit unsuccessfully and are correctness inputs, not timing workloads.

Observed during combined-build integration: new `Tool.cpp` requires
`ON1X_ARCHON_TARGET_TRIPLE`, but `on1x-archon-mir.dir/flags.make` had no compile
definitions. This makes even default native emission fail with
`target mismatch: requested , configured `. Apply the configured target triple
definition to `on1x-archon-mir` as well as the SDK adapter. Minimal repro:
`on1x-archon-mir --print-target`. The build owner subsequently fixed this;
the combined-build tool now reports `x86_64-unknown-linux-gnu`.
