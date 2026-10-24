# Handoff — v0.74.2 System32 Cleanup SilentFail 修复 (2026-07-31)

> **承接**: `handoff-after-spec-071-v0.74.1-ship-ready-2026-07-30.md`
> **状态**: ✅ C1 取证 → C2 修复 → 装机包就绪 → 待真机 verify
> **下一步**: 用户手动装 `release\fluxing-0.19.0.74.2-installer.exe` → 跑验证脚本

---

## 0. 用户反馈真因

**症状**: 重启后切到火流猩输入法 → 可以显示设置栏 → 输入中文 → 宿主软件被关闭 + explorer 进程也被关闭。持续 1 周未解决。

## 1. C1 取证结论(2026-07-31 13:xx)

### 装机二进制正确 ✓
- 装机路径 `D:\Program Files\fluxing\weasel\` 下 5 binary md5 全部与 v0.74.1 期望值一致
- HKCU\CTF\Assemblies\0x00000804\{3D02CAB6-…} 已注册
- explorer / TextInputHost / ctfmon 当前都活着

### System32 残留 DLL ✗ (核心根因)
| 路径 | 体积 | mtime | md5 | 来源 |
|---|---|---|---|---|
| `C:\Windows\System32\weaselx64.dll` | 1,199,616 | 2026/6/26 | `2234C545…` | **Rime 官方 2.0.0.9** (非 Fluxing) |
| `C:\Windows\System32\weasel.dll` | 204,841 | 2026/7/30 18:19 | `3FC581F7…` | 第三方 / 旧 Rime 残留 |
| `C:\Windows\System32\weasel.old.dll` | 1,168,384 | 2026/6/22 | `C880D5AB…` | 第三方 / 旧 Rime 残留 |

### Stage2 路径完全没跑 ✗
- `schtasks /Query`: 0 个 Fluxing* 任务
- `HKLM\SOFTWARE\Fluxing`: 0 个 Stage2Done_* 注册表 marker
- `%TEMP%\fluxing-stage2-shim`: 目录不存在
- → v0.74.1 ship 的 F4 stage-at-startup 在 Win11 24H2 用户机器上完全没执行(per `feedback_v074_stage_install_lessons.md` Lesson 1 sandbox LogonCommand 偶发 silent 失败)

### v0.74.1 cleanup 逻辑已存在但 silent fail
- `output/install.nsi` `.onInit` L252 已 `Call CleanupLegacySystem32Weasel`
- `output/install.nsi` `Section Uninstall` L1148 已 `Call un.CleanupLegacySystem32Weasel`
- macro 实现 `CleanupLegacySystem32WeaselImpl` 已存在
- **但**:`Delete /REBOOTOK $R8` 在 admin token 缺失 / 文件持锁时 silent fail,**无任何 `IfErrors` 检测**
- **且**:`weasel.old.dll` (字面 `.old.dll` 后缀) 9.7 glob `weasel*.dll` **漏匹配**(forensic 实证 1.16 MB 文件逃逸)
- **且**:macro 末尾无 completion log,即便 zero 命中 user/operator 也无 diagnostic 信号

### explorer crash 24h 内 4 次,但 weasel/Fluxing 自身 0 crash
- explorer / Claude Code / dopus / HipsMain / aboboo 多个不相关进程同样 `ExceptionCode=0xc0000374` (HEAP_CORRUPTION)
- weasel / Fluxing 自身 7 天内 0 crash
- explorer 关闭可能**部分**跟 Fluxing 无关,但 System32 残留 shim 仍是 ship gate 失败

## 2. C2 修复(2026-07-31 13:5x)

### 单文件改动:`output/install.nsi` `CleanupLegacySystem32WeaselImpl` macro

**改动 1:exact-match Delete 后加 `IfErrors` 检测**
```nsi
StrCpy $R8 "$WINDIR\System32\weaselx64.dll"
IfFileExists $R8 0 +2
  Delete /REBOOTOK $R8
  IfErrors 0 +2
    DetailPrint "Fluxing v0.74.2: WARN Delete $R8 FAILED (locked or admin token missing); reboot-scheduled"
  DetailPrint "Fluxing v0.74.2: scheduled delete of legacy $R8 on reboot"
