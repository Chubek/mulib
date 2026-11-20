#!/usr/bin/env python3
"""Build a SQLite terminal reference database from local documents.

Usage: python3 tools/mk-termdb.py OUTPUT [--source-dir DIR]

Version 1 uses case-sensitive names. The `ansi` profile is the default;
`xterm` adds explicitly selected DEC/xterm modes and legacy key encodings.
Templates take integer parameters only: CUP/cup take zero-based (row, column),
move takes zero-based (column, row), and ED, EL and SGR take one selector each.

`termdb_sources` archives exact bytes of each regular file; `termdb_facts`
contains documentation, not executable templates. Code and fact locations
use relative POSIX paths and 1-based LF-separated line numbers. A warning at
line 0 applies to the whole document. Unknown, malformed and binary documents
can still be retained. If a curated entry's primary citation cannot be
verified, that entry is omitted and the missing citation is reported.

No timestamps or absolute paths are stored. Files and rows are ordered
deterministically: identical inputs with the same SQLite build produce
byte-identical databases. The output path itself is not a source. The build
replaces the output atomically; do not replace a database in active use. For
multiple builds, keep the output files outside the source directory.
"""

import argparse
from collections import defaultdict
from contextlib import closing
import hashlib
import os
from pathlib import Path
import re
import sqlite3
import stat
import sys
import tempfile
from typing import NamedTuple


DEFAULT_SOURCE = Path(__file__).resolve().parent.parent / ".sot" / "term"
ECMA = "ecma-48-code-sequence.txt"
VT = "vt100.txt"
TI = "Terminfo.man"
TC = "Termcap.man"
KITTY = "kitty-escape-seq.txt"
PROFILE = "termscript-profile.md"

SCHEMA = """
CREATE TABLE termdb_meta ("key" TEXT, value TEXT);
CREATE TABLE termdb_codes (name TEXT, profile TEXT, template TEXT, arity INTEGER, description TEXT, source TEXT, line INTEGER);
CREATE TABLE termdb_sources (name TEXT, sha256 TEXT, content BLOB, status TEXT);
CREATE TABLE termdb_facts (source TEXT, line INTEGER, kind TEXT, name TEXT, notation TEXT, description TEXT);
CREATE TABLE termdb_code_refs (name TEXT, profile TEXT, source TEXT, line INTEGER, kind TEXT, fact_name TEXT, notation TEXT);
CREATE TABLE termdb_warnings (source TEXT, line INTEGER, message TEXT);
"""


class Fact(NamedTuple):
    line: int
    kind: str
    name: str
    notation: str
    description: str


def normalized(text):
    return " ".join(text.split())


def ecma_facts(lines):
    section = ""
    for i, raw in enumerate(lines):
        text = raw.strip()
        if re.fullmatch(r"8\.3\.[0-9]+", text):
            section = text
        match = re.fullmatch(r"([A-Z][A-Z0-9]+) - (.+)", text)
        if match and section:
            # The stand-alone heading is followed by the notation and
            # representation; do not treat the table of contents as definitions.
            for following in lines[i + 1:i + 7]:
                if not following.strip() or not following.startswith(("Notation:", "Representation:")):
                    break
                if following.startswith("Representation:"):
                    name, title = match.groups()
                    yield Fact(i + 1, "ecma-control", name,
                               normalized(following.partition(":")[2]), f"{title} (ECMA-48 {section})")
                    break
            section = ""


def vt_facts(lines):
    for i, text in enumerate(lines):
        match = re.fullmatch(r"([A-Z][A-Z0-9]+) [–—] (.+)", text)
        if match:
            name, desc = match.groups()
            notation = ""
            for following in lines[i + 1:i + 6]:
                if following.startswith("  ESC "):
                    notation = normalized(re.split(r" {2,}", following.strip(), maxsplit=1)[0])
                    break
            yield Fact(i + 1, "vt-control", name, notation, desc)
        arrow = re.fullmatch(r"  (Up|Down|Right|Left) +ESC [ABCD] +(ESC \[ [ABCD]) +(ESC O [ABCD])\s*", text)
        if arrow:
            direction, normal, application = arrow.groups()
            yield Fact(i + 1, "vt-key", direction, normal, f"{direction} arrow, normal cursor keys")
            yield Fact(i + 1, "vt-key", direction, application, f"{direction} arrow, application cursor keys")


