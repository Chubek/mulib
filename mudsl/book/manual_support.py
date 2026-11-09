"""Print presentation and a cross-document link catalogue for Markdown manuals.

This does not modify Markdown sources or require the repository manifest.
"""
from dataclasses import dataclass
import hashlib
import html
import os
from pathlib import Path
import re
import unicodedata
from urllib.parse import quote, unquote, urlsplit, urlunsplit

import md2latex


def clean_source(source):
    """Expand print disclosures and omit HTML markers, never fenced/indented code."""
    output = []
    fence = None
    comment = False
    summary = None
    for line in source.replace("\r\n", "\n").replace("\r", "\n").split("\n"):
        stripped = line.lstrip()
        if fence:
            output.append(line)
            if re.fullmatch(re.escape(fence[0]) + "{" + str(fence[1]) + r",}\s*", stripped):
                fence = None
            continue
        if comment:
            if "-->" in line:
                comment = False
                output.append(line.split("-->", 1)[1])
            continue
        if summary is not None:
            before, found, after = line.partition("</summary>")
            summary.append(before.strip())
            if found:
                output.extend(["", "**" + " ".join(summary).strip() + "**", after, ""])
                summary = None
            continue
        match = md2latex.FENCE.match(stripped)
        if match:
            fence = (match[1][0], len(match[1]))
            output.append(line)
        elif line.startswith(("    ", "\t")):
            output.append(line)
        elif stripped.startswith("<!--"):
            before, found, after = stripped[4:].partition("-->")
            comment = not found
            output.append(after if found else "")
        elif re.fullmatch(r"</?details(?:\s[^>]*)?>", stripped, re.IGNORECASE):
            output.append("")
        elif stripped.startswith("<summary>"):
            before, found, after = stripped[9:].partition("</summary>")
            if found:
                output.extend(["", "**" + before.strip() + "**", after, ""])
            else:
                summary = [before.strip()]
        else:
            output.append(line)
    if summary is not None:
        output.extend(["", "**" + " ".join(summary).strip() + "**"])
    return "\n".join(output) + "\n"


def title_and_body(source, fallback):
    lines = source.lstrip("\ufeff").split("\n")
    first = next((i for i, line in enumerate(lines) if line.strip()), None)
    if first is not None:
        heading = md2latex.HEADING.match(lines[first])
        if heading and len(heading[1]) == 1:
            title = re.sub(r"[ \t]+#+[ \t]*$", "", heading[2] or "")
            if title:
                return title, "\n".join(lines[:first] + lines[first+1:]), True
        if first + 1 < len(lines) and re.fullmatch(r"\s*=+\s*", lines[first+1]):
            return lines[first].strip(), "\n".join(lines[:first] + lines[first+2:]), True
    return fallback, "\n".join(lines), False


def heading_slug(text):
    """GitHub-style fragments, preserving literal underscores inside code spans."""
    text = html.unescape(text)
    text = re.sub(r"!?\[([^\]]*)\]\([^)]*\)", r"\1", text)
    literals = []
    marker = "MUCODETOKEN"
    while marker in text:
        marker += "X"
    def literal(value):
        literals.append(value)
        return marker + str(len(literals)-1) + "END"
    text = re.sub(r"(`+)(.*?)\1", lambda match: literal(match[2]), text)
    text = re.sub(r"<[^>]*>", "", text)
    text = re.sub(r"\\([!\"#$%&'()*+,\-./:;<=>?@\[\]^_`{|}~])", lambda match: literal(match[1]), text)
    text = text.replace("*", "").replace("~", "")
    text = re.sub(r"(?<!\w)(_+)(\S(?:.*?\S)?)\1(?!\w)", r"\2", text)
    for i, value in enumerate(literals):
        text = text.replace(marker + str(i) + "END", value)
    text = unicodedata.normalize("NFC", text).lower()
    return "".join("-" if c.isspace() else c for c in text
                   if c.isspace() or c in "_-" or unicodedata.category(c)[0] in "LNM")


class HeadingCollector(md2latex.MarkdownParser):
    def __init__(self, source):
        super().__init__(source)
        self.headings = []

    def _heading(self, level, text):
        self.headings.append((level, text))
        return ""

    def _inline(self, text):
        return ""

    def _code(self, lines):
        return ""


@dataclass
class Heading:
    level: int
    title: str
    anchor: str
    bookmark_level: int


