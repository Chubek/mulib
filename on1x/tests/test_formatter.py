"""On1x source formatting, round trips, and target escaping."""
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile


HOME = Path(__file__).resolve().parents[1]
ON1XC, CC = sys.argv[1:3]
ON1XC = Path(ON1XC).resolve()


def invoke(*args, cwd=None, input=None, expected=0):
    p = subprocess.run(args, cwd=cwd, input=input, capture_output=True, timeout=30)
    assert p.returncode == expected, (args, p.returncode, p.stdout, p.stderr)
    return p


def fmt(file, *args, **kwargs):
    return invoke(ON1XC, "fmt", file, *args, **kwargs)


def run_source(source, output):
    c = output.with_suffix(".c")
    invoke(ON1XC, "--emit-c", source, "-o", c)
    invoke(CC, "-std=c11", "-Wall", "-Wextra", "-pedantic", "-Werror", "-I",
           HOME / "runtime", c, HOME / "runtime/on1x_runtime.c", "-o", output)
    return invoke(output).stdout


def test_inplace(work):
    source = work / "inplace.on1x"
    output = work / "other.on1x"
    original = b"fn main(){println(42);}\n"
    source.write_bytes(original)
    source.chmod(0o640)
    rendered = fmt(source, "-o", "-").stdout
    assert rendered != original and source.read_bytes() == original

    for args in ((source, "--inplace"), ("-i", "--target", "plain", source)):
        source.write_bytes(original)
        assert fmt(*args).stdout == b""
        assert source.read_bytes() == rendered
        assert stat.S_IMODE(source.stat().st_mode) == 0o640
        fmt(source, "--check")

    source.write_bytes(original)
    for args in (("--inplace", "--check"), ("-o", output, "-i"),
                 ("-i", "-o", "-"), ("-i", "--target", "latex"),
                 ("--target", "troff", "--inplace")):
        result = fmt(source, *args, expected=2)
        assert not result.stdout and b"--inplace" in result.stderr
        assert source.read_bytes() == original and not output.exists()
    assert b"input file" in fmt("--inplace", expected=2).stderr
    for args in (("--help",), ("fmt", "--help")):
        help_text = invoke(ON1XC, *args).stdout
        assert b"--inplace" in help_text and b"-i" in help_text

    source.write_bytes(b"# on1xfmt: target = latex\n" + original)
    assert fmt(source).stdout.startswith(b"\\documentclass")
    plain = fmt(source, "--target", "plain").stdout
    assert fmt(source, "-i").stdout == b""
    assert source.read_bytes() == plain

    dash = work / "-"
    dash.write_bytes(original)
    assert fmt("-i", "--", "-", cwd=work).stdout == b""
    assert dash.read_bytes() == rendered

    source.write_bytes(original)
    assert fmt(source, "-o", source).stdout == b""
    assert source.read_bytes() == rendered

    link = work / "link.on1x"
    source.write_bytes(original)
    link.symlink_to(source.name)
    assert b"regular file" in fmt(link, "-i", expected=2).stderr
    assert link.is_symlink() and source.read_bytes() == original

    malformed = b"fn malformed("
    source.write_bytes(malformed)
    err = fmt(source, "--inplace", expected=1)
    assert not err.stdout and source.name.encode() in err.stderr
    assert source.read_bytes() == malformed