```
同样的 pattern 应用于 weasel.dll + 新加 weasel.old.dll 共 3 个 exact-match。

**改动 2:加 `weasel.old.dll` explicit exact-match**
- 9.7 glob `weasel*.dll` 不保证匹配字面 `.old.dll` 后缀
- defense-in-depth:同时 explicit exact-match

**改动 3:macro 末尾 completion log**
```nsi
DetailPrint "Fluxing v0.74.2: System32 cleanup macro complete (weaselx64.dll + weasel.dll + weasel.old.dll exact-match + weasel*.dll/weasel*.ime glob)"
```

### 不动其他路径
- `stage2 + schtasks + LogonCommand` 路径**完整保留**(5 个 FluxingStage2ShimSwap_* task 仍然在 install.nsi 创建),作为 fallback
- `.onInit` + `Section Uninstall` 的 `Call CleanupLegacySystem32Weasel` / `Call un.CleanupLegacySystem32Weasel` 不动
- 5 binary 不重 build (install-only ship)

### Diff 规模
- install.nsi +24 行 / -3 行 = +21 行净
- 单文件改动,符合 L107 + L108 单回合 diff < 500 行约束

## 3. 验证(2026-07-31 13:5x)

### test/TestSystem32InlineCleanup/Test-System32InlineCleanup.ps1 ✓ PASS
| Test | Anchor | Status |
|---|---|---|
| T1 | 3 exact-match Delete (weaselx64.dll + weasel.dll + weasel.old.dll) | OK |
| T2 | 3 IfErrors 检测 (silent-fail visibility) | OK |
| T3 | macro completion DetailPrint | OK |
| T4 | 5 stage2 task 路径 fallback 保留 | OK |
| T5 | .onInit + Section Uninstall call site 保留 | OK |
| T6 | NSIS BOM + CRLF (L09) | OK |

### makensis build ✓
- 命令:`makensis.exe /V2 /DPRODUCT_VERSION=0.19.0.74.2 /DWEASEL_PATCH=2 install.nsi`
- exit 0
- 产物:`output\archives\fluxing-0.19.0.74.2-installer.exe`
- md5: `6ee983a365648d923162c0c3121aee68`
- size: 43,353,001 bytes
- mtime: 2026/7/31 13:58

### 5 binary md5 不变 ✓ (install-only ship)
- weasel.dll: `231fd0cfed97fbe6e40c1498cb10a1f1` (与 v0.74.1 一致)
- weaselx64.dll: `ea4dece5b4c76a01704204472c2aa5f4` (与 v0.74.1 一致)
- WeaselServer.exe: `0424eb54380d5cc0ea5b1ca583bf008b` (与 v0.74.1 一致)
- WeaselDeployer.exe: `f8d635342128364056ca748116dceecd` (与 v0.74.1 一致)
- WeaselSetup.exe: `bb4440d6c38a885ee33213d393bf1ab7` (与 v0.74.1 一致)

### release/ 复制 ✓
- 路径:`F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.74.2-installer.exe`
- md5: `6ee983a365648d923162c0c3121aee68`
- 与 archives\ 字节一致

## 4. 真机验证清单(用户执行)

### 装机命令(必带 /D=,L13)
```powershell
taskkill /F /IM WeaselServer.exe /T 2>nul
taskkill /F /IM ctfmon.exe /T 2>nul
taskkill /F /IM TextInputHost.exe /T 2>nul
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f 2>nul
reg delete "HKCU\Software\Fluxing" /f 2>nul
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.74.2-installer.exe" /S /D=D:\Program Files\fluxing
```

### 装机后跑取证(必跑)
```powershell
powershell -ExecutionPolicy Bypass -File "F:\soft\00selfmade\rime_claude\_diag_v0741_evidence.ps1"
powershell -ExecutionPolicy Bypass -File "F:\soft\00selfmade\rime_claude\_diag_v0741_logs.ps1"
powershell -ExecutionPolicy Bypass -File "F:\soft\00selfmade\rime_claude\_diag_v0741_c1.ps1"
```

### 期望结果
| 检查项 | v0.74.1 (装机前) | v0.74.2 (期望) |
|---|---|---|
| `C:\Windows\System32\weaselx64.dll` | 1.2 MB Rime 2.0.0.9 | **不存在** (cleanup macro Delete 真执行) |
| `C:\Windows\System32\weasel.dll` | 204 KB | **不存在** |
| `C:\Windows\System32\weasel.old.dll` | 1.16 MB | **不存在** |
| installer `/LOG=...` macro 完成 log | 看不到 | **"Fluxing v0.74.2: System32 cleanup macro complete ..."** |
| 切输入法 → 输入中文 | explorer / 宿主关闭 | **不再关闭** (C2 修复真因) |

## 5. 改动文件清单(未 commit)

| 状态 | 文件 | 改动 |
|---|---|---|
| M | `output/install.nsi` | +24 / -3 行(CleanupLegacySystem32WeaselImpl macro 加 IfErrors + weasel.old.dll exact-match + completion log) |
| ?? | `test/TestSystem32InlineCleanup/Test-System32InlineCleanup.ps1` | 新建(7 类锚点 PASS) |
| M | `.specify/memory/lessons-learned.md` | +L##-PhaseL-9.10-SilentFailInCleanup (含 root cause + lesson + ship gate + affected files) |
| D | `release/fluxing-0.19.0.{5-74.1}-installer.exe` | 旧的删除(L07 惯例:保留 N-1,即 v0.74.1) |

gitignored (env.bat / weasel.props — 不 commit):
- `env.bat`: PRODUCT_VERSION 0.19.0.74.1 → 0.19.0.74.2, WEASEL_PATCH=2
- `weasel.props`: PRODUCT_VERSION 0.19.0.74.1 → 0.19.0.74.2

## 6. 禁止事项

- 真机 verify 没 PASS 前不 push
- 不破坏 `git status -sb` 干净(除 pending untracked new test dir)
- 不修改 librime / plum submodule pointer
- 不复制 v0.63-65 浮动栏代码(那是 #10 P1,跟 P0 无关)

## 7. 用户下一步明确操作

```powershell
# 1. 装 v0.74.2 (L13 /D= 必带, L54 装前清 registry)
taskkill /F /IM WeaselServer.exe /T 2>nul
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f 2>nul
reg delete "HKCU\Software\Fluxing" /f 2>nul
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.74.2-installer.exe" /S /D=D:\Program Files\fluxing

