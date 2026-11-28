#!/usr/bin/env python3
# on1x: (On1xfile 1 (kind "ppx") (name "checks") (protocol "1"))
"""Standalone On1x checks PPX (source-compatible AST v1)."""
from _support.driver import serve

if __name__ == '__main__':
    serve('checks')
