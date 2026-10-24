# Handoff — Phase L 调整 1 v0.19.0.60 ship (2026-07-24)

> **承接 HEAD**: `ae34869` (refactor(WeaselServer): v0.19.0.60 — Phase L 调整 1 取消用户词典编辑模块)
> **承接 handoff**: `handoff-after-k7-push-result-2026-07-24.md`
> **状态**: ✅ 完整 ship 完成 — commit + tag + installer + SFTP 上传 + cross-source verify
> **会话时长**: 跨 2026-07-24 11:00 → ~12:00,约 1h (含 1 次 build 失败 + 修 ID_HOTKEY_USER_DICT 漏改 + 重 build)
> **user 选项**: 任务 1 已完整 ship

---

## 1. v0.19.0.60 ship artifacts

| Artifact | 值 |
|---|---|
| Commit | `ae34869` refactor(WeaselServer): v0.19.0.60 — Phase L 调整 1 取消用户词典编辑模块 |
| Tag | `v0.19.0.60` (lightweight, points at ae34869) |
| Installer | `release/fluxing-0.19.0.60-installer.exe` (43,336,947 bytes) |
| WeaselServer.exe md5 | `7E1E3FC376478F69DA41A547323498EC` (新 build 11:38) |
| FluxingPhrasesDialog.exe md5 | `C969AEB31E24D9EE1468F8869150970D` (v0.19.0.59 unchanged) |
| Installer md5 | `B3CC278B222D1685C3B162EC7D26C4AF` (新 build 11:40) |
| Installer sha256 | `3a9e761e4158f9f5a71327b63f2eabf62fe5d17c58933547980e26183f7e928b` |
| SFTP 端 | `www.aiec.fun/pinyin/` (atomic upload + cross-source verify OK) |

---

## 2. 调整 1 完成清单

| 清理项 | 状态 | 文件 |
|---|---|---|
| Ctrl+Shift+U 热键 RegisterHotKey / UnregisterHotKey | ✓ 删除 | `WeaselServer/WeaselServerApp.cpp` |
| `ID_HOTKEY_USER_DICT 9003` define | ✓ 删除 | `WeaselServer/resource.h` |
| PhrasesHotkeySubclassProc WM_HOTKEY 分支 | ✓ 删除 | `WeaselServer/WeaselServerApp.cpp` |
| QuickPanel UserDict 按钮 callback | ✓ no-op | `WeaselServer/QuickPanelDialog.cpp/h` |
| `SetQuickPanelUserDictCallback` setter | ✓ 删除 | `WeaselServer/WeaselServerApp.cpp/h` |
| `s_onUserDict` / `SetOnUserDict` / `OnShowUserDict` | ✓ 删除 | `WeaselServer/QuickPanelDialog.cpp/h` |
| `Action_OpenUserDict` enum + MakeBuiltin | ✓ 删除 | `WeaselServer/ShortcutSettings.h/cpp` |
| `UserDictionary.cpp` 1723 行 → 198 行 stub | ✓ stub | `WeaselServer/UserDictionary.cpp` |
| Verifier2 G6-G9 测试 | ✓ `#if 0` skip | `test/Verifier2_G2_G12/*` |
| v0_19_0_30_e2e P1b/P2b + Bug 2 routing | ✓ `#if 0` skip | `test/v0_19_0_30_e2e/*` |
| v0_19_0_32_e2e P1b/P2b/T_QP_UD + Bug 2 routing | ✓ `#if 0` skip | `test/v0_19_0_32_e2e/*` |
| TestQuickPanelDialog T7/T9/T10 | ✓ `#if 0` skip | `test/TestQuickPanelDialog/*` |
| TestUserDictionary main() | ✓ stub 出 SKIP | `test/TestUserDictionary/*` |
| TestShortcutSettings T06 enum 断言 | ✓ 改用 OpenPhrases vs ReselectCandidate | `test/TestShortcutSettings/*` |
| `_check_install_v2.ps1` expected md5 | ✓ 更新到 v0.19.0.60 | `_check_install_v2.ps1` |
| `env.bat` / `weasel.props` bump version | ✓ 0.19.0.59 → 0.19.0.60 (gitignored) | `env.bat` + `weasel.props` |
| `build_v060.ps1` (L107 双步 wrapper) | ✓ 新建 | `build_v060.ps1` |
| `CHANGELOG.md` v0.19.0.60 段 | ✓ 新增 | `CHANGELOG.md` |

---

## 3. 不在本阶段 (按 Phase L 计划原定)

- ❌ 调整 2 (所有方案都支持中文标点) — 留 v0.19.0.61+
- ❌ 调整 3 (切输入法弹浮动栏) — 留 v0.19.0.61+
- ❌ 浮动设置栏 UserDict 按钮槽位整槽删除 — 留 polish 后续会议
- ❌ librime `output/data/*.schema.yaml` 的 `user_dict:` 配置(那是 RIME 后端 dict,不动)
- ❌ K3 T019 装机事故调查 — PENDING (等 user 端 `_check_install_v2.ps1` 输出)
- ❌ K7 GitHub push 重试 — 仍 HTTP 500,等下次 ship 合并

