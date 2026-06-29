# 003 — Plan — Windows Build Pipeline

## Constitution Check

| Rule | Status | Notes |
|---|---|---|
| R1 intent + acceptance | OK | Spec US1/US2/US3 each have an acceptance test. |
| R2 spec vs plan | OK | spec.md has no language names; this plan names them. |
| R3 priority | OK | US1/US2 = P1, US3 = P2. |
| R4 Constitution Check | OK | This table. |
| R5 task granularity | OK | Each Txxx < 4h, 1-3 files. |
| R6 done = evidence | OK | Build log + installer hash = evidence. |
| R7 one source of truth | OK | spec/plan/tasks all under `.specify/specs/003-...`. |
| R8 specs versioned | OK | This commit. |
| R9 lookup beats memory | OK | Discovered X11/utf8/darts shims from peer fork
`F:\soft\02office\rime\weasel\librime\include\`. |

## Build Order (high-level)

1. **Boost 1.83.0** (prereq, not built) → `F:\b183\`
2. **librime x64 deps** → `librime/lib\*.lib` (glog, gtest, leveldb, marisa,
   opencc, yaml-cpp)
3. **librime x64 rime** → `librime\dist_x64\` (rime.dll, rime.lib)
4. **librime x86 deps** → rebuild as Win32
5. **librime x86 rime** → `librime\dist_x86\` (or `dist_x86_v2`)
6. **weasel x64** (msbuild Release|x64) → `output\weaselx64.dll`,
   `WeaselServer.exe`, `WeaselDeployer.exe`
7. **weasel x86** (msbuild Release|Win32) → `output\weasel.dll`,
   `output\Win32\WeaselServer.exe`, `output\Win32\WeaselDeployer.exe`
8. **NSIS install.nsi** → `output\archives\fluxing-<ver>-installer.exe`
9. **Copy** → `release\fluxing-<ver>-installer.exe`

## Environment

- `env.bat` (hardlinked at `F:\soft\00selfmade\rime\env.bat` and
  `F:\soft\00selfmade\rime\librime\env.bat`):
  - `BOOST_ROOT=F:\b183`
  - `BJAM_TOOLSET=msvc-14.3`
  - `PLATFORM_TOOLSET=v143`
  - `CMAKE_GENERATOR=Visual Studio 17 2022`
  - `ARCH=x64` (or `Win32` for x86 pass)
- `user-config.jam` at `C:\Users\Duanyi\user-config.jam` declares the
  Boost.Build msvc toolset using `vcvarsall.bat`.
- `F:\b183\tools\build\src\tools\msvc.jam` is **patched**:
  - `rewrite-setup` rule simplified (removed the `R=` line that broke
    `vcvarsall` path concatenation).
  - L1661 fast path uses `path.native`.

## Shim Headers (added to `librime\include\`)

Librime source on Windows expects these but the upstream CMake does
not install them. They were copied from the working
`F:\soft\02office\rime\weasel\librime\include\` peer:

- `librime/include/X11/keysym.h` (X.Org MIT, 73 lines)
- `librime/include/X11/keysymdef.h` (X.Org MIT, 2389 lines)
- `librime/include/utf8.h` (Nemanja Trifunovic MIT, 1555 bytes)
- `librime/include/utf8/{checked,core,cpp17,unchecked}.h` (same MIT)
- `librime/include/darts.h` (Daisuke Okanohara, BSD, 53001 bytes)
- `librime/include/COPYING.darts-clone` (license)

The librime `CMakeLists.txt` line 149 (`find_path(X11Keysym X11/keysym.h)`)
picks them up automatically once the file exists.

## Patches to upstream files (not yet committed)

- `librime/build.bat` L93: `-G%CMAKE_GENERATOR%` → `-G"%CMAKE_GENERATOR%"`
  (allows spaces in generator name).
- `librime/env.bat` is a hardlink to top-level `env.bat`.

## BOM Hygiene

- `output/install.nsi` had a **4× UTF-8 BOM** (12 bytes) before the
  content. NSIS 3.x reads the BOM bytes as `ï»¿ï»¿ï»¿ï»¿` and chokes on
  the first `;` line. Trimmed to 1× BOM. CRLF preserved.

## Tooling Notes

- **xmake is not installed** in this environment, so `xbuild.bat` is
  unavailable. We use `msbuild` directly via `weasel.sln`.
- **VS 18 Preview BuildTools 14.51** is installed alongside VS 2022 14.44.
  We force `-G "Visual Studio 17 2022"` and call `vcvars64.bat` from
  VS 2022 to avoid version confusion.
- **x86 deps take ~10 min to build** (longest is opencc + darts).
- **x64 deps take ~5 min** (cached, no X11 shim required for that pass).
- **Total end-to-end build**: ~60 min on this machine.

## Release Directory

- `release/fluxing-0.17.4.0-installer.exe` (10.5 MB)
- Hash: TBD (compute at commit time)