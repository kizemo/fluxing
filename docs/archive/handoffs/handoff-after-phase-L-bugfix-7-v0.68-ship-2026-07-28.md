# Handoff — Phase L Bugfix 7 v0.68 ship (2026-07-28)

> **承接**: `handoff-after-phase-L-bugfix-6-ppl-ipc-block-v0.68-2026-07-28.md`
> **状态**: ✅ **v0.68 真修已 ship** — 装机永久卡死 17 分钟 bug 修复,Patch A/B 完整 ship 进 32+64 位 shim
> **新目标**: 装机 user flow 验证 + (可选) repo hygiene cleanup

---

## 1. 一句话总结 (给 user)

输入法 v0.66 / v0.67 装机时会卡死 17 分钟的 bug,本次 v0.68 修复完成并发布到网站。新版自动 5 秒跳过无响应环节,不再永久卡死;同时把之前漏装的浮动工具栏修复完整版装进 32 位 + 64 位两个 shim。

## 2. 修复了什么 (大白话)

### Bug A: 装机卡死 17 分钟
- **症状**: 装新版本时卡在"正在部署"步骤不动,Windows 弹出"应用无响应"警告,user 必须强杀
- **原因**: Windows 把输入法服务标记为"受保护进程"(防恶意软件),`taskkill` 杀不死;新安装程序等"老输入法回应"才能继续,但老输入法永远不应 → 永久等
- **修复**: 加 5 秒保险丝 — 老输入法 5 秒不答,就跳过等待继续装。Worst case: floating bar 状态可能短暂 stale,user 按 `Ctrl+Alt+,` 重新触发即可

### Bug B: 浮动工具栏不弹
- **症状**: 切到火流猩输入法时,右上角的快速设置面板不弹出
- **原因**: v0.66 修复过一半,32 位 shim 装上了,64 位 shim 漏装
- **修复**: v0.68 完整重 build,32+64 位 shim 都包含完整修复

### 跨平台盲点 (sandbox 漏了)
- 自动测试只跑"静默安装"路径,从未测过"双击安装"路径 — 反而是双击安装才触发 bug
- **修复**: 新增 §9 GUI install 5min timeout test,自动按 Enter 跳过欢迎页

## 3. 装机端 user 期望 (v0.68 ship 后)

跑 6 项验证 (按验收 👇 顺序):

| # | 验收 | 期望 |
|---|---|---|
| 1 | 跑 v0.68 installer | 不再卡死,5-10 分钟内完成 |
| 2 | 装好重启后 | 默认 IME 不是 Fluxing (eng / 中文 各保留自己的默认) |
| 3 | Win+Space 切到 Fluxing | 浮动工具栏自动弹出 |
| 4 | Win+Space 切到英文 | 浮动工具栏不弹 |
| 5 | 按 Alt+, | 浮动工具栏 toggle (显/隐) |
| 6 | `reg query` 4 项 keys | HKLM\CTF\KnownClasses + HKCU\CTF\Assemblies\0x00000804 + HKLM\CLSID InprocServer32 + HKLM\Fluxing\Weasel InstallDir 全部存在 |

如果不想重装,跑 `_hotfix_patch_AB_v068.ps1` (admin PowerShell) 立即把新 shim 复制到 user system,跳过装机。

## 4. 已完成 (今天)

- ✅ MaintenanceGuard 5s timeout 修复 (C++ 头文件)
- ✅ 单测 7/7 PASS (含新增 T8 timeout test)
- ✅ sandbox-verify §9 GUI install test (沙箱)
- ✅ Win32 + x64 双架构 full rebuild
- ✅ NSIS 打包 v0.68 installer (43.3 MB, md5 `79e7787c7eac765d23e48e16ecac3dec`)
- ✅ 5 commits + tag v0.19.0.68
- ✅ SFTP 上传 4 文件 (installer + version.json + appcast.xml + release-notes.html)
- ✅ Server metadata live (cf-cache-status: DYNAMIC, 无需手动 purge)

