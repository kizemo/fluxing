# Handoff — Phase L 调整 2+3 v0.19.0.63 ship 完成 (2026-07-24)

> **承接**: `handoff-after-phase-L-2-3-attempt-2026-07-24.md` (build 卡死)
> **承接**: `handoff-after-phase-L-1-2-ship-2026-07-24.md` (v0.19.0.62 完成)
> **状态**: ✅ ship 完成 (commit + tag + SFTP 都过)
> **待 user**: 装机 verify (`_check_install_v2.ps1` Module 1+2+3)

---

## 1. ship 链 (4 步全过)

### Step 1 — 源码修复 (1 commit `055cc6c`)

**`feat(WeaselServer): v0.19.0.63 — Phase L 调整 2+3 (中文标点补全 + 切输入法弹浮动栏)`**
- `WeaselServer/WeaselServerApp.cpp` + `WeaselServerApp.h` 加 WM_INPUTLANGCHANGE 钩子
  (Phase L 调整 3)
- `output/data/radical_pinyin.schema.yaml` 加 `punctuator` 段 (Phase L 调整 2)
- `CHANGELOG.md` 加 v0.19.0.63 段
- `_check_install_v2.ps1` expected md5 同步更新

**修法选择**: uncommit 状态下发现 `WeaselServerApp.cpp` 的 `WM_HOTKEY` 分支在新版被错误简化
成"无差别调 PhrasesDialogIPC::Show()" — 会把 Ctrl+Shift+K / Alt+/ 全部劫持成 phrases 弹窗。
**systematic-debugging Phase 1** 抓到这个回归后,Edit 恢复了
`if (w == ID_HOTKEY_PHRASES_DOT) { ... }` / `if (w == ID_HOTKEY_SHORTCUT) { ... }` /
`if (w == ID_HOTKEY_USER_DICT_ALT_SLASH) { ... }` 三个守卫。

### Step 2 — 真重建 (msbuild `/t:Rebuild`)

`_buildflow.cmd` 从 `/t:Build` 改成 `/t:Rebuild` (L107 修复)。
- 重建前: `output/Win32/WeaselServer.exe` 不存在 (上轮 incremental 漏链)
- 重建后: 2,186,752 bytes / mtime 2026-07-24 17:33:58 / md5 `5CA3CFBEB9B0A34E66B1F94112601401`
- 跟 v0.19.0.62 (md5 `02876AA7...`) 不同,确认真重建。

### Step 3 — installer (1 commit `0e8905f`)

`_nsis_only.cmd` 跑通:
- `output/archives/fluxing-0.19.0.63-installer.exe` 43,344,633 bytes
- md5 `0D05F9F4BB1C75754DD52C0DC3BC1CC1`
- sha256 `82990F0DEFEB0A901ACACBB32A68220CBAFE0F37242E0799A80BB64A434107E7`
- 7z extract 后 WeaselServer.exe md5 = `5CA3CFBE...` = source build md5 (无 L97 stale binary ✓)

**`chore(release): v0.19.0.63-fluxing installer (Phase L 调整 2+3)`** (1 file, release/fluxing-0.19.0.63-installer.exe)

tag `v0.19.0.63` 指向 `0e8905f` (release commit)。

### Step 4 — SFTP 同步 (cross-source verify OK)

`python sftp_sync.py --version=0.19.0.63`:
- local md5 pre-check: installer `0D05F9F4...` ✓
- atomic upload 4 文件 (installer + version.json + appcast.xml + release-notes.html)
- staged md5 verify + final md5 verify 全 OK
- `www.aiec.fun/pinyin/fluxing-0.19.0.63-installer.exe` 公开可下

---

## 2. 装机 user flow 命令 (L13/L54/L66)

