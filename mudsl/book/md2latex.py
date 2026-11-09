#!/usr/bin/env python3
"""Convert Markdown to LaTeX using a handwritten parser (Python 3.9+).

Usage:
    python md2latex.py input.md -o output.tex
    python md2latex.py input.md --fragment > body.tex
    cat input.md | python md2latex.py > output.tex

Library API:
    markdown_to_latex(text, standalone=True, title=None)

This implements a practical Markdown subset, not full CommonMark.
Reference links, HTML, footnotes, and math extensions are not parsed.
Raw LaTeX is escaped. Remote images become links; local image paths
are relative to the generated .tex file. Code is not highlighted.
Standard LaTeX lists support four levels of each list type.

The converter has no third-party Python dependencies. Typesetting
requires a LaTeX installation with the packages listed in PREAMBLE.
Use XeLaTeX or LuaLaTeX for Unicode beyond pdfLaTeX's font coverage.
Fragments require PREAMBLE's packages and helper macros.
"""

import argparse
from dataclasses import dataclass, field
import html
from pathlib import Path
import re
import string
import sys
import unicodedata
from urllib.parse import quote, urlsplit


PREAMBLE = r"""\documentclass[11pt]{article}
\usepackage{iftex}
\ifPDFTeX
  \usepackage[utf8]{inputenc}
  \usepackage[T1]{fontenc}
\else
  \usepackage{fontspec}
\fi
\usepackage[margin=1in]{geometry}
\usepackage{graphicx}
\usepackage{fancyvrb}
\usepackage{array,tabularx}
\usepackage[hidelinks]{hyperref}
\ExplSyntaxOn
\cs_new:Npn \mdchar #1 { \char_generate:nn {#1} {12} }
\ExplSyntaxOff
\newsavebox{\mdstrikebox}
\DeclareRobustCommand{\mdstrike}[1]{%
  \begingroup\sbox{\mdstrikebox}{#1}%
  \leavevmode\rlap{\raisebox{0.5ex}{\rule{\wd\mdstrikebox}{0.4pt}}}%
  \usebox{\mdstrikebox}\endgroup}
\setlength{\parindent}{0pt}
\setlength{\parskip}{0.6em}
\setcounter{secnumdepth}{0}
"""

ESCAPES = {
    "\\": r"\textbackslash{}",
    "{": r"\{",
    "}": r"\}",
    "$": r"\$",
    "&": r"\&",
    "#": r"\#",
    "%": r"\%",
    "_": r"\_",
    "~": r"\textasciitilde{}",
    "^": r"\textasciicircum{}",
}

FENCE = re.compile(r"^ {0,3}(`{3,}|~{3,})(.*)$")
HEADING = re.compile(r"^ {0,3}(#{1,6})(?:[ \t]+(.*?)|[ \t]*)$")
SETEXT = re.compile(r"^ {0,3}(=+|-+)[ \t]*$")
LIST = re.compile(r"^( {0,3})([-+*]|\d{1,9}[.)])([ \t]+|$)(.*)$")
QUOTE = re.compile(r"^ {0,3}>[ \t]?(.*)$")
RULE = re.compile(
    r"^ {0,3}(?:(?:\*[ \t]*){3,}|(?:-[ \t]*){3,}|(?:_[ \t]*){3,})$"
)
ENTITY = re.compile(
    r"&(?:#[0-9]+|#[xX][0-9a-fA-F]+|[a-zA-Z][a-zA-Z0-9]+);"
)


def escape_latex(text):
    """Escape source text without touching generated LaTeX commands."""
    return "".join(ESCAPES.get(c, c) for c in text)


def _unescape(text):
    text = re.sub(
        r"""\\([!"#$%&'()*+,\-./:;<=>?@\[\]\\^_`{|}~])""",
        r"\1",
        text,
    )
    return html.unescape(text)


def _url(text):
    encoded = quote(
        _unescape(text),
        safe="/:@?&=+$,;~!*'().-_#%[]",
    )
    return escape_latex(encoded)


def _code_end(text, start):
    """Find a closing backtick run of exactly the opening run's length."""
    end = start
    while end < len(text) and text[end] == "`":
        end += 1

    for match in re.finditer(r"`+", text[end:]):
        if len(match[0]) == end - start:
            return end, end + match.start(), end + match.end()
    return None


