# Testing the MuLIB networking add-on

`CC=cc ./tests/run.sh` builds and runs the repository's CMake/CTest suites,
including the networking tests. `BUILD_DIR` and `JOBS` select the build directory
and parallelism. Use a separate build directory when changing compilers.
The networking cases include **symbol/link smoke tests for every module**,
packet codec checks, an offline stack datapath, D-Bus mock replies, and
`test_network_hardening.c` / `test_netlink_transactions.c` regressions. They do
not change routes, interfaces, DNS, Wi-Fi or firewall rules.

For Clang and sanitizers:
`CC=clang CXX=clang++ BUILD_DIR=tests/build/clang SANITIZERS=ON ./tests/run.sh`.
`./tests/check-cxx.sh` checks a separately compiled C11 implementation with a
C++17 consumer; CTest also includes that case. `python3 tools/check.py` adds
isolated-header and inventory checks. These checks are not a substitute for
fuzzing, privileged namespace integration, or real-service compatibility tests.