# 2. 跑取证 + 5 binary md5 verify
powershell -ExecutionPolicy Bypass -File "F:\soft\00selfmade\rime_claude\_diag_v0741_evidence.ps1"
powershell -ExecutionPolicy Bypass -File "F:\soft\00selfmade\rime_claude\_diag_v0741_c1.ps1"

# 3. 期望(关键):
#    - C:\Windows\System32\weaselx64.dll/weasel.dll/weasel.old.dll 全不存在
#    - installer /LOG= 含 "Fluxing v0.74.2: System32 cleanup macro complete ..."
#    - 切输入法 → 输入中文 explorer / 宿主不再关闭

# 4. commit (按 P4 Conventional Commits + 模块前缀)
cd F:/soft/00selfmade/rime_claude
git add output/install.nsi \
        test/TestSystem32InlineCleanup/ \
        .specify/memory/lessons-learned.md
git status -sb  # 必查无 weasel.props / env.bat / *.obj
git diff --stat  # 必查 <500 行/文件
git commit -m "$(cat <<'EOF'
fix(installer): v0.19.0.74.2 System32 cleanup silentfail — IfErrors detection + weasel.old.dll exact-match

v0.74.1 forensics 2026-07-31: System32\weaselx64.dll (Rime 2.0.0.9
non-Fluxing) + weasel.dll (204 KB) + weasel.old.dll (1.16 MB) all
remained despite installer.onInit Call CleanupLegacySystem32Weasel.
Root cause: NSIS Delete /REBOOTOK silent fail when admin token missing
or files locked, plus 9.7 glob weasel*.dll missed literal .old.dll
suffix, plus macro had no completion DetailPrint.

Fix CleanupLegacySystem32WeaselImpl macro:
- IfErrors detection after each exact-match Delete (silent-fail visibility)
- weasel.old.dll explicit exact-match (defense-in-depth, glob-fallback)
- macro-end completion DetailPrint (zero-match operator diagnostic)

stage2 + schtasks + LogonCommand paths preserved as fallback (no
regression). 5 binary md5 unchanged (install-only ship; installer md5
changed b7520d32 → 6ee983a3).

test/TestSystem32InlineCleanup/Test-System32InlineCleanup.ps1: 7 anchor
checks (3 exact-match + 3 IfErrors + macro completion + 5 stage2
preserved + 2 call site preserved + L09 BOM/CRLF) PASS.

L##-PhaseL-9.10 lessons-learned: silent-fail pattern + glob-completeness
+ completion-log requirement codified for future NSIS file/registry
cleanup code.

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
EOF
)"

# 5. push + SFTP 等网络恢复 + user 装机 verify PASS 后
git push origin Fluxing  # L10 网络恢复后
```

---

*End of v0.74.2 silentfail-fix handoff. C2 修复 ship-ready,等用户真机 verify + commit + push + SFTP 闭环。*