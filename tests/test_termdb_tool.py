"""Regression checks for the stdlib-only terminal database builder."""

from contextlib import closing
import hashlib
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / ".sot" / "term"
TOOL = ROOT / "tools" / "mk-termdb.py"


class TermdbTool(unittest.TestCase):
    def setUp(self):
        parent = "/tmp/opencode" if Path("/tmp/opencode").is_dir() else None
        directory = tempfile.TemporaryDirectory(prefix="mulib-termdb-", dir=parent)
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)

    def run_tool(self, output, source=None, check=True):
        args = [sys.executable, str(TOOL), str(output)]
        if source is not None:
            args += ["--source-dir", str(source)]
        return subprocess.run(args, cwd=self.root, capture_output=True, text=True, check=check)

    def test_bundled_database(self):
        if not all((SOURCE / name).is_file() for name in (
            "Terminfo.man", "Termcap.man", "vt100.txt", "ecma-48-code-sequence.txt",
            "console_commands.txt", "dec_ansi_parser.txt", "kitty-escape-seq.txt",
            "wezterm-escape-sequences.txt", "xterm-ctrl-seqs.txt",
        )):
            self.skipTest("full local .sot/term reference corpus not supplied")
        db_path = self.root / "terminal.sqlite3"
        proc = self.run_tool(db_path)
        self.assertIn("xterm-ctrl-seqs.txt", proc.stderr)
        self.assertNotIn("omitted", proc.stderr)
        with closing(sqlite3.connect(db_path)) as db:
            self.assertEqual(db.execute("PRAGMA integrity_check").fetchall(), [("ok",)])
            self.assertEqual(db.execute("PRAGMA page_size").fetchone(), (4096,))
            self.assertEqual(db.execute("PRAGMA encoding").fetchone(), ("UTF-8",))
            self.assertEqual(db.execute("SELECT * FROM termdb_meta").fetchall(), [("schema_version", "1")])
            columns = db.execute("PRAGMA table_info(termdb_codes)").fetchall()
            self.assertEqual([(row[1], row[2]) for row in columns],
                             list(zip(("name", "profile", "template", "arity", "description", "source", "line"),
                                      ("TEXT", "TEXT", "TEXT", "INTEGER", "TEXT", "TEXT", "INTEGER"))))
            code_rows = db.execute("SELECT name,profile,template,arity FROM termdb_codes").fetchall()
            codes = {(name, profile): (template, arity) for name, profile, template, arity in code_rows}
            self.assertEqual(len(codes), len(code_rows))
            for template, arity in codes.values():
                self.assertTrue(template.isascii())
                self.assertNotIn("\0", template)
                self.assertIn(arity, range(10))
            self.assertEqual(codes["CSI", "ansi"], ("\x1b[", 0))
            self.assertEqual(codes["CUP", "ansi"], ("\x1b[%i%p1%d;%p2%dH", 2))
            self.assertEqual(codes["move", "ansi"], ("\x1b[%i%p2%d;%p1%dH", 2))
            self.assertEqual(codes["ED", "ansi"], ("\x1b[%p1%dJ", 1))
            self.assertEqual(codes["ed", "ansi"], ("\x1b[J", 0))
            self.assertEqual(codes["EL", "ansi"], ("\x1b[%p1%dK", 1))
            self.assertEqual(codes["SGR", "ansi"], ("\x1b[%p1%dm", 1))
            self.assertEqual(codes["clear", "ansi"], ("\x1b[H\x1b[J", 0))
            self.assertEqual(codes["paste-on", "xterm"], ("\x1b[?2004h", 0))
            self.assertEqual(codes["mouse-off", "xterm"], ("\x1b[?1006l\x1b[?1000l", 0))
            self.assertEqual(codes["kf11", "xterm"], ("\x1b[23~", 0))
            self.assertNotIn(("paste-on", "ansi"), codes)

            sources = db.execute("SELECT name,sha256,content,status FROM termdb_sources ORDER BY rowid").fetchall()
            expected = sorted(p.relative_to(SOURCE).as_posix() for p in SOURCE.rglob("*") if p.is_file())
            self.assertEqual([row[0] for row in sources], expected)
            for name, digest, content, status in sources:
                self.assertEqual(content, (SOURCE / name).read_bytes())
                self.assertEqual(digest, hashlib.sha256(content).hexdigest())
                self.assertEqual(status, "binary" if name == "xterm-ctrl-seqs.txt" else "text")
            self.assertEqual(db.execute("SELECT source FROM termdb_warnings").fetchall(), [("xterm-ctrl-seqs.txt",)])
            # Verify physical source lines and the joins used to inspect provenance.
            refs = db.execute("""
                SELECT r.source,r.line,r.kind,r.fact_name
                FROM termdb_code_refs AS r JOIN termdb_facts AS f
                ON r.source=f.source AND r.line=f.line AND r.kind=f.kind
                    AND r.fact_name=f.name AND r.notation=f.notation
                WHERE r.name='CUP' AND r.profile='ansi' ORDER BY r.source
            """).fetchall()
            self.assertEqual([row[0] for row in refs], ["Terminfo.man", "ecma-48-code-sequence.txt", "vt100.txt"])
            for source, line, _, fact_name in refs:
                self.assertIn(fact_name.encode(), (SOURCE / source).read_bytes().split(b"\n")[line - 1])
            self.assertEqual(db.execute("SELECT DISTINCT source FROM termdb_facts ORDER BY source").fetchall(),
                             [(name,) for name in expected if name != "xterm-ctrl-seqs.txt"])
            self.assertEqual(db.execute("SELECT line FROM termdb_warnings").fetchall(), [(0,)])
        other = self.root / "other.sqlite3"
        self.run_tool(other, SOURCE)
        self.assertEqual(db_path.read_bytes(), other.read_bytes())
        self.run_tool(db_path)
        self.assertEqual(db_path.read_bytes(), other.read_bytes())

    def test_minimal_corpus(self):
        db_path = self.root / "terminal.sqlite3"
        self.run_tool(db_path, ROOT / "tests" / "fixtures" / "termdb")
        with closing(sqlite3.connect(db_path)) as db:
            codes = dict(db.execute("SELECT name, profile FROM termdb_codes"))
            self.assertEqual(codes["CUP"], "ansi")
            self.assertEqual(codes["kf11"], "xterm")
            self.assertEqual(codes["paste-on"], "xterm")
            self.assertNotIn("clear", codes)

    def test_bad_sources_are_archived_not_executed(self):
        sources = self.root / "sources"
        nested = sources / "nested"
        nested.mkdir(parents=True)
        # Text that looks like terminfo may be extracted, but is never used to
        # build an output template, even if it uses an existing code name.
        malformed = b"ansi|fixture,\r\n  clear=\\E]52;evil\\007,\r\n"
        (sources / "Terminfo.man").write_bytes(malformed)
        (sources / "Termcap.man").write_bytes(b"not a table\n")
        (nested / "opaque.dat").write_bytes(b"\0\xff\x1b\n")
        (nested / "unknown.txt").write_bytes(b"unrecognized text\n")
        doc = (SOURCE / "termscript-profile.md").read_bytes()
        (sources / "termscript-profile.md").write_bytes(doc.replace(b"| 1006 |", b"| 9006 |"))
        db_path = self.root / "fixture.sqlite3"
        proc = self.run_tool(db_path, sources)
        self.assertIn("no recognizable records", proc.stderr)
        self.assertIn("no extractor", proc.stderr)
        self.assertIn("opaque BLOB", proc.stderr)
        self.assertIn("omitted ansi/clear", proc.stderr)
        self.assertIn("omitted xterm/mouse-on", proc.stderr)
        with closing(sqlite3.connect(db_path)) as db:
            self.assertEqual(db.execute("SELECT content FROM termdb_sources WHERE name='Terminfo.man'").fetchone(),
                             (malformed,))
            self.assertEqual(db.execute("SELECT content,status FROM termdb_sources WHERE name='nested/opaque.dat'").fetchone(),
                             (b"\0\xff\x1b\n", "binary"))
            self.assertEqual(db.execute("SELECT line,notation FROM termdb_facts WHERE source='Terminfo.man'").fetchall(),
                             [(2, r"\E]52;evil\007")])
            self.assertEqual(db.execute("SELECT name FROM termdb_codes WHERE name IN ('clear','mouse-on','mouse-off')").fetchall(), [])
            self.assertEqual(db.execute("SELECT template FROM termdb_codes WHERE name='paste-on'").fetchone(),
                             ("\x1b[?2004h",))

    def test_failure_preserves_output_and_sources(self):
        output = self.root / "terminal.sqlite3"
        output.write_bytes(b"existing output")
        proc = self.run_tool(output, self.root / "missing", check=False)
        self.assertNotEqual(proc.returncode, 0)
        self.assertEqual(output.read_bytes(), b"existing output")

        sources = self.root / "sources"
        sources.mkdir()
        document = sources / "text file"
        document.write_bytes(b"source material")
        proc = self.run_tool(document, sources, check=False)
        self.assertNotEqual(proc.returncode, 0)
        self.assertEqual(document.read_bytes(), b"source material")

        blocked = self.root / "blocked"
        blocked.mkdir()
        before = sorted(self.root.iterdir())
        proc = self.run_tool(blocked, SOURCE, check=False)
        self.assertNotEqual(proc.returncode, 0)
        self.assertEqual(sorted(self.root.iterdir()), before)

        inside = sources / "generated #term?.sqlite3"
        self.run_tool(inside, sources)
        original = inside.read_bytes()
        self.run_tool(inside, sources)
        self.assertEqual(inside.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
