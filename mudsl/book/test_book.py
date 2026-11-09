#!/usr/bin/env python3
"""Regression checks for the generated book and MuLL highlighting."""
import importlib.util
from pathlib import Path
import tempfile
import re
import unittest

from book_support import fragment
from pygments_mull import MuLLLexer
from pygments.token import Error, Keyword, String, Comment, Operator

HERE = Path(__file__).resolve().parent


def load(name):
    spec = importlib.util.spec_from_file_location(name, HERE / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class BookTests(unittest.TestCase):
    def test_lexer(self):
        tokens = list(MuLLLexer().get_tokens('val x <- "# // $E?\\n"; # comment\nreturn true and x;'))
        self.assertNotIn(Error, [kind for kind, text in tokens])
        self.assertIn((Keyword, "val"), tokens)
        self.assertIn((String.Interpol, "$E?"), tokens)
        self.assertIn((String.Escape, "\\n"), tokens)
        self.assertIn((Comment.Single, "# comment"), tokens)
        self.assertIn((Operator.Word, "and"), tokens)

    def test_markdown(self):
        text = fragment('# C++ & D\n\n```mull\nreturn "\\\\end{MDCode}";\n```\n', chapter=True)
        self.assertIn(r"\chapter{C++ \& D}", text)
        self.assertIn(r"{mull}{}", text)
        self.assertIn(r"\begin{MDCodeX}", text)

    def test_manuals_and_stale_output(self):
        module = load("typeset-manuals")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manual = root / "manual"
            manual.mkdir()
            (manual / "one.md").write_text("# One\n\n```c\nint x;\n```\n")
            (manual / "two.md").write_text("# Two\n")
            output = root / "out/manuals.tex"
            self.assertEqual(module.generate(manual, output), 2)
            module.generate(manual, output, check=True)
            (manual / "two.md").write_text("# Changed\n")
            with self.assertRaises(ValueError):
                module.generate(manual, output, check=True)

    def test_manifest(self):
        module = load("typeset-manifest")
        source = 'module_count: 1\nmodules:\n- name: "a_b"\n  purpose: "50% & more"\n  dependencies: [x]\n'
        text = module.render(source)
        self.assertIn(r"a\_b", text)
        self.assertIn(r"50\% \& more", text)
        with self.assertRaises(ValueError):
            module.render(source.replace("module_count: 1", "module_count: 2"))

    def test_wrapping_preserves_destinations_and_code(self):
        source = '[link](https://example.org/a_b) alpha/beta 0⁰\n\n```c\na/b;\n```\n'
        text = fragment(source)
        self.assertIn(r'\href{https://example.org/a\_b}{link}', text)
        self.assertIn(r'alpha/\allowbreak{}beta', text)
        self.assertIn(r'0\textsuperscript{0}', text)
        self.assertIn('\na/b;\n', text)
        table = fragment('| A | B |\n| --- | --- |\n| x | y |\n')
        self.assertIn(r'\begin{xltabular}', table)
        self.assertNotIn(r'\begin{center}', table)

    def test_reference_tables_have_repeated_open_headers(self):
        table = fragment('| Name | Value |\n| --- | ---: |\n| a | b |\n| c | d |\n')
        self.assertEqual(table.count(r'\textbf{Name}'), 2)
        for command in ('toprule', 'midrule', 'bottomrule', 'endfirsthead',
                        'endhead', 'endfoot', 'endlastfoot'):
            self.assertIn('\\' + command, table)
        self.assertNotIn(r'\hline', table)
        self.assertNotIn('|', table)
        self.assertIn(r'\multicolumn{2}', table)
        self.assertIn(r'a & b \\', table)
        literal = fragment('```tex\n\\begin{tabularx}{\\linewidth}{|X|}\n\\hline\n```\n')
        self.assertIn(r'\begin{tabularx}{\linewidth}{|X|}', literal)
        self.assertIn(r'\hline', literal)

    def test_typography_suite_is_complete_and_loaded(self):
        support = HERE / 'support'
        packages = {path.stem for path in support.glob('*.sty')}
        self.assertGreater(len(packages), 20)
        loader = (support / 'mudsl-book.sty').read_text()
        loaded = set(re.findall(r'\\RequirePackage\{support/([^}]+)\}', loader))
        self.assertEqual(packages - {'mudsl-book'}, loaded)
        for name in packages:
            self.assertIn(r'\ProvidesPackage{support/' + name + '}',
                          (support / (name + '.sty')).read_text())
        self.assertIn(r'\RequirePackageWithOptions{support/mudsl-book}',
                      (HERE / 'mudsl.sty').read_text())


if __name__ == "__main__":
    unittest.main()
