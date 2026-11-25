FROM: coordinator
TO: W4
SUBJECT: CTest fixture root for the logic test
STATUS: blocker reproduced under strict Clang ASan/UBSan
DEPENDS: CMake mulib_register_test
INTERFACE: mulib_test-elvi-script-logic
BODY:
CTest isolates the working directory; the logic suite reads the canonical Lua file and expects the repository root argv[1]. Please add it to the source-directory branch of mulib_register_test along with the legacy tests. My other nine focused CTest suites passed, including the full integration case. The Make check executes from repo root and needs no argv for this suite.