def terminfo_facts(lines):
    sample = False
    for i, text in enumerate(lines, 1):
        # Keep the capability documentation and the ANSI sample distinct:
        # names can have other encodings on other terminals.
        match = re.fullmatch(r" {8}([a-z][a-z0-9_]*) {2,}(\w+) +(\S{2}) {2,}(\S.*)", text)
        if match:
            long_name, short_name, termcap, desc = match.groups()
            yield Fact(i, "terminfo-cap", short_name, termcap, f"{long_name}: {desc.rstrip()}")
        if text.strip().startswith("ansi|"):
            sample = True
        elif not text.strip():
            sample = False
        elif sample:
            # Match simple complete assignments only, not continued values.
            for name, value in re.findall(r"(?:^\s+|(?<!\\), )([a-z][a-z0-9]*)=([^,\s]+)(?<!\\)(?=,)", text):
                yield Fact(i, "terminfo-ansi", name, value, "ANSI sample capability")


def termcap_facts(lines):
    kind = ""
    for i, text in enumerate(lines, 1):
        section = re.fullmatch(r"   (Boolean|Numeric|String) capabilities", text)
        if section:
            kind = "termcap-" + section[1].lower()
        elif not text.strip():
            kind = ""
        elif kind:
            match = re.fullmatch(r" {7}([a-zA-Z0-9@!#%&*<>]{2}) {2,}(\S.*)", text)
            if match:
                yield Fact(i, kind, match[1], match[1], match[2].rstrip())


def console_facts(lines):
    section = ""
    for i, text in enumerate(lines, 1):
        if text == "       ESC- but not CSI-sequences":
            section = "ESC"
        elif text == "       ECMA-48 CSI sequences":
            section = "CSI"
        elif text == "       ECMA-48 Select Graphic Rendition":
            section = "SGR"
        elif text == "       ECMA-48 Mode Switches":
            break
        elif section == "SGR":
            match = re.fullmatch(r" {7}([0-9]+(?:\.\.[0-9]+)?) {2,}(\S.*)", text)
            if match:
                yield Fact(i, "linux-sgr", match[1], f"CSI {match[1]} m", match[2].rstrip())
        elif section:
            seq = r"ESC \S(?: \S)?" if section == "ESC" else r"[@-~]"
            match = re.fullmatch(rf" {{7}}({seq}) {{2,}}([A-Z][A-Z0-9]+) {{2,}}(\S.*)", text)
            if match:
                notation, name, desc = match.groups()
                yield Fact(i, "linux-console", name, f"CSI ... {notation}" if section == "CSI" else notation, desc.rstrip())


def parser_facts(lines):
    active = False
    for i, text in enumerate(lines):
        if text == "State Definitions":
            active = True
        if active and re.fullmatch(r"ground|(?:escape|csi|dcs|osc|sos/pm/apc)(?: [a-z]+)?", text):
            # Include the first explanatory paragraph.
            paragraph = []
            for following in lines[i + 2:]:
                if not following.startswith("    "):
                    break
                paragraph.append(following.strip())
            if paragraph:
                yield Fact(i + 1, "parser-state", text, "", " ".join(paragraph))


def kitty_facts(lines):
    active = False
    for i, text in enumerate(lines, 1):
        if text.startswith("Legacy functional keys"):
            active = True
        elif active and text.startswith("  : Legacy functional encoding"):
            break
        elif active:
            row = re.fullmatch(r" {2}([A-Z0-9_]+) +([a-z0-9,-]+) +((?:CSI|SS3) .+)", text)
            if row:
                # Comma-separated entries give separate normal/app encodings.
                names, notations = row[2].split(","), row[3].split(",")
                if len(names) == len(notations):
                    for name, notation in zip(names, notations):
                        if name != "-":
                            yield Fact(i, "legacy-key", name, normalized(notation), f"{row[1]} key (legacy encoding)")


