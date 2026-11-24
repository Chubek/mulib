#!/usr/bin/env python3
"""Refresh (or, by default, check) PrettyDoc's Unicode scalar width ranges."""
import argparse
from pathlib import Path
import unicodedata as ucd


VERSION = "16.0.0"
HEADER = Path(__file__).resolve().parents[1] / "kernels/mulib-prettydoc.h"
BEGIN = "/* BEGIN GENERATED WIDTH TABLE */\n"
END = "/* END GENERATED WIDTH TABLE */\n"


def ranges():
    rows = []
    for cp in range(0x110000):
        ch = chr(cp)
        category = ucd.category(ch)
        # Don't use the East Asian Width defaults for unassigned scalars.
        width = (0 if category in {"Mn", "Me", "Cf"} else
                 2 if category not in {"Cn", "Cs"} and
                 ucd.east_asian_width(ch) in {"W", "F"} else 1)
        if width == 1:
            continue
        if rows and rows[-1][1] == cp - 1 and rows[-1][2] == width:
            rows[-1] = rows[-1][0], cp, width
        else:
            rows.append((cp, cp, width))
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    if ucd.unidata_version != VERSION:
        parser.error(f"Unicode database {VERSION} required, found {ucd.unidata_version}")
    before, table = HEADER.read_text().split(BEGIN)
    _, after = table.split(END)
    rows = [f"{{0x{lo:x},0x{hi:x},{width}}}," for lo, hi, width in ranges()]
    body = "\n".join(" " + " ".join(rows[i:i + 4]) for i in range(0, len(rows), 4))
    new = (before + BEGIN + f"/* Unicode {VERSION}; see tests/generate_pretty_width.py. */\n"
           "static const struct {uint32_t first,last;unsigned width;} mulib_pretty_widths_[]={\n"
           + body + "\n};\n" + END + after)
    if args.write:
        HEADER.write_text(new)
    elif new != HEADER.read_text():
        parser.error("PrettyDoc width table is stale; run with --write")


if __name__ == "__main__":
    main()