def _bracket_end(text, start):
    depth = 1
    i = start + 1

    while i < len(text):
        if text[i] == "\\":
            i += 2
            continue

        if text[i] == "`":
            code = _code_end(text, i)
            if code:
                i = code[2]
                continue

        if text[i] == "[":
            depth += 1
        elif text[i] == "]":
            depth -= 1
            if depth == 0:
                return i
        i += 1

    return None


def _destination(text, start):
    """Read a link destination and optional title; titles are ignored."""
    i = start + 1
    while i < len(text) and text[i].isspace():
        i += 1

    begin = i
    if i < len(text) and text[i] == "<":
        i += 1
        begin = i
        while i < len(text) and text[i] != ">":
            i += 2 if text[i] == "\\" else 1
        if i >= len(text):
            return None
        target = text[begin:i]
        i += 1
    else:
        depth = 0
        while i < len(text):
            c = text[i]
            if c == "\\" and i + 1 < len(text):
                i += 2
                continue
            if c.isspace() and depth == 0:
                break
            if c == "(":
                depth += 1
            elif c == ")":
                if depth == 0:
                    break
                depth -= 1
            i += 1
        target = text[begin:i]

    before_space = i
    while i < len(text) and text[i].isspace():
        i += 1

    if i > before_space and i < len(text) and text[i] in "\"'(":
        closer = ")" if text[i] == "(" else text[i]
        i += 1
        while i < len(text) and text[i] != closer:
            i += 2 if text[i] == "\\" else 1
        if i >= len(text):
            return None
        i += 1
        while i < len(text) and text[i].isspace():
            i += 1

    if i < len(text) and text[i] == ")":
        return target, i + 1
    return None


@dataclass
class _Token:
    text: str
    count: int = 0
    opens: bool = False
    closes: bool = False
    commands: list = field(default_factory=list)
    endings: int = 0


def _punct(c):
    return bool(c) and unicodedata.category(c)[0] in "PS"