---

## 4. Build 路径决策 (踩坑+修法)

### 4.1 Build 第 1 轮:失败
- **症状 1**: `TestQuickPanelDialog.cpp` 7 处 `s_onUserDict` / `SetOnUserDict` 找不到标识符 (我没意识到这个 test 文件用了 s_onUserDict 静态字段)
- **症状 2**: `WeaselServer/WeaselServerApp.cpp` `ID_HOTKEY_USER_DICT` 未声明 (漏改的 PhrasesHotkeySubclassProc 分支)
- **根因**: 一开始 grep 覆盖范围不够,s_onUserDict / ID_HOTKEY_USER_DICT 都是间接引用
- **修法**: 重新 grep + 补 `#if 0` T7/T9/T10 + 补删 ID_HOTKEY_USER_DICT 分支

### 4.2 Build 第 2 轮:成功但 WeaselServer 没真重建
- **症状**: WeaselServer.exe mtime 仍是 23:54 (昨天),md5 还是 v0.19.0.59 = `448DE044...`
- **根因**: MSBuild incremental build 检测不到 source 文件改动,直接链了上次的 WeaselServer.exe
- **修法**: 不重 build 流程,直接改 WeaselServerApp.cpp 删 `ID_HOTKEY_USER_DICT` 引用触发编译,build_v060 run 3 全 build OK,WeaselServer.exe mtime 更新到 11:38

**教训**: 任何 cleanup 后必须看 mtime + md5 验证 binary 真变了,别只看 NSIS exit 0 + installer 大小。

### 4.3 Build 第 3 轮:成功
- BuildFlow exit 0 + NSIS exit 0
- WeaselServer.exe mtime 11:38,md5 `7E1E3FC3...` (新)
- Installer mtime 11:40,md5 `B3CC278B...` (新),sha256 `3a9e761e...`

---

## 5. Ship 链 path

```
1. cd F:\soft\00selfmade\rime_claude
2. env.bat / weasel.props bump 0.19.0.59 → 0.19.0.60 (gitignored)
3. _buildflow.cmd (MSBuild weasel.sln /p:SolutionDir=...)
4. _nsis_only.cmd (makensis output/install.nsi)
5. cp output/archives/fluxing-0.19.0.60-installer.exe release/
6. certutil -hashfile release/fluxing-0.19.0.60-installer.exe MD5 → B3CC278B...
7. _check_install_v2.ps1 expectedInstallerMd5 → B3CC278B...
8. git add + commit ae34869 (refactor 21 files +897/-1755)
9. git tag v0.19.0.60 (lightweight)
10. cp release/fluxing-0.19.0.60-installer.exe E:\办公文件\L1网站\pinyin\
11. python sftp_sync.py --version=0.19.0.60 (atomic upload + cross-source verify)
12. (可选) git push kizemo v0.19.0.60 (后跑,可能 fail per K7 HTTP 500)
```

---

## 6. Memory 更新

- `~/.claude/projects/.../memory/feedback_pinyin_release_workflow.md` — **不修改**(workflow本身 OK)
- `task.md` Phase L.1 段 — 已更新 ship 完成状态 (待此 handoff commit 后)
- `MEMORY.md` — 不承载 commit log

---

## 7. 装机端 user verify (5 项 after装机 v0.19.0.60)

```
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.60-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

**期望**:
- Module 1+2: WeaselServer.exe md5 = `7E1E3FC376478F69DA41A547323498EC`
- Module 2: installer md5 = `B3CC278B222D1685C3B162EC7D26C4AF`

**5 项装机 verify**:
1. Alt+. → PhrasesDialog (回归 v0.19.0.59 ✓)
2. Ctrl+Shift+U → **无反应** (已停用,预期)
3. 浮动设置栏 UserDict 按钮 → **点击无反应** (槽位仍画,click no-op)
4. 设置栏 (快捷键设置) → 找不到"打开用户词典"选项
5. SFTP 装机路径: `https://www.aiec.fun/pinyin/fluxing-0.19.0.60-installer.exe` 可下载

---

## 8. 风险 / 不动项

- ✅ 浮动设置栏 UserDict 按钮槽位仍画 (后续 polish 整槽删)
- ✅ librime `*.schema.yaml` `user_dict:` 配置保留 (RIME 后端,跟 UI 无关)
- ✅ QuickPanel hit==2 click 仍 no-op (不报错、不输出)
- ⚠️ K3 T019 装机事故仍 PENDING (user 没发装机端 _check_install_v2.ps1 输出)

---

## 9. 下次会话第一句话

> 承接 v0.19.0.60 ship (Phase L 调整 1),见 `handoff-after-phase-L-1-ship-2026-07-24.md`。
> 下一步做 Phase L 调整 2 (所有方案都支持中文标点) 还是调整 3 (切输入法弹浮动栏)?

---

— END OF HANDOFF —
