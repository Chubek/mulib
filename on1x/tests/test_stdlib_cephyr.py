"""Compile and execute a raw-FFI On1x consumer with its opt-in companion."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile


def main():
    compiler, cc, archive, library, chisel = sys.argv[1:]
    root = Path(__file__).resolve().parents[1]
    source = root / "tests/fixtures/stdlib_cephyr/analysis.on1x"
    runtime = root / "runtime"
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        generated, exe = work / "consumer.c", work / "consumer"
        subprocess.run([compiler, "--emit-c", source, "-o", generated], check=True)
        link = [cc, "-std=c11", "-I", runtime, generated, runtime / "on1x_runtime.c",
                archive, library, f"-Wl,-rpath,{Path(library).parent}", "-o", exe]
        subprocess.run(link, check=True)
        # An arbitrary caller's config must never select tools, target scripts or hooks.
        (work / "Cephyr.toml").write_text("[invalid TOML")
        env = dict(os.environ, CEPHYR_CONFIG=str(work / "Cephyr.toml"))
        asm = work / "square.s"
        with asm.open("wb") as output:
            subprocess.run([exe], cwd=work, check=True, env=env, stdout=output)
        harness = work / "square.c"
        harness.write_text("int square(int); int main(void){return square(-7) != 49;}\n")
        subprocess.run([cc, harness, asm, "-o", work / "square"], check=True)
        subprocess.run([work / "square"], check=True)

        # Result copies use normal On1x text allocation and its fail-closed behavior.
        subprocess.run(link + ["-DON1X_DATA_MALLOC(n)=((void)(n),(void*)0)"], check=True)
        failure = subprocess.run([exe], cwd=work, env=env, capture_output=True)
        assert failure.returncode == 73 and b"text allocation failure" in failure.stderr, failure

        nul = work / "nul.on1x"
        nul.write_text(r'import std.cephyr as C; fn main(){C.check(buffer_text(b!"a\0b"));}')
        subprocess.run([compiler, "--emit-c", nul, "-o", generated], check=True)
        subprocess.run(link, check=True)
        failure = subprocess.run([exe], cwd=work, env=env, capture_output=True)
        assert failure.returncode == 73 and b"NUL cannot be converted" in failure.stderr, failure

        gate = subprocess.run([compiler, "--check-memory", source],
                              capture_output=True, text=True)
        if chisel == "1":
            assert gate.returncode == 1 and "raw external" in gate.stderr, gate.stderr
        else:
            assert gate.returncode == 2 and "memory checking unavailable" in gate.stderr, gate.stderr


if __name__ == "__main__":
    main()
