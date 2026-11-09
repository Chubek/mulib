#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
CC=${CC:-cc}; CXX=${CXX:-c++}
mkdir -p tests/build
"$CC" -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude -c tests/cpp_impl.c -o tests/build/cpp_impl.o
"$CXX" -std=c++17 -Wall -Wextra -Wpedantic -Werror -Iinclude tests/cpp_consumer.cpp tests/build/cpp_impl.o -o tests/build/cpp_consumer
./tests/build/cpp_consumer
printf 'PASS: C11 implementation linked with C++17 consumer\n'
