# Handoff — Phase K6: v0.19.0.58 aiec staging SFTP 同步完成 (2026-07-23)

> **承接 HEAD**: `642cbb0` (docs(memory): L107 v0.19.0.58 WS_POPUP message loop + x64 stale binary)
> **承接 Ship**: v0.19.0.58 已 ship + 已 SFTP 上传 aiec staging remote
> **Installer**: `F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.58-installer.exe`
> **aiec staging remote**: `www.aiec.fun/pinyin/{fluxing-0.19.0.58-installer.exe, version.json, appcast.xml, release-notes.html}` 4/4 md5 verify PASS
> **装机验证**: Enter 智能 + Esc 退出 user flow PASS (v0.19.0.58 装机端 OK)

---

## 1. Phase K6 完成总结

### 1.1 Phase K5 hotfix 装机验证 (user 反馈)
- ✅ **Bug 3 修复**: 编辑栏输入文字 + Enter → 智能 add (无选中新加) / edit (有选中改) — PASS
- ✅ **Bug 4 修复**: Esc 键 → 退出常用短语 UI — PASS
- ✅ TestPhrasesDialog.exe 212/0 PASS (sandbox Win32 Release MSBuild path)

### 1.2 SFTP 上传 (Phase K6) 4/4 md5 verify PASS

| 文件 | Local md5 | Server md5 | Match |
|---|---|---|---|
| `fluxing-0.19.0.58-installer.exe` | `312053473F9D41FCA65EF659F2185B9A` | `312053473F9D41FCA65EF659F2185B9A` | ✓ |
| `version.json` | `AC0A4002D93132624D6BF45A2D5AA1AA` | `AC0A4002D93132624D6BF45A2D5AA1AA` | ✓ |
| `appcast.xml` | `6B9A755B17E049861C0CB008198E0D62` | `6B9A755B17E049861C0CB008198E0D62` | ✓ |
| `release-notes.html` | `D02986900192BCFDD9E437DCDD552642` | `D02986900192BCFDD9E437DCDD552642` | ✓ |

工具: Python paramiko 4.0.0 + SFTP, host=47.120.26.175 user=kizemo (凭据来源 `E:\办公文件\L1网站\.vscode\sftp.json`)。脚本: `_sftp_v058.py` (Phase K6 写, ship gate 复用)。

### 1.3 Lessons 沉淀 (LE §6.1.3)

- ✅ **L107** 已写入 `.specify/memory/lessons-learned.md` (合并 2 root cause — WS_POPUP bubble + x64 stale binary)
  - AP-L107-A: WS_POPUP main loop 不用 IsDialogMessage → 子控件键盘不 bubble
  - AP-L107-B: sandbox test 用 SendMessage(hwnd, ...) 假阳 (绕 focus routing) — 同 L100-PhaseF-Bug3 false-positive pattern
  - AP-L107-C: `_buildflow.cmd` 不 rebuild x64 → installer stale binary trap (同 L106 build infra zero-file 类问题)
  - AP-L107-D: ListView LVN_KEYDOWN 路径不适用 Edit/Button (无 notification)
  - 关联: L100 · L101 · L106 · 9f8a796 · bbbe843

---

## 2. 当前 ship 状态 (HEAD = 642cbb0)

```
642cbb0 docs(memory): L107 v0.19.0.58 WS_POPUP message loop + x64 stale binary (Phase K5 hotfix 教训)
bbbe843 chore(release): v0.19.0.58 installer
9f8a796 fix(FluxingPhrasesDialog): v0.19.0.58 (Phase K5 Bug 3+4 真修 hotfix) — input 子控件 Enter/Esc bubble via subclass
ddb8acb chore(release): v0.19.0.57 installer   ← Phase K4 baseline
11eeb7a fix(FluxingPhrasesDialog): v0.19.0.57 (Phase K4 UX polish)
88280dd chore(release): v0.19.0.56 installer   ← Phase K3 ship baseline
44292d7 fix(WeaselServer + FluxingPhrasesDialog + installer): v0.19.0.56 (Phase K3 T019 catastrophic-recovery)
```