def test_repository_style(work):
    root = work / "repository-style"
    root.mkdir()
    config = root / ".on1xfmt"
    shutil.copyfile(HOME / ".on1xfmt", config)
    source = root / "style.on1x"
    source.write_bytes(b'fn empty(){} fn foo(x: bool)->i64{if x{return 42;}else{return 0;}}'
                       b' fn main(){println(foo(true));}')
    before = run_source(source, work / "style-before")
    expected = (b"fn empty()\n{\n}\n\nfn foo(x: bool) -> i64\n{\n"
                 b"  if x\n  {\n    return 42;\n  }\n  else\n  {\n    return 0;\n  }\n}\n"
                 b"\nfn main()\n{\n  println(foo(true));\n}\n")
    assert fmt(source, "--inplace", cwd=HOME).stdout == b""
    assert source.read_bytes() == expected
    fmt(source, "--check")
    assert before == run_source(source, work / "style-after") == b"42\n"

    protected = b"# on1xfmt: off\nfn preserved(){ }\n# on1xfmt: on\n"
    source.write_bytes(protected + b'fn main(){println("{ literal }");}')
    assert fmt(source).stdout == protected + b'fn main()\n{\n  println("{ literal }");\n}\n'

    # At a very narrow width, continuation indent can cost more than keeping
    # the entire call on one line. Search considers both group choices.
    source.write_bytes(b"fn main(){println(g(1,2));}")
    config.write_text("width = 1\nalgorithm = search\n")
    assert b"println(g(1, 2));" in fmt(source).stdout
    config.write_text("width = 1\nalgorithm = fits\nbrace_style = same-line\n")
    assert b"fn main() {\n  println(g(1,\n    2));\n}\n" == fmt(source).stdout

    source.write_bytes(
        b"fn first(){if true{println(1);}println(2);} # first\nfn second(){}"
    )
    config.write_text(
        "brace_style = allman\n"
        "nested_block_newlines = 2\n"
        "top_level_block_newlines = 3\n"
    )
    spaced = fmt(source).stdout
    assert b"  }\n\n  println(2);" in spaced
    assert b"} # first\n\n\nfn second()" in spaced


def test_multiline_syntax(work):
    source = work / "multiline.on1x"
    formatted = work / "multiline-formatted.on1x"
    doc = b"{! top documentation\r\n\t{> an opener inside a doc comment is text}"
    trailing = b"{> trailing documentation\n  # on1xfmt: off\n}"
    block = b"{- outer\r\n\t  {@ inner -} still inner @}\n  # on1xfmt: on\n-}"
    alternate = b"{@ alternate\n\t{- nested -}\n@}"
    text = b"'''text\r\n\t  # on1xfmt: off\n  {- a string, not a comment -}'''"
    raw = b"r!'''raw\r\n  ${verbatim} \\q\n\t  ' \"'''"
    data = b"b!'''A\\0\r\n\tB'''"
    original = (
        doc + b"\nfn main(){\n" + block + b"println(" + text + b");\n"
        b"println(" + raw + b");let buf=" + data + b";\n"
        b"println(" + alternate + b"buffer_len(buf));}" + trailing
    )
    source.write_bytes(original)
    rendered = fmt(source).stdout
    assert rendered != original and b"fn main() {\n" in rendered
    for atom in (doc, trailing, block, alternate, text, raw, data):
        assert rendered.count(atom) == 1
    formatted.write_bytes(rendered)
    assert fmt(formatted).stdout == rendered
    fmt(formatted, "--check")
    latex = fmt(source, "--target", "latex").stdout
    assert b"\\textit{\\{!\\ top" in latex
    assert b"\\textit{\\{-\\ outer" in latex
    assert run_source(source, work / "multiline-before") == run_source(
        formatted, work / "multiline-after"
    )

    for malformed in (b"{- unclosed\n", b"{> unclosed\n", b"fn main(){println('''unclosed\n"):
        source.write_bytes(malformed)
        err = fmt(source, "-o", formatted, expected=1)
        assert not err.stdout and source.name.encode() in err.stderr
        assert formatted.read_bytes() == rendered


