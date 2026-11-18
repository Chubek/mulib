"""On1x source formatting, round trips, and target escaping."""
from pathlib import Path
import shutil
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
        run(Path(directory))
    print("On1x format configuration, round trips, C11 execution and markup passed")