class Manual:
    def __init__(self, path, root, namespace):
        self.path = path.resolve()
        self.relative = path.relative_to(root).as_posix()
        identity = namespace + "\0" + self.relative
        self.anchor = "mu-manual-" + hashlib.sha256(identity.encode()).hexdigest()[:20]
        cleaned = clean_source(path.read_text(encoding="utf-8-sig"))
        fallback = path.parent.name if path.stem.lower() == "readme" else path.stem
        self.title, self.body, consumed = title_and_body(cleaned, fallback)
        collector = HeadingCollector(cleaned)
        collector.parse()
        self.fragments = {"": self.anchor}
        self.headings = []
        used = set()
        previous = 0
        for i, (level, title) in enumerate(collector.headings):
            base = heading_slug(title)
            slug = base
            suffix = 1
            while slug in used:
                slug = base + "-" + str(suffix)
                suffix += 1
            used.add(slug)
            if i == 0 and consumed:
                anchor = self.anchor
            else:
                anchor = self.anchor + "-" + hashlib.sha256(slug.encode()).hexdigest()[:16]
                previous = min(max(level-1, 1), previous+1)
                self.headings.append(Heading(max(level, 2), title, anchor, previous))
            self.fragments[slug] = anchor


class ManualCatalogue:
    def __init__(self, root, sources, pdf_directory, namespace="manuals", tex_directory=None):
        self.root = root.resolve()
        self.pdf_directory = pdf_directory.resolve()
        self.tex_directory = (tex_directory or pdf_directory.parent).resolve()
        self.manuals = [Manual(path, root, namespace) for path in sources]
        self.paths = {manual.path: manual for manual in self.manuals}
        self.warnings = set()

    def resolve_link(self, manual, destination, for_typesetting=False):
        """Return an internal target or a source-relative URL, never an invented URL."""
        url = urlsplit(destination)
        if url.scheme or url.netloc:
            return None
        target = (manual.path.parent / unquote(url.path)).resolve() if url.path else manual.path
        document = self.paths.get(target)
        if document is not None and not url.query and not for_typesetting:
            fragment = unicodedata.normalize("NFC", unquote(url.fragment))
            anchor = document.fragments.get(fragment)
            if anchor is None:
                self.warnings.add(f"{manual.relative}: heading {destination!r} not found; linked to the manual opening")
                anchor = document.anchor
            return "internal", anchor
        if url.path:
            # Keep repository-file links usable next to the built PDF; no absolute
            # checkout path is serialized into a generated document.
            # Graphics are opened by TeX from its working directory, whereas
            # clickable file links are opened relative to the published PDF.
            directory = self.tex_directory if for_typesetting else self.pdf_directory
            relative = Path(os.path.relpath(target, directory)).as_posix()
            return "file", urlunsplit(("", "", quote(relative, safe="/.-_"), url.query, url.fragment))
        return None


def ordered_sources(manual):
    lead = {name: i for i, name in enumerate(("README.md", "integration.md", "modules.md", "strata.md"))}
    def key(path):
        relative = path.relative_to(manual).as_posix()
        if relative in lead:
            return 0, lead[relative], relative
        if not path.name.startswith("mulib-"):
            return 1, path.name == "index.md", relative.casefold(), relative
        return 2, relative.casefold(), relative
    return sorted(manual.rglob("*.md"), key=key)


def inline_code(tokens):
    """Prefer punctuation/case boundaries; allow penalized fallback in long names."""
    boundaries = []
    for i, token in enumerate(tokens[:-1]):
        following = tokens[i+1]
        if token in (r"\_", "/", ".", ":", "-", ",", ";", ")", "]", ">", r"\ ") or (
                len(token) == len(following) == 1 and token.islower() and following.isupper()):
            boundaries.append(i+1)
    ends = boundaries + [len(tokens)]
    segment = 0
    start = 0
    rendered = []
    for i, token in enumerate(tokens):
        rendered.append(token)
        if i+1 == len(tokens):
            continue
        if i+1 == ends[segment]:
            rendered.append(r"\allowbreak{}")
            start = i+1
            segment += 1
        elif ends[segment]-start >= 12 and i+1-start >= 4 and ends[segment]-i-1 >= 4:
            rendered.append(r"\MuManualBreak{}")
    return r"\MuManualCode{" + "".join(rendered) + "}"


def table_weights(rows, columns):
    """Gentle content weighting while retaining a minimum useful column measure."""
    scores = [0] * columns
    for row in rows:
        for i, cell in enumerate(row.split(" & ")[:columns]):
            cell = re.sub(r"\\href\{[^}]*\}", "", cell)
            cell = re.sub(r"\\[A-Za-z]+(?:\[[^]]*\])?", "", cell)
            scores[i] += len(cell.replace("{", "").replace("}", ""))
    scores = [max(8, min(score / max(1, len(rows)), 200)) ** 0.5 for score in scores]
    values = [round(1000 * (0.7 + 0.3 * columns * score / sum(scores))) for score in scores]
    values[-1] = 1000 * columns - sum(values[:-1])
    return [f"{value/1000:.3f}" for value in values]
