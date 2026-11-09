#!/usr/bin/env python3
"""Regression checks for the generated book and MuLL highlighting."""
import importlib.util
from pathlib import Path
import tempfile
import re
import unittest

from book_support import fragment
import md2latex
from manual_support import ManualCatalogue, clean_source, heading_slug, ordered_sources, table_weights
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

    def test_manifest_is_not_a_book_input(self):
        self.assertNotIn('manifest.tex', (HERE / 'the-book-of-mudsl.ltx').read_text())
        self.assertNotIn('typeset-manifest', (HERE / 'Makefile').read_text())
        self.assertFalse((HERE / 'typeset-manifest.py').exists())

    def test_standalone_converter_heading_hooks_preserve_output(self):
        source = '# One\n\n## Two\n\nSetext\n======\n\nBody.\n'
        self.assertEqual(md2latex.markdown_to_latex(source, standalone=False),
                         '\\section{One}\n\n\\subsection{Two}\n\n\\section{Setext}\n\nBody.\n')

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


class ManualTests(unittest.TestCase):
    def setUp(self):
        parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
        self.temp = tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / 'manual'
        self.root.mkdir()

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def catalogue(self, namespace='manuals'):
        return ManualCatalogue(self.root, ordered_sources(self.root), HERE / 'build', namespace)

    def test_titles_headings_and_provenance(self):
        self.write('mulib-one.md', '# MuLIB one\n\n## Contract\n\nThe contract.\n')
        module = load('typeset-manuals')
        output = self.root.parent / 'generated/manuals.tex'
        module.generate(self.root, output)
        chapter = (output.parent / 'manuals/manual-0001.tex').read_text()
        self.assertIn(r'\chapter{MuLIB one}', chapter)
        self.assertIn(r'\MuManualSource{mulib-one.md}', chapter)
        self.assertEqual(chapter.count('MuLIB one'), 1)
        self.assertIn(r'\MuManualHeading{2}', chapter)
        self.assertIn(r'\begin{MuManuals}', output.read_text())

    def test_guide_order_and_all_sources_included(self):
        for name in ('mulib-z.md', 'index.md', 'README.md', 'mulib-a.md', 'integration.md', 'guide.md'):
            self.write(name, '# ' + name + '\n')
        self.assertEqual([p.name for p in ordered_sources(self.root)],
                         ['README.md', 'integration.md', 'guide.md', 'index.md', 'mulib-a.md', 'mulib-z.md'])

    def test_duplicate_fragments_and_markdown_links(self):
        self.write('one.md', '# One\n\n## API `a_b`\n\n## API `a_b`\n')
        self.write('two.md', '# Two\n\n[First](one.md#api-a_b) [Second](one.md#api-a_b-1) [Root](one.md)\n')
        catalogue = self.catalogue()
        one, two = catalogue.manuals
        rendered = fragment(two.body, reference=(two, catalogue))
        for anchor in one.fragments.values():
            self.assertIn(r'\hyperref[' + anchor + ']', rendered)
        self.assertFalse(catalogue.warnings)

    def test_catalogues_are_namespaced_and_targets_do_not_depend_on_checkout(self):
        self.write('README.md', '# Guide\n\n## A\n')
        a, b = self.catalogue('manuals'), self.catalogue('stdlib')
        self.assertNotEqual(a.manuals[0].anchor, b.manuals[0].anchor)
        self.assertNotEqual(a.manuals[0].headings[0].anchor, b.manuals[0].headings[0].anchor)
        second = self.root.parent / 'elsewhere'
        second.mkdir()
        (second / 'README.md').write_text((self.root / 'README.md').read_text())
        relocated = ManualCatalogue(second, ordered_sources(second), HERE / 'build')
        self.assertEqual(a.manuals[0].anchor, relocated.manuals[0].anchor)

    def test_html_disclosures_comments_and_literal_code(self):
        source = ('# Test\n\n<!-- hidden\n## Not a heading\n-->\n'
                  '<details>\n<summary>Types and declarations</summary>\n\n'
                  '```c\n<!-- literal comment -->\n<details>\n# literal heading\n```\n</details>\n')
        self.write('one.md', source)
        catalogue = self.catalogue()
        manual = catalogue.manuals[0]
        rendered = fragment(manual.body, reference=(manual, catalogue))
        self.assertEqual(manual.headings, [])
        self.assertIn(r'\textbf{Types and declarations}', rendered)
        self.assertIn('<!-- literal comment -->\n<details>\n# literal heading', rendered)
        self.assertNotIn('Not a heading', rendered)
        self.assertNotIn('</details>', rendered)

    def test_setext_title_and_heading_slugs(self):
        self.write('one.md', 'Title\n=====\n\nA subsection\n------------\n')
        manual = self.catalogue().manuals[0]
        self.assertEqual(manual.title, 'Title')
        self.assertEqual(manual.headings[0].level, 2)
        self.assertEqual(heading_slug('API `a_b` & **bounds**'), 'api-a_b--bounds')
        self.assertEqual(heading_slug('plain_name and _emphasis_'), 'plain_name-and-emphasis')
        self.assertEqual(heading_slug(r'\_literal\_'), '_literal_')

    def test_file_images_and_external_link_destinations(self):
        self.write('one.md', '# One\n\n[Web](https://example.org/a_b) [Header](../include/a_b.h)\n\n![Plot](plot_one.png)\n')
        catalogue = self.catalogue()
        manual = catalogue.manuals[0]
        rendered = fragment(manual.body, reference=(manual, catalogue))
        self.assertIn(r'\href{https://example.org/a\_b}', rendered)
        self.assertIn('include/a\\_b.h', rendered)
        image = re.search(r'\\includegraphics\[[^]]*\]\{(.*?)\}\n', rendered)
        self.assertIsNotNone(image)
        self.assertNotIn(r'\allowbreak', image[0])
        self.assertIn(r'plot\mdchar{95}one.png', image[0])
        _, file_url = catalogue.resolve_link(manual, 'plot_one.png')
        _, graphic_url = catalogue.resolve_link(manual, 'plot_one.png', for_typesetting=True)
        self.assertEqual((catalogue.pdf_directory / file_url).resolve(),
                         (catalogue.tex_directory / graphic_url).resolve())

    def test_cleaner_preserves_indented_code_and_multiline_disclosure(self):
        source = ('# One\n\n    <!-- literal indented code -->\n    <details>\n\n'
                  '<!-- maintenance\nmarker -->\n<details open>\n<summary>Types\n'
                  'and declarations</summary>\n\nThe full body.\n</details>\n')
        cleaned = clean_source(source)
        self.assertIn('    <!-- literal indented code -->\n    <details>', cleaned)
        self.assertIn('**Types and declarations**', cleaned)
        self.assertIn('The full body.', cleaned)
        self.assertNotIn('maintenance', cleaned)
        self.assertNotIn('</details>', cleaned)

    def test_unknown_fragment_falls_back_with_diagnostic(self):
        self.write('one.md', '# One\n\n[missing](#not-a-heading)\n')
        catalogue = self.catalogue()
        manual = catalogue.manuals[0]
        rendered = fragment(manual.body, reference=(manual, catalogue))
        self.assertIn(r'\hyperref[' + manual.anchor + ']', rendered)
        self.assertEqual(len(catalogue.warnings), 1)

    def test_code_wrapping_and_weighted_tables(self):
        self.write('one.md', '# One\n\n`mulib_averylongidentifierwithmanyletters_parts`\n\n'
                   '| Name | Meaning |\n| --- | --- |\n| a | A longer description of this field. |\n')
        catalogue = self.catalogue()
        manual = catalogue.manuals[0]
        rendered = fragment(manual.body, reference=(manual, catalogue))
        self.assertIn(r'\MuManualCode{mulib\_\allowbreak{}', rendered)
        self.assertIn(r'\MuManualBreak{}', rendered)
        self.assertIn(r'\hsize=', rendered)
        self.assertEqual(rendered.count(r'\textbf{Name}'), 2)
        self.assertAlmostEqual(sum(map(float, table_weights(['a & much longer description'], 2))), 2)

    def test_literal_tex_links_in_code_are_not_rewritten(self):
        self.write('one.md', '# One\n\n```tex\n\\href{#no-such-heading}{literal}\n```\n')
        catalogue = self.catalogue()
        manual = catalogue.manuals[0]
        rendered = fragment(manual.body, reference=(manual, catalogue))
        self.assertIn(r'\href{#no-such-heading}{literal}', rendered)
        self.assertFalse(catalogue.warnings)

    def test_heading_fallback_and_filename_escaping(self):
        self.write('unsafe_#{}.md', 'No document heading.\n\n## Scope\n')
        self.write('other.md', '# Other\n\n[Unsafe](unsafe_%23%7B%7D.md#scope)\n')
        catalogue = self.catalogue()
        targets = {manual.relative: manual for manual in catalogue.manuals}
        manual = targets['unsafe_#{}.md']
        self.assertEqual(manual.title, 'unsafe_#{}')
        rendered = fragment(targets['other.md'].body, reference=(targets['other.md'], catalogue))
        self.assertIn(r'\hyperref[' + manual.fragments['scope'] + ']', rendered)
        self.assertRegex(manual.anchor, r'^mu-manual-[a-f0-9]+$')


if __name__ == "__main__":
    unittest.main()
