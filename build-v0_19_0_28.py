"""Build v0.19.0.28: L83 - DPI scaling + WIC BitmapScaler + mac glass aesthetic."""
import os, re, subprocess, sys, shutil, hashlib
from pathlib import Path

ROOT = Path(r"F:\soft\00selfmade\rime_claude")
NSIS_DIR = Path(r"C:\Program Files (x86)\NSIS")
VCVARS = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"

OVERRIDES = {
    "BOOST_ROOT": r"F:\b183",
    "FLUXING_VERSION": "0.19.0",
    ""WEASEL_BUILD": "28",
    "PRODUCT_VERSION": "0.19.0.28",
    "FILE_VERSION": "0.19.0.28",
    "VERSION_MAJOR": "0",
    "VERSION_MINOR": "19",
    "VERSION_PATCH": "0",
    "FLUXING_ROOT": str(ROOT),
    "RELEASE_BUILD": "1",
}

def dump_env_from_vcvars():
    tmp = ROOT / "_tmp_vcvars.cmd"
    tmp.write_text(f'@echo off\ncall "{VCVARS}"\nset\n', encoding="ascii")
    try:
        proc = subprocess.run(
            ["cmd", "/c", str(tmp)],
            capture_output=True, text=True, timeout=120,
        )
    finally:
        tmp.unlink(missing_ok=True)
    env = {}
    for line in proc.stdout.splitlines():
        if "=" in line and not line.startswith("_="):
            k, _, v = line.partition("=")
            env[k] = v
    return env

def main():
    print(f"[build-v0.19.0.28] ROOT={ROOT}")
    vcvars_env = dump_env_from_vcvars()
    print(f"[build-v0.19.0.28] got {len(vcvars_env)} vars")
    merged = dict(vcvars_env)
    for k, v in OVERRIDES.items():
        merged[k] = v
    merged["include"] = merged.get("INCLUDE", "")

    xmake_bin = r"F:\soft\08tools\xmake\xmake.exe"

    cache_dir = ROOT / ".xmake"
    if cache_dir.exists():
        shutil.rmtree(cache_dir, ignore_errors=True)
        print(f"[build-v0.19.0.28] cleared .xmake cache")

    print(f"[build-v0.19.0.28] xmake configure -a x86 -m release -p windows ...")
    res = subprocess.run(
        [xmake_bin, "f", "-a", "x86", "-m", "release", "-p", "windows"],
        cwd=str(ROOT), env=merged, capture_output=True, text=True, timeout=180,
    )
    print(f"[build-v0.19.0.28] configure exit={res.returncode}")
    if res.returncode != 0:
        print("  stdout (tail 1500):", res.stdout[-1500:])
        print("  stderr (tail 1500):", res.stderr[-1500:])
        return res.returncode

    print(f"[build-v0.19.0.28] xmake build WeaselServer ...")
    res = subprocess.run(
        [xmake_bin, "build", "WeaselServer"],
        cwd=str(ROOT), env=merged, capture_output=True, text=True, timeout=600,
    )
    print(f"[build-v0.19.0.28] build exit={res.returncode}")
    if res.stdout:
        print("  stdout (tail 1500):", res.stdout[-1500:])
    if res.stderr:
        print("  stderr (tail 1500):", res.stderr[-1500:])
    if res.returncode != 0:
        return res.returncode

    # L82-fix: NSIS 之前先把 fluxing-logo_small.png 复制到 output/ 和 output/Win32/
    small_logo_src = ROOT / "docs" / "design" / "fluxing-logo_small.png"
    if small_logo_src.exists():
        for dest_dir in [ROOT / "output" / "Win32", ROOT / "output"]:
            shutil.copy2(small_logo_src, dest_dir / "fluxing-logo_small.png")
            print(f"[build-v0.19.0.28] copied logo -> {dest_dir / 'fluxing-logo_small.png'}")

    print(f"[build-v0.19.0.28] NSIS install.nsi ...")
    makensis = NSIS_DIR / "Bin" / "makensis.exe"
    res = subprocess.run(
        [str(makensis),
         "/DFLUXING_VERSION=0.19.0",
         "/DWEASEL_BUILD=28",
         "/DPRODUCT_VERSION=0.19.0.28",
         "install.nsi"],
        cwd=str(ROOT / "output"),
        env=merged,
        capture_output=True, text=True, timeout=120,
    )
    print(f"[build-v0.19.0.28] NSIS exit={res.returncode}")
    print(res.stdout[-2000:])
    if res.stderr:
        print("  stderr:", res.stderr[-1000:])
    if res.returncode != 0:
        return res.returncode

    src = ROOT / "output" / "archives" / "fluxing-0.19.0.28-installer.exe"
    dst = ROOT / "release" / "fluxing-0.19.0.28-installer.exe"
    if src.exists():
        shutil.copy2(src, dst)
        print(f"[build-v0.19.0.28] copied -> {dst}")
        sz = dst.stat().st_size
        print(f"[build-v0.19.0.28] size: {sz} bytes")
        h = hashlib.sha256()
        with open(dst, "rb") as f:
            for chunk in iter(lambda: f.read(65536), b""):
                h.update(chunk)
        print(f"[build-v0.19.0.28] sha256: {h.hexdigest()}")
    else:
        print(f"[build-v0.19.0.28] WARN: src {src} not found")
    return 0

if __name__ == "__main__":
    sys.exit(main())