### 2.1 Working tree 状态

- **Modified (本会话 ship)**: `FluxingPhrasesDialog/PhrasesDialog.{cpp,h}` + `test/TestPhrasesDialog/TestPhrasesDialog.cpp` + `test/TestPhrasesDialog/TestPhrasesDialog.vcxproj` + `_check_install_v2.ps1` (Phase K5 fix 9f8a796) + `.specify/memory/lessons-learned.md` + `_sftp_v058.py` (Phase K6 docs/scripts 642cbb0)
- **Modified (prior session 残留, 不动)**: `.claude/skills/*` (5 files) · `CLAUDE.md` · `librime` (submodule dirty) · `task.md`
- **Untracked (prior session 残留, 不动)**: `$extractDir/` · `$out/` · `.claude/{agents,hooks,apple-preview.html,design-md.html,what-is-design-md.html,rules/ui-design-tokens.md}` · `.specify/{memory/.learnings,specs/040-fluent-ui-tokens,specs/051-out-of-process-phrases-dialog}` · `docs/{adr,agents,design,superpowers}` · `handoff-*.md` (历史 11+ 个) · `memory/2026-07-{18,20,21,22}.md` · `report-*.md` (5 个) · `build_v046/v048/v052.ps1` · `_prompt_T019_new_session.md` · `CONTEXT.md` · `task-*.md` · `output/_smoke_test_v38*.ps1` · `output/fluxing-logo_small.png` · `release/fluxing-0.19.0.54-installer.exe` · `release/v0_19_0_32_e2e.{exe,pdb}` · `test/{TestDarkModeBridge/PaletteFieldTest.{cpp,h},TestPipeProtocol/TestPipeProtocol.{exe,ilk,pdb},Verifier2_G2_G12/}`

---

## 3. 不在本次范围 (新会话不要重做)

- ❌ 不要重 build v0.19.0.58 (installer + binary 已 ship + 装机 PASS + SFTP 已上传)
- ❌ 不要重 commit (642cbb0 / bbbe843 / 9f8a796 / ddb8acb / 11eeb7a / 88280dd / 44292d7 / 8559ed4 / 823bc1f / f6200a6 / c465254 / cead012 / 05a11bd 全 ship record 保留)
- ❌ 不要改 `.specify/memory/lessons-learned.md` (L107 已 ship)
- ❌ 不要改 aiec staging remote (`www.aiec.fun/pinyin/` 已含 v0.19.0.58, 4/4 md5 verified)
- ❌ 不要改 env.bat / weasel.props / _check_install_v2.ps1 (已 commit)
- ❌ 不要动 librime submodule (Phase K5/K6 不动 RimeWithWeasel / librime API)
- ❌ 不要改 install.nsi (L66 hardening 44292d7 + L106 hardening 已 ship, 不破)
- ❌ 不要走 xmake 路径 (L106 + L107 教训, 走 `_buildflow.cmd` MSBuild Win32 + 手动 rebuild x64)

---

## 4. 关键文件索引 (新会话按需读)

