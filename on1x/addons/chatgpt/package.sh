#!/usr/bin/env sh
# Packages only this addon, never repository data or credentials.
set -eu
exec python3 -B "$(dirname "$0")/package.py" "$@"
