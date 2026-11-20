#!/usr/bin/env python3
"""Check or refresh bounded graphics-overlay metadata without promoting parity.

Only static-inline graphics entries are generated; existing ABI module records
and user-authored descriptions are retained. No third-party Python module needed.
"""
import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent
PREFIXES = ("drm-", "glx-", "gui-", "immgtk-", "input-", "retgtk",
            "wayland-", "x11-")
LEGACY = {"immgtk-canvas", "immgtk-drawlist", "immgtk-plot"}
TEXT = {"richtext", "textbuffer", "textcursor", "texteditor", "texthistory",
        "textselection"}
SUPPORT = {"container-core", "qvm", "lprowse"}
INCLUDE = re.compile(r'^\s*#\s*include\s*["<]mulib-([^">]+)\.h[">]', re.M)
SWITCH = re.compile(r'^#\s*if(?:def\s+|\s+defined\s*\(\s*)MULIB_IMPL_', re.M)
MODULES = re.compile(r'^- name: .*?(?=^- name: |\Z)', re.M | re.S)
MANIFEST_HEADER = re.compile(r'^  header: include/(mulib-.+\.h)$', re.M)
VALIDATION = "  validation: tests/run-graphics.sh; header compilation is not feature completeness.\n"
NOTEBOOK_PROFILE = re.compile(r'^  profile: notebook-desktop$', re.M)
PURPOSES = {
    "glx-vulkan": "Optional native Vulkan devices and headless command submission.",
}


def headers(root=ROOT):
    # glx-* and x11-* are also used by the separate notebook desktop overlay.
    # That profile has its own inventory, documentation and compiler gate.
    manifest = (root / "MANIFEST.yaml").read_text().split("project_files:\n", 1)[0]
    notebook = set()
    for block in MODULES.findall(manifest):
        if not NOTEBOOK_PROFILE.search(block):
            continue
        match = MANIFEST_HEADER.search(block)
        if not match:
            raise ValueError("notebook desktop manifest entry lacks a public header path")
        notebook.add(match.group(1))
    return sorted(p for p in (root / "include").glob("mulib-*.h")
                  if p.name not in notebook and p.stem[6:] not in LEGACY
                  and (p.stem[6:] in TEXT or p.stem[6:].startswith(PREFIXES)))


def group(name):
    if name in TEXT or name == "gui-undo":
        return "text"
    if name.startswith("drm-") or name == "glx-backend-drm":
        return "drm"
    if name.startswith(("wayland-", "x11-")) or name in (
            "glx-backend-wayland", "glx-backend-x11"):
        return "wire"
    if name.startswith("input-") or name in (
            "glx-event", "glx-eventloop", "glx-unix"):
        return "input"
    if name.startswith("immgtk-"):
        return "immgtk"
    if name.startswith("retgtk"):
        return "retgtk"
    if name.startswith("gui-"):
        return "gui"
    if name in ("glx-platform", "glx-present", "glx-swapchain", "glx-window"):
        return "present"
    if name.startswith(("glx-font", "glx-glyph", "glx-text", "glx-atlas",
                        "glx-icon", "glx-image")):
        return "font-image"
    if name in {"glx-path", "glx-raster", "glx-stroke", "glx-svg",
                "glx-tessellate", "glx-antialias", "glx-transform", "glx-shape",
                "glx-paint", "glx-gradient", "glx-mesh", "glx-filter"}:
        return "path"
    return "raster"


def documentation(name):
    family = group(name)
    if name in {"glx-shaderir", "glx-shaderjit", "glx-shadervm"}:
        return "manual/mulib-gfx-shader.md"
    if name == "glx-vulkan":
        return "manual/mulib-glx-vulkan.md"
    if family == "retgtk" or name in {
            "glx-egl", "glx-opengl", "glx-gpu", "glx-shaderir",
            "glx-shaderjit", "glx-shadervm", "glx-simd", "glx-tiledraster",
            "glx-rendercache", "glx-cursor", "glx-display", "glx-legacy-bridge"}:
        return "manual/graphics-overlay.md"
    return f"manual/mulib-gfx-{family}.md"


