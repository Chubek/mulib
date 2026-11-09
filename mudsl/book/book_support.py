"""Book-specific presentation on top of the supplied md2latex parser."""
from pathlib import Path
import re

import md2latex


def repository_file(name):
    here = Path(__file__).resolve().parent
    source = here.parents[1] / name
    return source if source.exists() else here.parent / "reference" / name


class BookMarkdown(md2latex.MarkdownParser):
    def __init__(self, source, reference=None):
        super().__init__(source)
        self.reference = reference
        self.heading_index = 0
        self.code_blocks = []
        self.marker = "MUDSL-CODE-PLACEHOLDER-"
        while self.marker in source:
            self.marker += "X"
        self.languages = {}
        lines = self.lines
        i = 0
        while i < len(lines):
            fence = md2latex.FENCE.match(lines[i].lstrip())
            if not fence:
                i += 1
                continue
            language = fence[2].strip().split()[0] if fence[2].strip() else "text"
            end = re.compile(r"^\s*" + re.escape(fence[1][0]) + "{" + str(len(fence[1])) + r",}\s*$")
            code = []
            i += 1
            while i < len(lines) and not end.match(lines[i]):
                code.append(lines[i])
                i += 1
            self.languages["\n".join(code)] = language
            i += 1

    def _heading(self, level, text):
        if self.reference is None:
            return super()._heading(level, text)
        manual, catalogue = self.reference
        if self.heading_index >= len(manual.headings):
            raise ValueError(f"{manual.relative}: heading catalogue is out of sync")
        heading = manual.headings[self.heading_index]
        self.heading_index += 1
        if (max(level, 2), text) != (heading.level, heading.title):
            raise ValueError(f"{manual.relative}: inconsistent heading order")
        return (rf"\MuManualHeading{{{heading.level}}}{{{heading.anchor}}}"
                + "{" + self._inline(text) + "}" + "{" + str(heading.bookmark_level) + "}")

    def _code(self, lines):
        from pygments.lexers import get_lexer_by_name
        from pygments.util import ClassNotFound
        code = "\n".join(lines)
        language = self.languages.get(code, "text")
        language = {"c++": "cpp", "sh": "bash", "sexpr": "scheme"}.get(language, language)
        if language != "mull":
            try:
                get_lexer_by_name(language)
            except ClassNotFound:
                language = "text"
        # Use a unique environment name if the listing contains TeX delimiters.
        env = self.code_environment
        rendered = ("\\begingroup\n" + rf"\newminted[{env}]{{{language}}}{{}}" + "\n"
                    + "\\begin{" + env + "}\n" + code + "\n\\end{" + env + "}\n\\endgroup")
        self.code_blocks.append(rendered)
        return self.marker + str(len(self.code_blocks)-1) + "!"


def _command_destinations(text, transform):
    """Transform a command's balanced first argument without inspecting its label."""
    pattern = re.compile(r"\\(href|url|includegraphics)(?:\[[^]]*\])?\{")
    position = 0
    pieces = []
    while (match := pattern.search(text, position)):
        end, depth = match.end(), 1
        while end < len(text) and depth:
            if text[end] == "\\" and end+1 < len(text) and text[end+1] in "{}":
                end += 2
                continue
            depth += (text[end] == "{") - (text[end] == "}")
            end += 1
        if depth:
            raise ValueError("unbalanced generated link destination")
        pieces.append(text[position:match.start()])
        pieces.append(transform(match[1], text[match.end():end-1], text[match.start():end]))
        position = end
    pieces.append(text[position:])
    return "".join(pieces)


def _reference_links(text, manual, catalogue):
    from urllib.parse import unquote, urlsplit

    def transform(command, destination, original):
        # Destinations have already been escaped by the standalone converter.
        decoded = re.sub(r"\\mdchar\{(\d+)\}", lambda m: chr(int(m[1])), destination)
        for char, escaped in sorted(md2latex.ESCAPES.items(), key=lambda item: -len(item[1])):
            decoded = decoded.replace(escaped, char)
        resolution = catalogue.resolve_link(manual, decoded, for_typesetting=command == "includegraphics")
        if resolution is None:
            return original
        kind, target = resolution
        if command == "href" and kind == "internal":
            return r"\hyperref[" + target + "]"
        if kind == "file" and command in ("href", "url"):
            return "\\" + command + "{" + md2latex._url(target) + "}"
        if kind == "file" and command == "includegraphics":
            path = unquote(urlsplit(target).path)
            encoded = "".join(c if c.isalnum() or c in "/.- " else rf"\mdchar{{{ord(c)}}}" for c in path)
            return original[:original.index("{")+1] + encoded + "}"
        return original

    return _command_destinations(text, transform)