def wezterm_facts(lines):
    section = ""
    for i, text in enumerate(lines, 1):
        # Neither web markup nor vendor command examples are evaluated.
        heading = re.match(r"#{1,6} (.+?)\[¶\]", text)
        if heading:
            section = heading[1]
            yield Fact(i, "wezterm-section", section, "", section)
        elif section in ("C0 Control Codes", "C1 Control Codes"):
            row = re.fullmatch(r" {2}(\S+(?: \S+)?) +0x[0-9a-fA-F]{2} +([A-Z][A-Z0-9]+) +(.+)", text)
            if row:
                yield Fact(i, "wezterm-control", row[2], row[1], row[3].rstrip())
        elif section == "Graphic Rendition (SGR)":
            row = re.fullmatch(r" {2}([0-9]+(?::[0-9]+)?) +([A-Za-z]+) {2,}(\S.*)", text)
            if row:
                yield Fact(i, "wezterm-sgr", row[1], f"CSI {row[1]} m", f"{row[2]}: {row[3].rstrip()}")
        elif section == "Operating System Command Sequences":
            row = re.match(r" {2}([0-9]+|[Ll]) {2,}(\S.*?) {2,}", text)
            if row:
                yield Fact(i, "wezterm-osc", row[1], f"OSC {row[1]}", row[2])


def profile_facts(lines):
    modes = False
    for i, text in enumerate(lines, 1):
        if text == "The explicit xterm profile adds DECSET/DECRST (`CSI ? Ps h` and `CSI ? Ps l`)":
            modes = True
        row = re.fullmatch(r"\| ([0-9]+) \| ([^|]+?) \|", text)
        if modes and row:
            yield Fact(i, "xterm-mode", row[1], f"CSI ? {row[1]} h/l", row[2])
        if text.startswith("reverse order. Bracketed paste input is delimited by `CSI 200 ~` and"):
            if i < len(lines) and lines[i].startswith("`CSI 201 ~`."):
                yield Fact(i, "xterm-input", "paste-start", "CSI 200 ~", "Bracketed paste opening delimiter")
                yield Fact(i + 1, "xterm-input", "paste-end", "CSI 201 ~", "Bracketed paste closing delimiter")


EXTRACTORS = {
    ECMA: ecma_facts, VT: vt_facts, TI: terminfo_facts, TC: termcap_facts,
    KITTY: kitty_facts, PROFILE: profile_facts, "console_commands.txt": console_facts,
    "dec_ansi_parser.txt": parser_facts, "wezterm-escape-sequences.txt": wezterm_facts,
}


class Ref(NamedTuple):
    source: str
    kind: str
    name: str
    notation: str = ""
    description: str = ""
    required: bool = False


CSI = "\x1b["


