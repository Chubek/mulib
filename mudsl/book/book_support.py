"""Book-specific presentation on top of the supplied md2latex parser."""
from pathlib import Path
import re

import md2latex


def repository_file(name):
    here = Path(__file__).resolve().parent
    source = here.parents[1] / name
    return source if source.exists() else here.parent / "reference" / name


class BookMarkdown(md2latex.MarkdownParser):
    def __init__(self, source):
        super().__init__(source)
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


def fragment(source, chapter=False):
    parser = BookMarkdown(source)
    result = parser.parse()
    # Long reference tables must span pages, rather than becoming tall boxes.
    result = result.replace("\\begin{center}\n\\begin{tabularx}", "\\begin{xltabular}")
    result = result.replace("\\end{tabularx}\n\\end{center}", "\\end{xltabular}")
    # Permit identifiers and paths to wrap in prose/table cells. Keep escaped
    # TeX commands intact and never modify literal code listings.
    def break_code(match):
        tokens = re.findall(r"\\[A-Za-z]+\{\}|\\.|[^\\]", match[1])
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
