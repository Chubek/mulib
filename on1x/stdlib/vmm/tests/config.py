"""Run the pure config facade with checked HIR and no linked ArchVirt."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

compiler, cc = sys.argv[1:]
root = Path(__file__).resolve().parents[3]
stdlib = root / "stdlib"


def run(args, expected=0):
    result = subprocess.run([str(x) for x in args], text=True, capture_output=True, timeout=30)
    assert (result.returncode == 0) == (expected == 0), (args, result)
    return result


with tempfile.TemporaryDirectory(prefix="vmm-config-", dir=Path.cwd()) as tmp:
    work = Path(tmp)
    source, generated, exe = (work / name for name in ("config.on1x", "config.c", "config"))
    source.write_text(
        """
import std.vmm as V;

fn config(backend: V.VmmBackend, ram: i64, limit: i64, domix: bool, domios: bool) -> V.VmmConfig {
  return record<V.VmmConfig>{
    machine: variant<V.VmmMachine>(VmmRiscv64, record {}), backend: backend,
    memory_mb: ram, instruction_limit: limit, domix: domix, domios: domios
  };
}

fn main() {
  let software = variant<V.VmmBackend>(VmmSoftware, record {});
  let auto = variant<V.VmmBackend>(VmmAuto, record {});
  let kvm = variant<V.VmmBackend>(VmmKvm, record {});
  let bundled = V.bundled_config(variant<V.VmmMachine>(VmmX86_64, record {}));
  assert(match V.validate_config(bundled, "", "", "", "", "") with { VmmValid() => true, _ => false });
  assert(match V.validate_config(config(kvm, 40, 1, true, true), "", "", "", "", "") with {
    VmmUnsupportedBackend() => true, _ => false
  });
  assert(match V.validate_config(config(software, 39, 1, true, true), "", "", "", "", "") with {
    VmmBadMemory() => true, _ => false
  });
  assert(match V.validate_config(config(software, 40, 0, true, true), "", "", "", "", "") with {
    VmmBadInstructionLimit() => true, _ => false
  });
  let upper = config(auto, 1024, 100000000, false, true);
  assert(match V.validate_config(upper, PATH_AT_LIMIT, "kernel", "image", "fw", CMD_AT_LIMIT) with {
    VmmValid() => true, _ => false
  });
  assert(match V.validate_config(upper, PATH_OVER_LIMIT, "kernel", "image", "fw", "") with {
    VmmPathTooLong() => true, _ => false
  });
  assert(match V.validate_config(upper, "", "kernel", "image", "fw", CMD_OVER_LIMIT) with {
    VmmCmdlineTooLong() => true, _ => false
  });
  assert(match V.validate_config(upper, "", "", "", "", "") with { VmmMissingKernel() => true, _ => false });
  let no_flags = config(software, 40, 1, false, false);
  assert(match V.validate_config(no_flags, "", "kernel", "", "fw", "") with {
    VmmFirmwareNeedsDomios() => true, _ => false
  });
  println("vmm configuration ok");
}
"""
        .replace("PATH_AT_LIMIT", json.dumps("x" * 4096))
        .replace("PATH_OVER_LIMIT", json.dumps("x" * 4097))
        .replace("CMD_AT_LIMIT", json.dumps("x" * 2048))
        .replace("CMD_OVER_LIMIT", json.dumps("x" * 2049)),
        encoding="utf-8",
    )
    run([compiler, "--stdlib", stdlib, "--emit-c", source, "-o", generated])
    c = generated.read_text(encoding="utf-8")
    assert "archvirt_" not in c and "on1x_vmm_run_v1" not in c
    runtime = root / "runtime"
    run([cc, "-std=c11", "-I", runtime, generated, runtime / "on1x_runtime.c", "-o", exe])
    assert run([exe]).stdout == "vmm configuration ok\n"
    source.write_text("import std.vmm as V; fn main() { V.start(1); }\n", encoding="utf-8")
    rejected = run([compiler, "--stdlib", stdlib, "--check", source], expected=1)
    assert "V.start" in rejected.stderr and "error:" in rejected.stderr, rejected.stderr