def catalog():
    """Only literals authored here contribute to executable templates."""
    cup = Ref(ECMA, "ecma-control", "CUP", "CSI Pn1;Pn2 04/08")
    ti_cup = Ref(TI, "terminfo-ansi", "cup", r"\E[%i%p1%d;%p2%dH")
    vt_cup = Ref(VT, "vt-control", "CUP", "ESC [ Pn ; Pn H")
    yield "ansi", ("CUP", "cup"), CSI + "%i%p1%d;%p2%dH", 2, "Cursor position (zero-based row, column)", (cup, ti_cup, vt_cup)
    yield "ansi", ("move",), CSI + "%i%p2%d;%p1%dH", 2, "Move cursor (zero-based column, row)", (ti_cup, cup, vt_cup)
    yield "ansi", ("CSI",), CSI, 0, "7-bit control sequence introducer (prefix)", (Ref(ECMA, "ecma-control", "CSI", "09/11 or ESC 05/11"),)
    yield "ansi", ("BEL", "bel", "bell"), "\x07", 0, "Audible bell", (Ref(ECMA, "ecma-control", "BEL", "00/07"),)
    for name, alias, letter, notation, desc in (
        ("CUU", "cuu", "A", "CSI Pn 04/01", "Move cursor up by a row count"),
        ("CUD", "cud", "B", "CSI Pn 04/02", "Move cursor down by a row count"),
        ("CUF", "cuf", "C", "CSI Pn 04/03", "Move cursor right by a column count"),
        ("CUB", "cub", "D", "CSI Pn 04/04", "Move cursor left by a column count"),
        ("ED", None, "J", "CSI Ps 04/10", "Erase display (0: to end, 1: from start, 2: entire display)"),
        ("EL", None, "K", "CSI Ps 04/11", "Erase line (0: to end, 1: from start, 2: entire line)"),
        ("SGR", None, "m", "CSI Ps... 06/13", "Select one graphic rendition attribute"),
    ):
        names = (name, alias) if alias else (name,)
        yield "ansi", names, CSI + "%p1%d" + letter, 1, desc, (Ref(ECMA, "ecma-control", name, notation),)

    for names, template, sample, desc in (
        (("clear",), CSI + "H" + CSI + "J", r"\E[H\E[J", "Home cursor and clear display"),
        (("home",), CSI + "H", r"\E[H", "Home cursor"),
        (("ed", "erase-below"), CSI + "J", r"\E[J", "Erase to end of display"),
        (("el", "erase-line"), CSI + "K", r"\E[K", "Erase to end of line"),
    ):
        yield "ansi", names, template, 0, desc, (Ref(TI, "terminfo-ansi", names[0], sample),)
    yield "ansi", ("sgr0", "reset"), CSI + "0m", 0, "Reset graphic rendition", (Ref(ECMA, "ecma-control", "SGR", "CSI Ps... 06/13"),)

    yield ("xterm", ("DECSTBM", "csr", "region"), CSI + "%i%p1%d;%p2%dr", 2,
           "Set scroll margins (zero-based top, bottom)",
           (Ref(VT, "vt-control", "DECSTBM", "ESC [ Pn; Pn r"), Ref(TI, "terminfo-cap", "csr")))
    for names, char, desc in (
        (("DECKPAM", "keypad-enter"), "=", "Enable application keypad"),
        (("DECKPNM", "keypad-leave"), ">", "Enable numeric keypad"),
        (("DECSC", "sc", "cursor-save"), "7", "Save cursor state"),
        (("DECRC", "rc", "cursor-restore"), "8", "Restore cursor state"),
    ):
        yield "xterm", names, "\x1b" + char, 0, desc, (Ref(VT, "vt-control", names[0], "ESC " + char),)

    for (on, off), mode, desc in (
        ((("cursor-show", "cnorm"), ("cursor-hide", "civis")), "25", "Show cursor"),
        ((("screen-enter", "smcup"), ("screen-leave", "rmcup")), "1049", "Alternate screen and saved cursor"),
        ((("paste-on",), ("paste-off",)), "2004", "Bracketed paste"),
    ):
        refs = (Ref(PROFILE, "xterm-mode", mode, f"CSI ? {mode} h/l", desc),)
        yield "xterm", on, CSI + "?" + mode + "h", 0, "Enable: " + desc, refs
        yield "xterm", off, CSI + "?" + mode + "l", 0, "Disable: " + desc, refs
    mouse = (Ref(PROFILE, "xterm-mode", "1000", "CSI ? 1000 h/l", "Button-event mouse tracking"),
             Ref(PROFILE, "xterm-mode", "1006", "CSI ? 1006 h/l", "SGR mouse encoding", required=True))
    yield "xterm", ("mouse-on",), CSI + "?1000h" + CSI + "?1006h", 0, "Enable SGR mouse tracking", mouse
    yield "xterm", ("mouse-off",), CSI + "?1006l" + CSI + "?1000l", 0, "Disable SGR mouse tracking", mouse
    for name, number in (("paste-start", "200"), ("paste-end", "201")):
        yield "xterm", (name,), CSI + number + "~", 0, "Bracketed paste input delimiter", (Ref(PROFILE, "xterm-input", name, f"CSI {number} ~"),)

    for direction, letter, short in (("up", "A", "kcuu1"), ("down", "B", "kcud1"), ("right", "C", "kcuf1"), ("left", "D", "kcub1")):
        name = "key-" + direction
        yield "ansi", (name,), CSI + letter, 0, f"{direction.title()} arrow input (normal)", (Ref(VT, "vt-key", direction.title(), "ESC [ " + letter),)
        yield "xterm", (name + "-app", short), "\x1bO" + letter, 0, f"{direction.title()} arrow input (application)", (
            Ref(VT, "vt-key", direction.title(), "ESC O " + letter), Ref(KITTY, "legacy-key", short, "SS3 " + letter), Ref(TI, "terminfo-cap", short))
    for number, code in enumerate(("P", "Q", "R", "S", "15", "17", "18", "19", "20", "21", "23", "24"), 1):
        short = f"kf{number}"
        if code.isdigit():
            template, notation = CSI + code + "~", f"CSI {code} ~"
        else:
            template, notation = "\x1bO" + code, "SS3 " + code
        yield "xterm", (short, f"key-f{number}"), template, 0, f"F{number} key input (legacy)", (
            Ref(KITTY, "legacy-key", short, notation), Ref(TI, "terminfo-cap", short))
    for name, short, code, desc in (
        ("key-insert", "kich1", "2", "Insert"),
        ("key-delete", "kdch1", "3", "Delete"),
        ("key-page-up", "kpp", "5", "Page Up"),
        ("key-page-down", "knp", "6", "Page Down"),
    ):
        yield "xterm", (name, short), CSI + code + "~", 0, f"{desc} key input (legacy)", (
            Ref(KITTY, "legacy-key", short, f"CSI {code} ~"), Ref(TI, "terminfo-cap", short))