def run(work):
    (work / ".on1xfmt").write_text("tabstop = 4 # spaces, not tabs\nwidth = 28\n")
    root = work / "nested"
    root.mkdir()
    p = root / "sample.on1x"
    q = root / "formatted.on1x"
    original = b'''# lead caf\xc3\xa9
macro add{rule([$x: Expression . $y: Expression])=>expression{$x+$y};}
fn combine(alpha:i64,beta:i64,gamma:i64)->i64{return alpha+beta+gamma;}
fn main(){ # keep trailing
  \t# on1xfmt: off\r
let    z=add!([40 . 2]);\r
    # on1xfmt: on\r
println(combine(100,200,300));
println(z);let q=quote{6*7};println(run(q));
println("hash # inside string \\\\ \\\"");} # EOF'''
    p.write_bytes(original)
    rendered = fmt(p, cwd=HOME).stdout
    assert rendered != original
    assert rendered.startswith(b"# lead caf\xc3\xa9\nmacro add {\n    rule")
    assert b"fn main() { # keep trailing\n" in rendered
    span_start = original.index(b"  \t# on1xfmt: off")
    span_end = original.index(b"\nprintln(combine")
    assert original[span_start:span_end] in rendered
    assert b"\n    println(combine(100,\n        200,\n        300));\n" in rendered
    assert b'\n    println("hash # inside string \\\\ \\\"");\n} # EOF\n' in rendered
    assert fmt(p, "--check", expected=1).stdout == b""
    assert fmt(p, "-o", q).stdout == b""
    assert q.read_bytes() == rendered
    assert fmt(q).stdout == rendered
    fmt(q, "--check")
    assert invoke(ON1XC, "--format", p).stdout == rendered
    original_output = run_source(p, work / "before")
    assert original_output == run_source(q, work / "after")
    assert original_output == b'600\n42\n42\nhash # inside string \\ "\n'

    # Nearest config replaces, rather than merges with, inherited settings.
    config = root / ".on1xfmt"
    config.write_text("tabstop = 3\n")
    different = fmt(p).stdout
    assert b"\n   println(combine(100, 200, 300));\n" in different
    for value in (b"tabstop=0", b"width=-1", b"width=999999999999999999999",
                  b"tabstop=1\ntabstop=2", b"unknown=1", b"tabstop 3",
                  b"algorithm=knuth-plass", b"brace_style=missing",
                  b"nested_block_newlines=0", b"top_level_block_newlines=17",
                  b"nested_block_newlines=1\nnested_block_newlines=2",
                  b"algorithm=search\nalgorithm=fits", b"brace_style=allman\nbrace_style=same-line",
                  b"tabstop=4\0", b" " * 4097):
        config.write_bytes(value)
        err = fmt(p, "-o", q, expected=1)
        assert not err.stdout and b".on1xfmt:" in err.stderr
        assert q.read_bytes() == rendered  # No partial output on any rejection.
    config.unlink()
    for source in (b"fn malformed(", b'fn main(){println("invalid \\q");}',
                   b"fn main() { }\n# on1xfmt: on",
                   b"# on1xfmt: maybe\nfn main() {}",
                   b"# on1xfmt: target = html\nfn main() {}",
                   b"# on1xfmt: target = latex\n# on1xfmt: target=troff\nfn main() {}",
                   b"# on1xfmt: off\nfn main() {}",
                   b"# on1xfmt: off\n# on1xfmt: off\n# on1xfmt: on\nfn main() {}",
                   b"# invalid UTF-8 \xff\n", b"# NUL \0\n", b" " * (2 * 1024 * 1024 + 1)):
        p.write_bytes(source)
        err = fmt(p, "-o", q, expected=1)
        assert not err.stdout and p.name.encode() in err.stderr
        assert q.read_bytes() == rendered
    p.write_bytes(b" \n\t\n")
    assert fmt(p).stdout == b""
    p.write_bytes(b"# just a comment")
    assert fmt(p).stdout == b"# just a comment\n"

    # PPX declarations are parsed without loading their program or imports.
    p.write_bytes(b'@ppx Missing "/filename/does-not-exist"\nimport missing.module;\nfn g(){println(1);}')
    assert b"@ppx Missing " in fmt(p).stdout
    # Module extension bodies are observable as source strings by native PPX.
    carrier = b"@extension   Protocols{\n  # on1xfmt: not-a-directive-here\n  protocol Eq<Self> { fn eq(a:Self,b:Self)->bool; }}"
    p.write_bytes(b"@enable Protocols\n" + carrier + b"\nfn main() { }")
    assert carrier in fmt(p).stdout
    p.write_bytes(carrier + b"\nfn main() { }")
    fmt(p, expected=1)
    assert carrier in fmt(p, "--enable", "Protocols").stdout

    # Multiline literal bytes and their '#' lines must not be reformatted.
    command = b"```/bin/echo\r\n # on1xfmt: off\n\t  'one two'```"
    p.write_bytes(b"fn main(){taskgroup{let h=launch " + command + b";println(join h);}}")
    rendered = fmt(p).stdout
    assert command in rendered
    q.write_bytes(rendered)
    assert fmt(q).stdout == rendered

    # These control-line spellings are present only as untrusted listing text.
    p.write_bytes(
        b"# ^^{%_&$} \\input{nope} ~\nfn main() { taskgroup { let h = launch "
        b'```/bin/echo "\n.sy should-not-run\n\'so missing\n\\c\\!not-a-request"```;'
        b" println(join h); } }\n"
    )
    latex = fmt(p, "--target", "latex").stdout
    assert latex.startswith(b"\\documentclass{article}")
    assert b"\\input{" not in latex and b"^^" not in latex
    assert b"\\textbackslash{}input\\{nope\\}" in latex
    assert b"\\%\\_\\&\\$" in latex
    assert invoke(ON1XC, "--emit-latex", p).stdout == latex
    if pdflatex := shutil.which("pdflatex"):
        tex = work / "safe.tex"
        tex.write_bytes(latex)
        invoke(pdflatex, "-no-shell-escape", "-halt-on-error", "-interaction=nonstopmode", tex.name, cwd=work)
        assert tex.with_suffix(".pdf").stat().st_size > 0
    roff = fmt(p, "--target", "troff").stdout
    assert b"\n.sy" not in roff and b"\n'so" not in roff
    assert b"\n\\&.sy should-not-run\n\\&'so missing\n\\ec\\e!not-a-request" in roff
    assert invoke(ON1XC, "--emit-troff", p).stdout == roff
    if groff := shutil.which("groff"):
        result = invoke(groff, "-S", "-Tutf8", input=roff)
        assert not result.stderr and b".sy should-not-run" in result.stdout.replace("‐".encode(), b"-")
    assert fmt(p, "--target", "unavailable", expected=2).stderr
    assert fmt(p, "--target", "latex", "--check", expected=2).stderr

    for target, header in (("latex", b"\\documentclass"), ("troff", b".nf"), ("plain", b"#")):
        comment = f"# on1xfmt: target = {target}\n".encode()
        source = comment + b"fn main() {\n    println(1);\n}\n"
        p.write_bytes(source)
        assert fmt(p).stdout.startswith(header)
        assert fmt(p, "--target", "plain").stdout == source
        fmt(p, "--check")  # Always on the plain source, not the default listing.
        assert run_source(p, work / "target-directive") == b"1\n"
    p.write_bytes(b"# on1xfmt: off\n  # on1xfmt: target = latex\n# on1xfmt: on\n")
    assert fmt(p).stdout == p.read_bytes()

    # A named On1xfile command uses precisely the same adapter and config.
    project = work / "project"
    project.mkdir()
    entry = project / "main.on1x"
    original = b"# on1xfmt: target = latex\nfn main(){println(42);}\n"
    entry.write_bytes(original)
    (project / ".on1xfmt").write_text("tabstop=5\n")
    targets = ("plain", "latex", "troff")
    (project / "On1xfile").write_text(
        'on1xfile 1\nkind executable\nname "listing"\ntarget "listing" "main.on1x"\n'
        + "".join(f'command "fmt-{target}" format {target} "main.on1x"\n' for target in targets)
    )
    for target in targets:
        assert invoke(ON1XC, "exec", f"fmt-{target}", cwd=project).stdout == fmt(
            entry, "--target", target
        ).stdout
    assert b"\n     println(42);\n" in fmt(entry, "--target", "plain").stdout
    assert entry.read_bytes() == original
    assert not (project / "target").exists()


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="on1x-fmt-") as directory:
        test_inplace(Path(directory))
        test_repository_style(Path(directory))
        test_multiline_syntax(Path(directory))
        run(Path(directory))
    print("On1x format configuration, round trips, C11 execution and markup passed")