## 5. Working tree 状态

```
c528728 docs(memory): 2026-07-28 v0.19.0.68 ship 台账
0be04e1 chore(verify): v0.68 expected md5 in _check_install_v2.ps1
67520c3 (tag: v0.19.0.68) chore(release): v0.19.0.68 installer
e767356 fix(WeaselDeployer): v0.68 MaintenanceGuard Connect timeout 5s
c9bb103 docs(memory): v0.66/v0.67 ship notes + L##-PhaseL-7+8
bbd0414 (基线) fix(WeaselTSF+installer): v0.19.0.67
```

**Pre-existing repo state (NOT v0.68 work, user 可选 cleanup)**:
- `librime/` submodule dirty (`deps/opencc` sub-submodule dirty) — L107 教训模式,需要 user 决策 (submodule commit 需 ADR + 用户授权 per P3)
- 80+ untracked files (`.claude/`, `docs/`, `handoff-*.md`, 各种 build/verify scripts, `release/fluxing-0.19.0.67-installer.exe` 上版 stale) — 都是 session 开始前已存在的 working tree residue

## 6. 关键文件位置

| 用途 | 路径 |
|---|---|
| v0.68 installer (本地) | `release/fluxing-0.19.0.68-installer.exe` |
| v0.68 installer (远程) | `E:\办公文件\L1网站\pinyin\fluxing-0.19.0.68-installer.exe` |
| 三件套 (远程) | `E:\办公文件\L1网站\pinyin\{version.json,appcast.xml,release-notes.html}` |
| 测试 | `Release/TestOrphanRecovery.exe` (7/7 PASS) |
| Hotfix script (双 shim) | `_hotfix_patch_AB_v068.ps1` |
| Check installer (装机端) | `_check_install_v2.ps1` (updated md5) |
| 教训沉淀 | `.specify/memory/lessons-learned.md` L##-PhaseL-7+8 |

## 7. 立刻要 user 做

1. **跑 v0.68 installer** (或 `_hotfix_patch_AB_v068.ps1`) 验证 6 项装机流程
2. **(可选) 清理 librime submodule dirty** — submodule 改动需 user 授权,agent 不擅自 commit
3. **(可选) 清理 80+ untracked files** — 大部分是 working tree residue,user 决定保留/删除
4. **(可选) push tag v0.19.0.68** — K7 push 持续 500,等网络稳定后手动 `git push origin v0.19.0.68` (per handoff `不允许 push` 规则)

## 8. Session health

- bash 用量: ~30 calls (红线 80)
- 活跃 ~1.5h (self-limit <2h)
- 全程 minimal parallel subagent (subagent 风险 per L107/L17)
- 完成时机: 9 步 ship gate 全部 commit + tag + SFTP + cross-source verify OK

## 9. 避坑提示

- 不要用 `git describe --tags` 取版本号 (CLAUDE.md 强制 `env.bat` 自己写)
- 任何 installer 改动必跑 `output/install.nsi` first byte `EF BB BF` + CRLF 检查
- silent install (`/S`) 跟 GUI install 走 install.nsi 不同分支,只测 silent 会漏 GUI hang
- NSIS `ExecWait <child>` 无 timeout,IPC deadlocks 会导致 installer 永久 hang
- `__DATE__` 宏会让 binary md5 每次 rebuild 都变,md5 cross-source check 需用 build output 即时 md5,不能用 handoff 旧值

## 10. next session 第一句话

```
承接 handoff-after-phase-L-bugfix-7-v0.68-ship-2026-07-28.md。
v0.68 ship 全部完成 + SFTP sync OK + 5 commits + tag v0.19.0.68。
本会话任务: 装机 user flow 验证 6 项 + (可选) libsrme submodule cleanup + (可选) tag push。
```

---

*End of Phase L Bugfix 7 handoff. v0.68 真修 + 真 ship 完成. user 验证决定 ship-gate PASS.*