PARAM = re.compile(r"%p([1-9])%d")
# This reviewed catalog needs only numeric decimal formatting, %i, CSI/SS3,
# four VT controls and BEL. Reject accidental use of other terminfo syntax.
TEMPLATE = re.compile(r"(?:\x1b\[(?:%i)?\??(?:[0-9;]|%p[1-9]%d)*[@-~]|\x1bO[A-Z]|\x1b[78=>]|\x07)+")


def validate_catalog():
    wanted = list(catalog())
    seen = set()
    for profile, names, template, arity, _, refs in wanted:
        if (profile not in ("ansi", "xterm") or not 0 <= arity <= 9 or
                not (template == CSI or TEMPLATE.fullmatch(template)) or
                set(map(int, PARAM.findall(template))) != set(range(1, arity + 1)) or
                ("%i" in template and (arity < 2 or template.count("%i") != 1)) or not refs):
            raise ValueError(f"invalid curated template: {profile}/{names[0]}")
        for name in names:
            if not re.fullmatch(r"[A-Za-z][A-Za-z0-9-]*", name) or (name, profile) in seen:
                raise ValueError(f"invalid or duplicate curated name: {profile}/{name}")
            seen.add((name, profile))
    return wanted


def source_paths(root, output, warnings):
    def onerror(error):
        raise error

    paths = []
    for directory, dirs, files in os.walk(root, onerror=onerror, followlinks=False):
        for item in sorted(dirs + files):
            path = Path(directory) / item
            if path == output:
                continue
            mode = path.lstat().st_mode
            if stat.S_ISREG(mode):
                paths.append((path.relative_to(root).as_posix(), path))
            elif not stat.S_ISDIR(mode):
                warnings.add((path.relative_to(root).as_posix(), 0, "skipped non-regular input (including symlinks)"))
    return sorted(paths)


def read_sources(root, output, warnings):
    sources, extracted = [], []
    for name, path in source_paths(root, output, warnings):
        blob = path.read_bytes()
        facts = []
        status = "unparsed"
        extractor = EXTRACTORS.get(path.name)
        try:
            text = blob.decode("utf-8-sig")
        except UnicodeDecodeError:
            status = "binary"
            warnings.add((name, 0, "not UTF-8; stored as an opaque BLOB"))
        else:
            if re.search(r"[\x00-\x08\x0b\x0e-\x1f\x7f-\x9f]", text):
                status = "binary"
                warnings.add((name, 0, "binary control bytes; stored as an opaque BLOB"))
            elif not extractor:
                warnings.add((name, 0, "no extractor for this document; stored verbatim"))
            else:
                facts = sorted(set(extractor([line.rstrip("\r") for line in text.split("\n")])))
                if facts:
                    status = "text"
                else:
                    warnings.add((name, 0, "no recognizable records; stored verbatim"))
        sources.append((name, hashlib.sha256(blob).hexdigest(), blob, status))
        extracted.extend((name, *fact) for fact in facts)
    return sources, extracted


