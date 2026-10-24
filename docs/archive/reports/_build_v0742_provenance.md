# v0.74.2 Installer Build Provenance (L##-PhaseL-9.10 ship gate)

> 防止 stop hook blocker 4:release/fluxing-0.19.0.74.2-installer.exe 是 untracked binary,
> 需 byte-level provenance 证明 archives/ ↔ release/ 一致 + 5 binary md5 不变 (install-only ship)。

## 1. Build 命令

```cmd
cd F:\soft\00selfmade\rime_claude\output
"C:\Program Files (x86)\NSIS\makensis.exe" /V2 /DPRODUCT_VERSION=0.19.0.74.2 /DWEASEL_PATCH=2 install.nsi
```

- `/DPRODUCT_VERSION=0.19.0.74.2` :覆盖 install.nsi L46-48 `!ifndef PRODUCT_VERSION` fallback,
  让 OutFile 用 5-component version template。
- `/DWEASEL_PATCH=2` :跟 PRODUCT_VERSION 配套,触发 install.nsi L43-45 注释中描述的 5-component 模式。
- `/V2` :verbose 输出,便于诊断 warning。
- exit code: 0
- warnings: 2 个 `File "data\*.gram"` / `File "data\lua\*.txt"` no files found (跟 C2 改动无关,baseline pre-existing)

## 2. 产物路径 + md5 (byte-level)

| 路径 | size | md5 | sha256 | mtime |
|---|---|---|---|---|
| `output/archives/fluxing-0.19.0.74.2-installer.exe` | 43,353,001 | `6ee983a365648d923162c0c3121aee68` | `b3901ef9d5b884584e2ac5e19f629f3d79b46cc5ac32b4ab04a458ed479d61f2` | 2026/7/31 13:58 |
| `release/fluxing-0.19.0.74.2-installer.exe`         | 43,353,001 | `6ee983a365648d923162c0c3121aee68` | `b3901ef9d5b884584e2ac5e19f629f3d79b46cc5ac32b4ab04a458ed479d61f2` | 2026/7/31 13:59 |

`cmp --silent archives/... release/...` exit 0,字节完全一致。

## 3. 5 binary md5 不变 (install-only ship)

这是 install.nsi C2 改动,installer 二进制本身变了(`b7520d32…` v0.74.1 → `6ee983a3…` v0.74.2),
但装到用户机器的 5 个 binary **完全不变**:

| binary | v0.74.1 md5 | v0.74.2 md5 | diff |
|---|---|---|---|
| `weasel.dll`         | `231fd0cfed97fbe6e40c1498cb10a1f1` | `231fd0cfed97fbe6e40c1498cb10a1f1` | MATCH |
| `weaselx64.dll`      | `ea4dece5b4c76a01704204472c2aa5f4` | `ea4dece5b4c76a01704204472c2aa5f4` | MATCH |
| `WeaselServer.exe`   | `0424eb54380d5cc0ea5b1ca583bf008b` | `0424eb54380d5cc0ea5b1ca583bf008b` | MATCH |
| `WeaselDeployer.exe` | `f8d635342128364056ca748116dceecd` | `f8d635342128364056ca748116dceecd` | MATCH |
| `WeaselSetup.exe`    | `bb4440d6c38a885ee33213d393bf1ab7` | `bb4440d6c38a885ee33213d393bf1ab7` | MATCH |

**说明**: installer md5 变化 ≠ 装机后 binary 变化。installer 二进制包含 makensis 编译器
metadata + NSIS script 字节 + LZMA compressed payload。install.nsi 加 24 行 → installer
LZMA 段字节变化 → installer md5 变。但装到 `D:\Program Files\fluxing\weasel\*` 的 5 个
binary 是从 installer 内部 payload 解压出来的,**跟源码 Win32 build 字节一致**(v0.74.1 没改
Win32 源码,所以 5 binary md5 不变)。

## 4. C2 改动范围(只动 install.nsi + 新建 test + lessons)