def updated_manifest(root=ROOT):
    path = root / "MANIFEST.yaml"
    manifest = path.read_text()
    if manifest.count("\nproject_files:\n") != 1:
        raise ValueError("manifest must contain one project_files section")
    before, projects = manifest.split("project_files:\n", 1)
    blocks = MODULES.findall(before)
    if not blocks:
        raise ValueError("manifest has no module entries")
    prefix = before[:before.index(blocks[0])]
    retained = []
    names = set()
    for block in blocks:
        if VALIDATION in block and not NOTEBOOK_PROFILE.search(block) and re.search(
            r'^  implementation: static-inline$', block, re.M
        ):
            continue
        retained.append(block)
        match = MANIFEST_HEADER.search(block)
        if match:
            names.add(match.group(1)[6:-2])
    overlay = headers(root)
    for header in overlay:
        name = header.stem[6:]
        if name in names:
            raise ValueError(f"graphics entry collides with a separately registered entry: {name}")
        text = header.read_text()
        if SWITCH.search(text):
            raise ValueError(f"graphics header now has an ABI switch: {name}")
        deps = sorted(set(INCLUDE.findall(text)) - SUPPORT)
        for dependency in deps:
            if not (root / "include" / f"mulib-{dependency}.h").is_file():
                raise ValueError(f"{name}: unknown dependency {dependency}")
        purpose = PURPOSES.get(name, f"Bounded {name} API; see documented scope and acceptance gates.")
        retained.append(
            f"- name: {name}\n"
            f"  header: include/{header.name}\n"
            f"  manual: {documentation(name)}\n"
            "  implementation: static-inline\n"
            "  status: bounded\n"
            f"  purpose: {purpose}\n"
            f"  dependencies: [{', '.join(deps)}]\n"
            "  example: examples/gfx-headless.c\n" + VALIDATION)
    # Do not silently register or assign status to unrelated, previously
    # unregistered headers. This command owns only the graphics overlay.
    total = len(retained)
    prefix = re.sub(r'^module_count: \d+$', f"module_count: {total}", prefix, flags=re.M)
    bounded = sum(bool(re.search(r'^  status: bounded$', block, re.M)) for block in retained)
    count_line = f"bounded_module_count: {bounded}"
    if re.search(r'^bounded_module_count:', prefix, re.M):
        prefix = re.sub(r'^bounded_module_count:.*$', count_line, prefix, flags=re.M)
    else:
        prefix = prefix.replace("modules:\n", count_line + "\nmodules:\n", 1)
    registered_files = set(re.findall(r'^- file: (.+)$', projects, re.M))
    artifacts = sorted(set(
        list((root / "tests").glob("test-gfx-*")) +
        list((root / "tests").glob("gfx-*")) +
        list((root / "tests/graphics").glob("*")) +
        list((root / "manual").glob("mulib-gfx-*.md")) +
        [root / "manual/graphics-overlay.md", root / "examples/gfx-headless.c",
         root / "tests/run-graphics.sh", root / "tools/graphics_inventory.py"]))
    for artifact in artifacts:
        relative = artifact.relative_to(root).as_posix()
        if artifact.is_file() and relative not in registered_files:
            projects += (f"- file: {relative}\n"
                         "  purpose: Bounded graphics contracts, device-free examples or regression infrastructure.\n")
    return prefix + "".join(retained) + "project_files:\n" + projects


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--list-headers", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        if args.list_headers:
            for header in headers():
                print(header.name)
            return
        expected = updated_manifest()
        path = ROOT / "MANIFEST.yaml"
        if args.write:
            path.write_text(expected)
        elif path.read_text() != expected:
            parser.exit(1, "Stale graphics metadata: run python3 tools/graphics_inventory.py --write\n")
        if not args.write:
            required = {documentation(header.stem[6:]) for header in headers()}
            required.add("examples/gfx-headless.c")
            missing = sorted(name for name in required if not (ROOT / name).is_file())
            if missing:
                parser.exit(1, "Missing graphics documentation/example: " + ", ".join(missing) + "\n")
        print(f"Graphics metadata: {len(headers())} bounded static-inline headers; dependencies verified")
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + "\n")


if __name__ == "__main__":
    main()