def fragment(source, chapter=False, reference=None):
    parser = BookMarkdown(source, reference)
    result = parser.parse()
    if reference is not None:
        result = _reference_links(result, *reference)
    # Replace the converter's boxed grids with open, multipage book tables.
    # Work before restoring code placeholders: literal TeX in listings is data.
    def reference_table(match):
        spec = match[1].replace("|", "")
        rows = [row.removesuffix(r" \hline") for row in match[2].splitlines()]
        header, *body = rows
        columns = spec.count("X")
        if reference is not None:
            from manual_support import table_weights
            weights = iter(table_weights(rows, columns))
            spec = re.sub("X", lambda m: r">{\hsize=" + next(weights) + r"\hsize\linewidth=\hsize}X", spec)
        return "\n".join([
            r"\begin{xltabular}{\linewidth}{@{}" + spec + r"@{}}",
            r"\toprule", header, r"\midrule", r"\endfirsthead",
            r"\toprule", header, r"\midrule", r"\endhead",
            r"\midrule",
            rf"\multicolumn{{{columns}}}{{r}}{{\footnotesize\itshape Continued on next page}} \\",
            r"\endfoot", r"\bottomrule", r"\endlastfoot",
            *body, r"\end{xltabular}",
        ])

    result = re.sub(
        r"\\begin\{center\}\n\\begin\{tabularx\}\{\\linewidth\}\{([^\n]+)\}\n"
        r"\\hline\n(.*?)\n\\end\{tabularx\}\n\\end\{center\}",
        reference_table, result, flags=re.DOTALL,
    )
    # Permit identifiers and paths to wrap in prose/table cells. Keep escaped
    # TeX commands intact and never modify literal code listings.
    def break_code(match):
        tokens = re.findall(r"\\[A-Za-z]+\{\}|\\.|[^\\]", match[1])
        if reference is not None:
            from manual_support import inline_code
            return inline_code(tokens)
        return r"\texttt{" + r"\allowbreak{}".join(tokens) + "}"
    result = re.sub(r"\\texttt\{((?:\\[A-Za-z]+\{\}|\\.|[^{}])*)\}", break_code, result)
    # Protect command destinations while adding break opportunities to prose
    # paths and slash-separated protocol/operator names.
    destinations = []
    pattern = re.compile(r"\\(?:href|url|includegraphics)(?:\[[^]]*\])?\{")
    position = 0
    while (match := pattern.search(result, position)):
        end, depth = match.end(), 1
        while end < len(result) and depth:
            if result[end] == "\\" and end + 1 < len(result) and result[end+1] in "{}":
                end += 2
                continue
            depth += (result[end] == "{") - (result[end] == "}")
            end += 1
        key = parser.marker + "DEST" + str(len(destinations)) + "!"
        destinations.append(result[match.start():end])
        result = result[:match.start()] + key + result[end:]
        position = match.start() + len(key)
    if reference is not None:
        result = re.sub(r"/(?!\\allowbreak\{\})", lambda m: r"/\allowbreak{}", result)
        result = re.sub(r"\\_(?!\\allowbreak\{\})", lambda m: r"\_\allowbreak{}", result)
    else:
        result = result.replace("/", r"/\allowbreak{}").replace(r"\_", r"\_\allowbreak{}")
    result = result.replace("ᵀ", r"\textsuperscript{T}").replace("⁰", r"\textsuperscript{0}")
    for i, destination in enumerate(destinations):
        result = result.replace(parser.marker + "DEST" + str(i) + "!", destination)
    if chapter:
        levels = {"section": "chapter", "subsection": "section", "subsubsection": "subsection",
                  "paragraph": "subsubsection", "subparagraph": "paragraph"}
        result = re.sub(r"\\(section|subsection|subsubsection|paragraph|subparagraph)\{",
                        lambda m: "\\" + levels[m[1]] + "{", result)
    for i, code in enumerate(parser.code_blocks):
        result = result.replace(parser.marker + str(i) + "!", code)
    return result + "\n"


def write_output(path, text, check=False):
    path = Path(path)
    if path.exists() and path.read_text() == text:
        return
    if check:
        raise ValueError(f"{path}: generated output is missing or stale")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