```powershell
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.63-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

期望 verify:
- Module 1+2 WeaselServer.exe md5 = `5CA3CFBEB9B0A34E66B1F94112601401`
- Module 2 installer md5 = `0D05F9F4BB1C75754DD52C0DC3BC1CC1`
- Module 3 L66 4 keys 全写出 (HKLM\CTF\KnownClasses + HKCU\CTF\Assemblies\0x00000804 + HKLM\CLSID\{...}\InprocServer32)

---

## 3. 装机端 5 项 user 验收 (per CHANGELOG v0.19.0.63)

1. 切方案到 `radical_pinyin` (部件拆字) → 中文模式按 `,` 出 "，" (Phase L 调整 2 ✓)
2. 切到 `rime_ice` (拼音) → 中文模式按 `,` 出 "，" (回归测试, 其他 9 方案不受影响 ✓)
3. 切到英文 → 按 `,` 出英文 "," (回归 ✓)
4. Win+Space 切到火流猩 → 浮动设置栏自动弹 (Phase L 调整 3 ✓)
5. Win+Space 切到英文 → 不弹 ✓
6. Alt+, 隐藏 / Alt+, 显示 (v0.19.0.62 行为不变 ✓)

---

## 4. 已知风险 (承 handoff §5)

- WM_INPUTLANGCHANGE 仅在 IPC server window 收到时触发 — 装机器 user 必须跟其他 app 互动
  时才完整触发。Win+Space 在 explorer / 任意 app focus 时都会触发。
- 启动时如果 Fluxing 不是 active IME, s_fluxingHkl 会记错 → 切换检测可能失效。装机后
  第一次启动 WeaselServer 通常 active, 后续用户手动换到 Fluxing 仍可触发。
- 失败模式 (已撤回的 spec 074 v0.18.31): 服务启动自动弹 — 这次刻意不触发, 只在 user
  主动切到 Fluxing 时弹。

---

## 5. Phase L 整体进度 (更新)

**Phase L 计划内**:
- ✅ 调整 1 — v0.19.0.60 (取消用户词典) ship 完成
- ✅ 调整 1.2 — v0.19.0.61 (UserDict 按钮整槽 invisible) ship 完成
- ✅ 调整 1.3 — v0.19.0.62 (UserDict 槽位彻底删除) ship 完成
- ✅ 调整 2+3 — v0.19.0.63 (中文标点 + 切输入法弹浮动栏) ship 完成 (本次)
- **(Phase L 全部 ship 完 → 完成)**

**Phase L 之外活跃项**:
- ⏸ K3 T019 装机事故调查 — PENDING user 反馈
- ⏸ K7 GitHub push HTTP 500 — 持续 fail (走 fallback, 等下次合并 push)
- ❌ 浮动设置栏其他 polish (button 0 Schema / button 3 Account no-op)
- ❌ installer.exe 老版本链接 404 (download.php fallback)

---

## 6. 整个 backlog

| ID | 项 | 状态 | 阻塞 |
|---|---|---|---|
| K3 T019 | v0.19.0.55 catastrophic regression 调查 | PENDING | user 跑 `_check_install_v2.ps1` (现在可顺便跑 v0.19.0.63 verify, 反馈 module 1+2+3) |
| K7 | GitHub push HTTP 500 | FAIL | server-side, 下次 ship 合并 push |
| QuickPanel polish | button 0 Schema / button 3 Account no-op | TODO | 不在本 phase |
| 装机 user flow verify | v0.19.0.63 5 项验收 | PENDING | user 端 |
| 浮动栏 label | 装机 verify 后再 polish | TODO | depends on user feedback |

---

## 7. 时间预算反思

本会话 (从 Phase L 2+3 attempt 失败后重启):
- 读 handoff + Phase L 计划 + AGENTS + constitution: 5 min
- 调查 build 卡死 + systematic-debugging (Phase 1 root cause + Phase 3 fix): 15 min
- 修 WM_HOTKEY 路由回归 (systematic-debugging Phase 4 修 root cause): 5 min
- 改 `_buildflow.cmd` `/t:Build → /t:Rebuild`: 2 min
- 跑 build + NSIS + 验证 md5: 10 min
- commit + tag + SFTP: 5 min
- **总: ~45 min**

教训: 
- `task.md` / `plan-phase-L-2026-07-24.md` / handoff 三件套 → 跨会话接续的核心
- L107 (增量 build 跳过) 经验直接落到 `_buildflow.cmd` (改 `/t:Rebuild` 是 5 分钟修复)
- systematic-debugging Phase 1 抓到的 WM_HOTKEY 路由回归是 hidden bug — source 改完未编译
  之前不会暴露,真要 ship 前必查

---

## 8. 下次会话第一句话

> 承接 v0.19.0.63 ship (Phase L 调整 2+3),见
> `handoff-after-phase-L-2-3-ship-2026-07-24.md`。
> Phase L 计划内全部 ship 完成。下一步:
> 1. 等 user 跑 `_check_install_v2.ps1` 反馈 (K3 T019 调查 / v0.19.0.63 装机 verify 共用)
> 2. Phase L 之外活跃项: K3 T019 / QuickPanel polish / 老版本链接 404

— END OF HANDOFF —
