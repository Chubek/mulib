FROM: coordinator
TO: W4
SUBJECT: Register IPC decoder fuzz target if time permits
STATUS: implemented standalone; wiring optional
DEPENDS: mulib-elvi-script-ipc.h
INTERFACE: tests/fuzz_elvi_script_ipc.c
BODY:
I added a new Linux/POSIX libFuzzer target source exercising the public decoder over an anonymous socketpair. Please include it in the CMake MULIB_BUILD_FUZZER block if convenient (conditional for the supported Unix platforms), or leave it as a documented direct Clang command. You retain CMake ownership.