| 文件 | 用途 |
|---|---|
| `FluxingPhrasesDialog/PhrasesDialog.cpp:102-144` | `s_inputOrigWndProc` + `InputSubclassProc` (Phase K5 修法核心) |
| `FluxingPhrasesDialog/PhrasesDialog.cpp:773-778` | OnCreate 末 InstallInputSubclass |
| `FluxingPhrasesDialog/PhrasesDialog.cpp:786-794` | OnDestroy 卸 input subclass |
| `FluxingPhrasesDialog/PhrasesDialog.cpp:801-830` | InstallInputSubclass + RemoveInputSubclass 实施 |
| `FluxingPhrasesDialog/PhrasesDialog.cpp:843-858` | OnKeyDown VK_RETURN 智能分支 (Phase K4 source, 经 subclass bubble 现在 work) |
| `FluxingPhrasesDialog/PhrasesDialog.cpp:881-883` | OnKeyDown VK_ESCAPE → Hide (Phase K4 source, 经 subclass bubble 现在 work) |
| `FluxingPhrasesDialog/PhrasesDialog.h:212-237` | InstallInputSubclass + RemoveInputSubclass 声明 |
| `FluxingPhrasesDialog/main.cpp:37-40` | 简单 message loop (无 IsDialogMessage, root cause 之一 — 但已通过 subclass 修复, 不需要改) |
| `test/TestPhrasesDialog/TestPhrasesDialog.cpp:1480-1548` | Test 43 (Enter bubble) + Test 44 (Esc bubble) |
| `.specify/memory/lessons-learned.md:9187-9341` | L107 (Phase K5 hotfix 教训) |
| `_sftp_v058.py` | Phase K6 SFTP upload script (paramiko 4.0.0 + 4 文件 md5 verify) — ship gate 复用 |
| `_check_install_v2.ps1:55-58` | expected md5 = v0.19.0.58 |
| `env.bat` | `WEASEL_BUILD=58`, `PRODUCT_VERSION/FILE_VERSION=0.19.0.58` |
| `E:\办公文件\L1网站\.vscode\sftp.json` | SFTP 凭据来源 (host=47.120.26.175 user=kizemo password=[REDACTED-PLEASE-ROTATE] remotePath=/var/www/wordpress) |
| `E:\办公文件\L1网站\pinyin\{fluxing-0.19.0.58-installer.exe, version.json, appcast.xml, release-notes.html}` | aiec staging 本地 + remote 已同步 |

---

## 5. 不动项 / 风险点

- **不动**: prior session 累积 modifications (`.claude/skills/*`, `CLAUDE.md`, `task.md`, `librime`) — 跟 Phase K5/K6 无关, 留给 user 判断单独 commit / discard
- **不动**: untracked files (60+) — prior session 残留
- **风险**: 用户在 `www.aiec.fun/pinyin` 看 appcast.xml 应该顶部第一项是 v0.19.0.58 — verify 浏览器访问 https://www.aiec.fun/pinyin/appcast.xml 看 XML 第一个 `<item>`
- **风险**: 用户点 Sparkle "检查更新" 应该弹 v0.19.0.58 installer — verify TSF 客户端 (如果安装) 是否能自动检测新版
- **风险**: `_sftp_v058.py` 是 Phase K6 一次性脚本 (硬编码 4 文件名 + md5), 不是通用 SFTP 工具。后续 Phase 发版时复制改 `_sftp_v059.py` 即可, 或 refactor 成通用 `sftp_sync.py` + JSON config (新会话 task scope 看 user)

---

## 6. Commit 计划 (新会话)

Phase K6 已 ship 完整, 无新 commit 必要. 如果新会话要:
- v0.19.0.59 (Phase K5.5 follow-up, 装机端再次发现新 bug): `fix(FluxingPhrasesDialog)` + `chore(release)`
- 沉淀新 L## lessons: `docs(memory): L10X <title>`
- Refactor `_sftp_v058.py` 通用化: `feat(scripts): sftp_sync.py 通用 SFTP 上传工具`

---

## 7. 上下文读取建议 (新会话第一分钟)

1. 读 `MEMORY.md` (auto-memory 索引) + 本 handoff
2. 读 `.specify/memory/lessons-learned.md` L100 / L101 / L103 / L104 / L105 / L106 / L107 (最近 8 个 lesson)
3. 跑 `git log --oneline -10` 确认 HEAD = `642cbb0`
4. 跑 `git status -sb` 确认 dirty set (per CLAUDE.md §2 必查)
5. 读 `_check_install_v2.ps1:55-58` + `env.bat` 确认版本号 + md5 预期
6. 询问 user 下一阶段任务 — K7 (Phase K5.5 follow-up?) / refactor / 完全不同方向

— END OF PHASE K6 HANDOFF —