def inline(text, allow_links=True):
    """Scan inline syntax, then resolve emphasis delimiters."""
    tokens = []
    i = 0

    def emit(value):
        if (
            tokens
            and tokens[-1].count == 0
            and not tokens[-1].commands
            and not tokens[-1].endings
        ):
            tokens[-1].text += value
        else:
            tokens.append(_Token(value))

    while i < len(text):
        c = text[i]

        if c == "\\" and i + 1 < len(text):
            if text[i + 1] == "\n":
                emit("\\\\\n")
                i += 2
                continue
            if text[i + 1] in string.punctuation:
                emit(escape_latex(text[i + 1]))
                i += 2
                continue

        if c == "`":
            code = _code_end(text, i)
            if code:
                value = text[code[0]:code[1]].replace("\n", " ")
                if (
                    value.startswith(" ")
                    and value.endswith(" ")
                    and value.strip()
                ):
                    value = value[1:-1]
                value = escape_latex(value).replace(" ", r"\ ")
                emit(r"\texttt{" + value + "}")
                i = code[2]
                continue

        image = text.startswith("![", i)
        if image or (c == "[" and allow_links):
            opening = i + 1 if image else i
            closing = _bracket_end(text, opening)

            if (
                closing is not None
                and text[closing + 1:closing + 2] == "("
            ):
                dest = _destination(text, closing + 1)
                if dest:
                    target, end = dest
                    label = inline(
                        text[opening + 1:closing],
                        allow_links=False,
                    )

                    if (
                        image
                        and target
                        and not urlsplit(_unescape(target)).scheme
                    ):
                        path = "".join(
                            c if c.isalnum() or c in "/.- "
                            else r"\mdchar{" + str(ord(c)) + "}"
                            for c in _unescape(target).replace("\\", "/")
                        )
                        emit(
                            r"\includegraphics[width=0.8\linewidth,"
                            r"height=0.4\textheight,keepaspectratio]{"
                            + path + "}"
                        )
                    else:
                        label = label or escape_latex(_unescape(target))
                        emit(r"\href{" + _url(target) + "}{" + label + "}")

                    i = end
                    continue

        if c == "<" and allow_links:
            end = text.find(">", i + 1)
            if end != -1:
                target = text[i + 1:end]
                if re.fullmatch(r"(?:https?://|mailto:)[^\s<>]+", target):
                    emit(
                        r"\href{" + _url(target) + "}{"
                        + escape_latex(target) + "}"
                    )
                    i = end + 1
                    continue
                if re.fullmatch(r"[^\s<>@]+@[^\s<>@]+\.[^\s<>@]+", target):
                    emit(
                        r"\href{mailto:" + _url(target) + "}{"
                        + escape_latex(target) + "}"
                    )
                    i = end + 1
                    continue

        if c == "&":
            match = ENTITY.match(text, i)
            if match:
                emit(escape_latex(html.unescape(match[0])))
                i = match.end()
                continue

        if c in "*_~":
            end = i + 1
            while end < len(text) and text[end] == c:
                end += 1

            count = end - i
            before = text[i - 1] if i else "\n"
            after = text[end] if end < len(text) else "\n"

            left = not after.isspace() and (
                not _punct(after) or before.isspace() or _punct(before)
            )
            right = not before.isspace() and (
                not _punct(before) or after.isspace() or _punct(after)
            )
            opens = left and (c != "_" or not right or _punct(before))
            closes = right and (c != "_" or not left or _punct(after))

            if c != "~" or count == 2:
                tokens.append(_Token(c, count, opens, closes))
            else:
                emit(escape_latex(c * count))

            i = end
            continue

        if c == " " and text[i:].startswith("  "):
            end = i
            while end < len(text) and text[end] == " ":
                end += 1
            if end < len(text) and text[end] == "\n":
                emit("\\\\\n")
                i = end + 1
                continue

        emit(" " if c == "\n" else escape_latex(c))
        i += 1

    for ci, closer in enumerate(tokens):
        while closer.closes and closer.count:
            oi = ci - 1
            while oi >= 0:
                opener = tokens[oi]
                if (
                    opener.opens
                    and opener.count
                    and opener.text == closer.text
                ):
                    ambiguous = (
                        (opener.closes or closer.opens)
                        and (opener.count + closer.count) % 3 == 0
                        and (opener.count % 3 or closer.count % 3)
                    )
                    if closer.text == "~" or not ambiguous:
                        break
                oi -= 1

            if oi < 0:
                break

            opener = tokens[oi]
            use = 2 if min(opener.count, closer.count) >= 2 else 1
            if closer.text == "~":
                command = r"\mdstrike{"
            else:
                command = r"\textbf{" if use == 2 else r"\emph{"

            opener.commands.insert(0, command)
            closer.endings += 1
            opener.count -= use
            closer.count -= use

            for inner in tokens[oi + 1:ci]:
                inner.opens = inner.closes = False

    parts = []
    for token in tokens:
        if token.count:
            value = escape_latex(token.text * token.count)
        elif token.commands or token.endings:
            value = ""
        else:
            value = token.text
        parts.append(
            "}" * token.endings + value + "".join(token.commands)
        )
    return "".join(parts)


def _cells(line):
    """Split table pipes except those escaped or inside code spans."""
    cells = []
    current = []
    i = 0
    last_pipe = -1

    while i < len(line):
        if line[i] == "\\" and i + 1 < len(line):
            current.extend(line[i:i + 2])
            i += 2
            continue

        if line[i] == "`":
            code = _code_end(line, i)
            if code:
                current.append(line[i:code[2]])
                i = code[2]
                continue

        if line[i] == "|":
            cells.append("".join(current).strip())
            current = []
            last_pipe = i
        else:
            current.append(line[i])
        i += 1

    cells.append("".join(current).strip())
    if line.lstrip().startswith("|"):
        cells.pop(0)
    if (
        len(cells) > 1
        and cells[-1] == ""
        and last_pipe == len(line.rstrip()) - 1
    ):
        cells.pop()
    return cells


def _alignment(header, separator):
    if "|" not in header:
        return None

    cells = _cells(separator)
    if (
        len(cells) != len(_cells(header))
        or not all(re.fullmatch(r":?-{3,}:?", c) for c in cells)
    ):
        return None

    return [
        "c" if c.startswith(":") and c.endswith(":")
        else "r" if c.endswith(":")
        else "l"
        for c in cells
    ]


