#!/bin/sh
set -eu
mkdir -p /tmp/muweb-tests
for f in tests/test-*.c; do
  n=$(basename "$f" .c)
  cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude "$f" -o "/tmp/muweb-tests/$n"
  "/tmp/muweb-tests/$n"
done
echo "All MuWeb module tests passed."