def code_rows(extracted, warnings):
    index = defaultdict(list)
    for source, line, kind, name, notation, desc in extracted:
        index[source, kind, name].append((source, line, kind, notation, desc))

    def resolve(ref):
        hits = [row for row in index[ref.source, ref.kind, ref.name]
                if (not ref.notation or row[3] == ref.notation)
                and (not ref.description or row[4] == ref.description)]
        return hits[0] if hits else None

    rows, provenance = [], []
    for profile, names, template, arity, desc, references in validate_catalog():
        hits = [resolve(ref) for ref in references]
        missing = [ref for i, (ref, hit) in enumerate(zip(references, hits))
                   if hit is None and (i == 0 or ref.required)]
        for ref in missing:
            warnings.add((ref.source, 0, f"no matching {ref.kind} citation for {ref.name!r}; omitted {profile}/{names[0]}"))
        if missing:
            continue
        primary = hits[0]
        for name in names:
            rows.append((name, profile, template, arity, desc, primary[0], primary[1]))
            for ref, hit in zip(references, hits):
                if hit is not None:
                    provenance.append((name, profile, *hit[:3], ref.name, hit[3]))
    return sorted(rows, key=lambda row: (row[1], row[0])), sorted(set(provenance))


def build(root, output):
    if not root.is_dir():
        raise ValueError(f"source directory does not exist or is not a directory: {root}")
    if root in output.parents and (output.exists() or output.is_symlink()):
        # Permit rebuilding in place, but never overwrite a source document.
        if not output.is_symlink() and output.is_file():
            try:
                with closing(sqlite3.connect(output.as_uri() + "?mode=ro", uri=True)) as existing:
                    is_termdb = existing.execute("""SELECT value FROM termdb_meta WHERE "key" = 'schema_version'""").fetchall() == [("1",)]
            except sqlite3.Error:
                is_termdb = False
        else:
            is_termdb = False
        if not is_termdb:
            raise ValueError(f"refusing to replace a source document: {output}")
    warnings = set()
    sources, extracted = read_sources(root, output, warnings)
    rows, provenance = code_rows(extracted, warnings)

    fd, temp_name = tempfile.mkstemp(prefix=".termdb-", suffix=".sqlite3", dir=output.parent)
    try:
        os.close(fd)
        with closing(sqlite3.connect(temp_name)) as db:
            db.execute("PRAGMA page_size = 4096")
            db.execute("PRAGMA encoding = 'UTF-8'")
            db.execute("PRAGMA journal_mode = DELETE")
            db.execute("PRAGMA auto_vacuum = NONE")
            db.executescript("BEGIN;\n" + SCHEMA)
            db.execute("INSERT INTO termdb_meta VALUES ('schema_version', '1')")
            db.executemany("INSERT INTO termdb_codes VALUES (?,?,?,?,?,?,?)", rows)
            db.executemany("INSERT INTO termdb_sources VALUES (?,?,?,?)", sources)
            db.executemany("INSERT INTO termdb_facts VALUES (?,?,?,?,?,?)", extracted)
            db.executemany("INSERT INTO termdb_code_refs VALUES (?,?,?,?,?,?,?)", provenance)
            db.executemany("INSERT INTO termdb_warnings VALUES (?,?,?)", sorted(warnings))
            db.commit()
        os.replace(temp_name, output)
    finally:
        if os.path.exists(temp_name):
            os.unlink(temp_name)
    return sorted(warnings)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("output", type=Path, help="SQLite file to create or atomically replace")
    parser.add_argument("--source-dir", type=Path, default=DEFAULT_SOURCE,
                        help="source directory (default: the repository's .sot/term)")
    args = parser.parse_args()
    try:
        output = args.output.parent.resolve() / args.output.name
        for source, line, message in build(args.source_dir.resolve(), output):
            location = f"{source}:{line}" if line else source
            print(f"warning: {location!r}: {message}", file=sys.stderr)
    except (OSError, ValueError, sqlite3.Error) as error:
        parser.exit(1, f"{parser.prog}: {error}\n")


if __name__ == "__main__":
    main()