class MarkdownParser:
    def __init__(self, source):
        normalized = source.replace("\r\n", "\n").replace("\r", "\n")
        self.lines = normalized.expandtabs(4).split("\n")

        # Avoid terminating a code block with text from its own contents.
        self.code_environment = "MDCode"
        while "\\end{" + self.code_environment + "}" in source:
            self.code_environment += "X"

    def _code(self, lines):
        env = self.code_environment
        return (
            r"\DefineVerbatimEnvironment{" + env
            + r"}{Verbatim}{fontsize=\small,frame=single}" + "\n"
            + "\\begin{" + env + "}\n"
            + "\n".join(lines)
            + "\n\\end{" + env + "}"
        )

    @staticmethod
    def _starts_block(lines, i):
        line = lines[i]
        return bool(
            FENCE.match(line)
            or HEADING.match(line)
            or RULE.match(line)
            or LIST.match(line)
            or QUOTE.match(line)
            or (
                i + 1 < len(lines)
                and _alignment(line, lines[i + 1])
            )
        )

    def _list(self, lines, start, enum_depth):
        first = LIST.match(lines[start])
        ordered = first[2][0].isdigit()
        base = len(first[1])
        env = "enumerate" if ordered else "itemize"
        out = ["\\begin{" + env + "}"]
        i = start

        if ordered and int(first[2][:-1]) != 1:
            if enum_depth >= 4:
                raise ValueError(
                    "LaTeX supports at most four nested enumerate environments"
                )
            counter = ("enumi", "enumii", "enumiii", "enumiv")[enum_depth]
            out.append(
                "\\setcounter{" + counter + "}{"
                + str(int(first[2][:-1]) - 1) + "}"
            )

        while i < len(lines):
            match = LIST.match(lines[i])
            if (
                not match
                or len(match[1]) != base
                or match[2][0].isdigit() != ordered
            ):
                break

            indent = match.start(4)
            body = [match[4]]
            i += 1
            blank = False

            while i < len(lines):
                line = lines[i]
                if not line.strip():
                    body.append("")
                    blank = True
                    i += 1
                    continue

                leading = len(line) - len(line.lstrip(" "))
                if leading >= indent:
                    body.append(line[indent:])
                elif leading > base:
                    body.append(line[leading:])
                elif (
                    LIST.match(line)
                    or blank
                    or self._starts_block(lines, i)
                ):
                    break
                else:
                    body.append(line)

                blank = False
                i += 1

            out.append(
                "\\item{} "
                + self._blocks(body, enum_depth + int(ordered))
            )

        out.append("\\end{" + env + "}")
        return "\n".join(out), i

    def _blocks(self, lines, enum_depth=0):
        out = []
        i = 0

        while i < len(lines):
            line = lines[i]
            if not line.strip():
                i += 1
                continue

            fence = FENCE.match(line)
            if fence and not (
                fence[1][0] == "`" and "`" in fence[2]
            ):
                closing = re.compile(
                    r"^ {0,3}" + re.escape(fence[1][0])
                    + "{" + str(len(fence[1])) + r",}[ \t]*$"
                )
                code = []
                indent = len(line) - len(line.lstrip(" "))
                i += 1

                while i < len(lines) and not closing.match(lines[i]):
                    leading = len(lines[i]) - len(lines[i].lstrip(" "))
                    code.append(lines[i][min(indent, leading):])
                    i += 1

                out.append(self._code(code))
                i += int(i < len(lines))
                continue

            if line.startswith("    "):
                code = []
                while i < len(lines) and (
                    lines[i].startswith("    ") or not lines[i].strip()
                ):
                    code.append(lines[i][4:] if lines[i].strip() else "")
                    i += 1
                while code and not code[-1]:
                    code.pop()
                out.append(self._code(code))
                continue

            heading = HEADING.match(line)
            if heading:
                level = len(heading[1])
                value = re.sub(
                    r"[ \t]+#+[ \t]*$", "", heading[2] or ""
                )
                command = (
                    "section", "subsection", "subsubsection",
                    "paragraph", "subparagraph", "subparagraph",
                )[level - 1]
                out.append("\\" + command + "{" + inline(value) + "}")
                i += 1
                continue

            if RULE.match(line):
                out.append(r"\par\noindent\rule{\linewidth}{0.4pt}\par")
                i += 1
                continue

            if QUOTE.match(line):
                body = []
                while i < len(lines) and QUOTE.match(lines[i]):
                    body.append(QUOTE.match(lines[i])[1])
                    i += 1
                out.append(
                    "\\begin{quote}\n"
                    + self._blocks(body, enum_depth)
                    + "\n\\end{quote}"
                )
                continue

            if LIST.match(line):
                rendered, i = self._list(lines, i, enum_depth)
                out.append(rendered)
                continue

            align = (
                _alignment(line, lines[i + 1])
                if i + 1 < len(lines) else None
            )
            if align:
                commands = {
                    "l": r"\raggedright",
                    "c": r"\centering",
                    "r": r"\raggedleft",
                }
                spec = "|" + "|".join(
                    ">{" + commands[a] + r"\arraybackslash}X"
                    for a in align
                ) + "|"

                table = [
                    r"\begin{center}",
                    r"\begin{tabularx}{\linewidth}{" + spec + "}",
                    r"\hline",
                ]
                table.append(
                    " & ".join(
                        r"\textbf{" + inline(c) + "}"
                        for c in _cells(line)
                    ) + r" \\ \hline"
                )
                i += 2

                while (
                    i < len(lines)
                    and lines[i].strip()
                    and "|" in lines[i]
                    and not self._starts_block(lines, i)
                ):
                    cells = (
                        _cells(lines[i]) + [""] * len(align)
                    )[:len(align)]
                    table.append(
                        " & ".join(inline(c) for c in cells)
                        + r" \\ \hline"
                    )
                    i += 1

                table.extend([
                    r"\end{tabularx}",
                    r"\end{center}",
                ])
                out.append("\n".join(table))
                continue

            paragraph = [line.lstrip()]
            i += 1
            setext = None

            while i < len(lines) and lines[i].strip():
                setext = SETEXT.match(lines[i])
                if setext:
                    i += 1
                    break
                if self._starts_block(lines, i):
                    break
                paragraph.append(lines[i].lstrip())
                i += 1

            value = inline("\n".join(paragraph).rstrip())
            if setext:
                command = (
                    "section" if setext[1][0] == "=" else "subsection"
                )
                value = "\\" + command + "{" + value + "}"
            out.append(value)

        return "\n\n".join(out)

    def parse(self):
        return self._blocks(self.lines)


