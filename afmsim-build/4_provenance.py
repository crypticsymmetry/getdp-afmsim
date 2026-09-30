"""
Step 4: write what a GetDP build is made of, next to the binary.

    python tools/getdp_real_build/4_provenance.py --exe <getdp.exe> --out <provenance.json>
        [--src "E:/Program Files/getdp-git-source"] [--arch arch-mingw-real-omp-hypre]

The GetDP source is a snapshot without git metadata (its --version says only
"4.0.0-git"), so it is identified by a content hash of the pristine tree:
every file, with a patched file's ``.orig`` backup standing in for it, the
files the patches add left out, and ``doc/texinfo/version.texi`` (rewritten
with the date by every configure) skipped. The hash is the same before and
after apply_ams_patch.py runs, and the same for any copy of the snapshot.

Also recorded: the patches and build scripts by hash, the dependency versions
read from the PETSc arch's own headers and configure log, the MSYS2 toolchain
packages, the binary's hash and its --info output.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import json
import platform
import re
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
PATCHES = HERE / "patches"
PETSC_DIR = Path(r"E:\petsc-real\petsc-3.25.5")
GMSH_INSTALL = Path(r"E:\petsc-real\gmsh-install")
MSYS2 = Path(r"C:\msys64")
#: Files the patches add to src/kernel, which a pristine tree does not have.
ADDED = {"AMS_Gradient.cpp", "TermCache.cpp"}
#: Generated into the source tree by every configure (the date is in it).
GENERATED = {"doc/texinfo/version.texi"}
PACKAGES = ("mingw-w64-x86_64-gcc", "mingw-w64-x86_64-gcc-fortran",
            "mingw-w64-x86_64-openblas", "mingw-w64-x86_64-cmake",
            "mingw-w64-x86_64-ninja", "mingw-w64-x86_64-python")


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def pristine_tree_hash(src: Path) -> dict:
    """Content hash of the unpatched GetDP source (see the module docstring)."""
    h = hashlib.sha256()
    n = 0
    for p in sorted(src.rglob("*"), key=lambda q: q.relative_to(src).as_posix()):
        if not p.is_file() or p.suffix == ".orig":
            continue
        rel = p.relative_to(src).as_posix()
        if rel in GENERATED or (p.parent.name == "kernel" and p.name in ADDED):
            continue
        orig = p.with_name(p.name + ".orig")
        body = (orig if orig.is_file() else p).read_bytes()
        h.update(rel.encode() + b"\0" + hashlib.sha256(body).digest())
        n += 1
    return {"sha256": h.hexdigest(), "files": n,
            "patched_in_place": sorted(o.relative_to(src).as_posix()[:-5]
                                       for o in src.rglob("*.orig"))}


def define(path: Path, name: str) -> str | None:
    try:
        m = re.search(r"#define\s+" + name + r"\s+(\S+)", path.read_text(errors="replace"))
    except OSError:
        return None
    return m.group(1).strip('"') if m else None


def dependencies(arch: str) -> dict:
    inc = PETSC_DIR / "include"
    ainc = PETSC_DIR / arch / "include"
    petsc = ".".join(define(inc / "petscversion.h", f"PETSC_VERSION_{k}") or "?"
                     for k in ("MAJOR", "MINOR", "SUBMINOR"))
    metis = ".".join(define(ainc / "metis.h", f"METIS_VER_{k}") or "?"
                     for k in ("MAJOR", "MINOR", "SUBMINOR"))
    options = None
    log = PETSC_DIR / arch / "lib" / "petsc" / "conf" / "configure.log"
    try:
        with open(log, encoding="utf-8", errors="replace") as fh:
            for line in fh:
                if line.startswith("Configure Options:"):
                    options = line.split(":", 1)[1].strip()
                    break
    except OSError:
        pass
    return {
        "petsc": {"version": petsc, "arch": arch, "dir": str(PETSC_DIR),
                  "configure_options": options},
        "hypre": define(ainc / "HYPRE_config.h", "HYPRE_RELEASE_VERSION"),
        "mumps": define(ainc / "dmumps_c.h", "MUMPS_VERSION"),
        "metis": metis,
        "gmsh_library": define(GMSH_INSTALL / "include" / "gmsh.h", "GMSH_API_VERSION"),
    }


def toolchain() -> dict:
    local = MSYS2 / "var" / "lib" / "pacman" / "local"
    found = {}
    names = sorted(p.name for p in local.iterdir()) if local.is_dir() else []
    for pkg in PACKAGES:
        # the directory is <name>-<version>-<release>; the version starts with a digit
        hit = [n for n in names if re.fullmatch(re.escape(pkg) + r"-\d[^-]*-\d+", n)]
        found[pkg] = hit[0][len(pkg) + 1:] if hit else None
    try:
        gcc = subprocess.run([str(MSYS2 / "mingw64" / "bin" / "gcc.exe"), "--version"],
                             capture_output=True, text=True, encoding="utf-8",
                             errors="replace", timeout=30).stdout.splitlines()[0]
    except (OSError, IndexError, subprocess.SubprocessError):
        gcc = None
    return {"gcc": gcc, "msys2_packages": found}


def run(exe: Path, flag: str) -> str:
    try:
        p = subprocess.run([str(exe), flag], capture_output=True, text=True,
                           encoding="utf-8", errors="replace", timeout=60)
    except (OSError, subprocess.SubprocessError) as exc:
        return f"<failed: {exc}>"
    return ((p.stdout or "") + (p.stderr or "")).strip()


def provenance(exe: Path, src: Path, arch: str) -> dict:
    exe = Path(exe)
    return {
        "written": _dt.datetime.now().astimezone().isoformat(timespec="seconds"),
        "host": {"machine": platform.node(), "platform": platform.platform()},
        "binary": {"file": exe.name, "sha256": sha256(exe), "bytes": exe.stat().st_size,
                   "built": _dt.datetime.fromtimestamp(exe.stat().st_mtime).astimezone()
                   .isoformat(timespec="seconds"),
                   "version": run(exe, "--version"), "info": run(exe, "--info")},
        "getdp_source": {"path": str(src), "pristine": pristine_tree_hash(src)},
        "patches": {p.name: sha256(p) for p in sorted(PATCHES.iterdir()) if p.is_file()},
        "build_scripts": {p.name: sha256(p) for p in sorted(HERE.iterdir())
                          if p.is_file() and p.suffix in (".sh", ".py")},
        "dependencies": dependencies(arch),
        "toolchain": toolchain(),
        "license": "GetDP is GPL-2.0-or-later; this binary carries the patches in "
                   "tools/getdp_real_build/patches (Docs/THIRD_PARTY_LICENSES.md).",
    }


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--src", default=r"E:\Program Files\getdp-git-source")
    ap.add_argument("--arch", default="arch-mingw-real-omp-hypre")
    a = ap.parse_args()
    data = provenance(Path(a.exe), Path(a.src), a.arch)
    Path(a.out).write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print(f"{a.out}: getdp {data['binary']['sha256'][:12]}, source "
          f"{data['getdp_source']['pristine']['sha256'][:12]} "
          f"({data['getdp_source']['pristine']['files']} files)")


if __name__ == "__main__":
    main()
