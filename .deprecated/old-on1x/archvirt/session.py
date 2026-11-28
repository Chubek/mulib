#!/usr/bin/env python3
"""Adapt on1xc's ArchVirt session arguments to the native MuLIB console tool."""

import argparse
import os
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", required=True)
    parser.add_argument("--repl", required=True)
    parser.add_argument("--image")
    commands = parser.add_subparsers(dest="operation", required=True)
    connect = commands.add_parser("connect")
    connect.add_argument("id")
    connect.add_argument("--detached", action="store_true")
    connect.add_argument("--param", action="append", default=[])
    stop = commands.add_parser("stop")
    stop.add_argument("id")
    args = parser.parse_args()
    for path in (args.runner, args.repl, args.image):
        if path is not None and not os.path.isabs(path):
            parser.error("runner, console tool, and guest paths must be absolute")
    operation = "detach" if args.operation == "connect" and args.detached else args.operation
    command = [args.repl, operation, args.id]
    if args.operation == "connect":
        if args.image:
            command.extend((args.runner, args.image))
        for parameter in args.param:
            command.extend(("--param", parameter))
    try:
        os.execv(args.repl, command)
    except OSError as error:
        print(f"On1x ArchVirt session: cannot start console tool: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