def markdown_to_latex(text, standalone=True, title=None):
    """Return a complete article document or a body-only fragment."""
    body = MarkdownParser(text).parse()
    if not standalone:
        return body + ("\n" if body else "")

    metadata = ""
    if title is not None:
        metadata = (
            r"\title{" + escape_latex(title)
            + "}\n\\author{}\n\\date{}\n"
        )

    return (
        PREAMBLE + metadata + "\\begin{document}\n"
        + ("\\maketitle\n" if title is not None else "")
        + body + "\n\\end{document}\n"
    )


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "input", nargs="?", default="-",
        help="UTF-8 Markdown file; '-' or omitted reads stdin",
    )
    parser.add_argument(
        "-o", "--output", default="-",
        help="LaTeX output file; '-' or omitted writes stdout",
    )
    parser.add_argument(
        "--fragment", action="store_true",
        help="omit the preamble and document environment",
    )
    parser.add_argument("--title", help="plain-text document title")
    args = parser.parse_args(argv)

    if args.fragment and args.title is not None:
        parser.error("--title requires a standalone document")

    try:
        source = (
            sys.stdin.read()
            if args.input == "-"
            else Path(args.input).read_text(encoding="utf-8-sig")
        )
        result = markdown_to_latex(
            source,
            standalone=not args.fragment,
            title=args.title,
        )
        if args.output == "-":
            sys.stdout.write(result)
        else:
            Path(args.output).write_text(result, encoding="utf-8")
    except (OSError, UnicodeError, ValueError) as exc:
        parser.exit(1, f"md2latex: {exc}\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())