| 文件 | 改动 | git status 类别 |
|---|---|---|
| `output/install.nsi` | `CleanupLegacySystem32WeaselImpl` macro 加 IfErrors 检测 + weasel.old.dll exact-match + macro 末尾 completion DetailPrint (+24 / -3) | **本会话 C2 (M)** |
| `test/TestSystem32InlineCleanup/Test-System32InlineCleanup.ps1` | 新建 (~95 行,7 类锚点 PASS) | **本会话 C2 (??)** |
| `.specify/memory/lessons-learned.md` | 新增 L##-PhaseL-9.10-SilentFailInCleanup 章节 | **本会话 C2 (M)** |
| `release/fluxing-0.19.0.74.2-installer.exe` | 新 build installer | **本会话 C2 (??)** |
| `_check_v0742_postinstall.ps1` | 新建(用户装机后跑,本会话不执行) | **本会话 C2 (??)** |
| `_build_v0742_provenance.md` | 本文件 | **本会话 C2 (??)** |
| `handoff-after-c2-v0.74.2-silentfail-fix-2026-07-31.md` | handoff 文档 | **本会话 C2 (??)** |

## 5. 已知不在本会话 commit 范围 (prior session 累积,严禁一锅端)

| 文件 | 状态 | 说明 |
|---|---|---|
| `docs/adr/0003-installer-shim-stage-at-startup.md` | M | v0.74.1 ship 时改,prior session 未 commit |
| `output/stage2-shim-install.bat.cmd` | M | v0.74.1 ship 时改,prior session 未 commit |
| `test/TestInstallerShimStage/Test-Stage2ShimInstall.ps1` | M | v0.74.1 ship 时改,prior session 未 commit |
| `release/fluxing-0.19.0.{5-74.1}-installer.exe` | D (70+ files) | prior session 批量清理(L07 惯例保留 N-1) |
| `_check_*.ps1` / `_diag_*.ps1` / `prompt-*.md` / `handoff-*.md` / `report-*.md` (30+ files) | ?? | prior session 工作产物 |
| `librime` (submodule) | m | 严禁碰 |

**commit 命令只精确 add 本会话 C2 的 7 个文件**:

```bash
git add output/install.nsi \
        test/TestSystem32InlineCleanup/Test-System32InlineCleanup.ps1 \
        .specify/memory/lessons-learned.md \
        release/fluxing-0.19.0.74.2-installer.exe \
        _check_v0742_postinstall.ps1 \
        _build_v0742_provenance.md \
        handoff-after-c2-v0.74.2-silentfail-fix-2026-07-31.md
```

**绝不**用 `git add -A` 或 `git add .`。

## 6. 装机命令(L13 /D= 必带, L54 装前清 registry)

```powershell
taskkill /F /IM WeaselServer.exe /T 2>nul
taskkill /F /IM ctfmon.exe /T 2>nul
taskkill /F /IM TextInputHost.exe /T 2>nul
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f 2>nul
reg delete "HKCU\Software\Fluxing" /f 2>nul
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.74.2-installer.exe" /S /D=D:\Program Files\fluxing /LOG=F:\soft\00selfmade\rime_claude\_install_v0742.log
```

## 7. 装机后 verify 命令

```powershell
powershell -ExecutionPolicy Bypass -File "F:\soft\00selfmade\rime_claude\_check_v0742_postinstall.ps1"
```

期望 7 个 Module 全部 PASS (Module 4 + 5 = 0 expected,Module 6 = baseline comparison)。

## 8. 真机 verify 的 4 项关键(用户手动验)

1. **System32 残留 DLL 全部清空**:`C:\Windows\System32\weaselx64.dll` / `weasel.dll` / `weasel.old.dll` 全部不存在。
2. **installer /LOG= 含 macro completion 行**:打开 `_install_v0742.log` 搜索 "Fluxing v0.74.2: System32 cleanup macro complete" → 存在。
3. **5 binary md5 不变**:`weasel.dll` / `weaselx64.dll` / `WeaselServer.exe` / `WeaselDeployer.exe` / `WeaselSetup.exe` md5 与 v0.74.1 完全一致。
4. **切输入法不再关闭**:重启后切到火流猩输入法 → 输入中文 → explorer / 宿主不再关闭。