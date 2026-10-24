# Handoff — Phase G v0.19.0.44 现状 (2026-07-21)

> **承接会话**: `S-2026-07-21-01-phase-f-bug3-vk-escape-fix`(本次会话,健康)
> **承接 ship**: `be4f14a` (v0.19.0.44-fluxing installer) ← HEAD
> **承接状态**: Phase F Bug 3 修复已 ship 5 commits,本 handoff 不重启旧 plan
> **状态**: 旧 handoff `handoff-phase-f-bug3-vk_escape-2026-07-20.md` 已 100% ship,作 archive 保留
> **不变量**: v0.19.0.40~44 已 ship,working tree 累积改动(`.claude/skills/*` + `CLAUDE.md` + `librime` + `task.md`)不动

---

## 1. 当前活跃工作线 (TL;DR)

**v0.19.0.44 已 ship — 当前 HEAD `be4f14a`**(UI 迭代 2:resize layout + 拖动 reorder ListView entry)

- installer: `release/fluxing-0.19.0.44-installer.exe` 43,194,607 bytes,md5 `0ce7ae135a33cdbb8b4311a964157266`
- WeaselServer binary: md5 `e0612f56e030ab1299b746e629e090c0`(extract match source)
- TestPhrasesDialog: **153 PASS / 0 FAIL**
- install.nsi: UTF-8 BOM + CRLF ✓
- git: ahead 77 / `Fluxing...kizemo/Fluxing` (origin master 没动)

---

## 2. 时间线 — Phase F Bug 3 修复 → 当前 Phase G (5 commits ship)

```
be4f14a chore(release): v0.19.0.44-fluxing installer (UI 迭代 2 装机)         ← HEAD
e2f0a60 feat(WeaselServer): v0.19.0.44 (UI 迭代 2) — resize layout + drag reorder
879fa69 chore(release): v0.19.0.43-fluxing installer (UI 真修 装机)
e658e42 fix(WeaselServer): v0.19.0.43 (UI 真修) — IME ImmReleaseContext + drag screen coords + outer size AdjustWindowRect
404916d chore(release): v0.19.0.42-fluxing installer (Stop hook 修补 装机)
f3dcf00 fix(WeaselServer): v0.19.0.42 (Stop hook 修补) — OnGetMinMaxInfo DPI 缩放 + 5 项 polish
e2c17be chore(release): v0.19.0.41-fluxing installer (UI 迭代 装机)
d612720 fix(WeaselServer): v0.19.0.41 (UI 迭代) — DPI 缩放 + 鼠标拖动 resize + 长按拖动位置 + 删重复 Add 按钮
3eb1cb7 docs(ERRORS): record listview-escape-wndmock-false-positive (Recurrence 1)  ← L100 沉淀
680af24 chore(release): v0.19.0.40-fluxing installer (Phase F Bug 3 装机)
4c8d47d fix(WeaselServer): v0.19.0.40 (Phase F Bug 3 续修) — OnNotify LVN_KEYDOWN handler for Esc  ← Bug 3 真 fix
c6d8b35 chore(release): v0.19.0.39-fluxing installer (Phase F 装机)                ← 旧 handoff 承接 ship
bba3683 fix(WeaselServer): v0.19.0.39 (Phase F) — 按钮路径 Phrase UI 键盘事件路由 + Esc 退出   ← Phase F 首次试 fix (Test 24 false-positive)
```

**关键事实**:
- v0.19.0.39 (`c6d8b35` + `bba3683`) Phase F 首次 fix,装后 user 反馈 Bug 3 仍 fail → 触发 Phase F Bug 3 续修
- v0.19.0.40 (`4c8d47d` + `680af24`) Phase F Bug 3 真 fix — `OnNotify` 加 `case LVN_KEYDOWN` 处理 ListView 焦点时 Esc
- v0.19.0.41 (`d612720` + `e2c17be`) UI 迭代: DPI 缩放 + 鼠标拖动 resize + 长按拖动位置 + 删重复 Add 按钮
- v0.19.0.42 (`f3dcf00` + `404916d`) Stop hook 修补: OnGetMinMaxInfo DPI 缩放 + 5 项 polish
- v0.19.0.43 (`e658e42` + `879fa69`) UI 真修: IME ImmReleaseContext + drag screen coords + outer size AdjustWindowRect
- v0.19.0.44 (`e2f0a60` + `be4f14a`) UI 迭代 2: resize layout (内部 child 重新定位) + 拖动 reorder ListView entry

---

## 3. v0.19.0.40~44 ship 详情(installer md5 / size / 主变更)

| Version | Source commit | Release commit | Size (bytes) | md5 | 主变更 |
|---|---|---|---|---|---|
| **v0.19.0.40** | `4c8d47d` | `680af24` | 43,195,078 | `5e977318c38af5a91ae01fc510677c84` | Phase F Bug 3 续修:`OnNotify` `case LVN_KEYDOWN` (line 1023) + Test 25 |
| **v0.19.0.41** | `d612720` | `e2c17be` | 43,198,482 | `7c32a8358b1805d53054a097d8ac96ee` | DPI 缩放 + resize + 长按 drag + 删重复 Add (Bug 4) |
| **v0.19.0.42** | `f3dcf00` | `404916d` | 43,183,124 | `656e6894e8d15e3065d39519c5fa1ba1` | OnGetMinMaxInfo DPI 缩放 + 5 项 polish (Stop hook) |
| **v0.19.0.43** | `e658e42` | `879fa69` | 43,183,198 | `85b985346399c40b691f4c5e682f17a9` | IME ImmReleaseContext + drag screen coords + outer size AdjustWindowRect |
| **v0.19.0.44** | `e2f0a60` | `be4f14a` | 43,194,607 | `0ce7ae135a33cdbb8b4311a964157266` | resize layout + 拖动 reorder ListView (UI 迭代 2) |

---

## 4. Phase F Bug 3 实际 ship 状态(旧 handoff 验证)

### 4.1 `OnNotify` 已加 `case LVN_KEYDOWN`

**位置**: `WeaselServer/PhrasesDialog.cpp:1023-1036`

```cpp
case LVN_KEYDOWN: {
  // v0.19.0.40 (Phase F Bug 3 续修): ListView 焦点时按 Esc, ListView
  // 自身不处理, 转 LVN_KEYDOWN 给 parent. 原 OnNotify 缺此 case → return 0
  // → Esc 无响应. Test 24 (sandbox) false-positive 因为直接 dispatch
  // WM_KEYDOWN 到 dialog WndProc, 不模拟 ListView 焦点 + WM_NOTIFY 路径.
  // 修法: OnNotify 加 case LVN_KEYDOWN 处理 VK_ESCAPE → Hide. (跟
  // NM_DBLCLK / NM_RETURN 模式一致, 不 subclass ListView WndProc.)
  LPNMLVKEYDOWN pnkd = reinterpret_cast<LPNMLVKEYDOWN>(lp);
  if (pnkd && pnkd->wVKey == VK_ESCAPE) {
    Hide();
  }
  return 0;
}
```

### 4.2 Test 25 已加(`TestLVNKeyDownEscapeHides`)

**位置**: `test/TestPhrasesDialog/TestPhrasesDialog.cpp:869-901`

模拟完整 ListView 焦点路径:
1. Show() 创建 dialog + ListView
2. SetFocus(hList) — 显式设焦点(OnCreate 已默认 SetFocus)
3. 构造 `NMLVKEYDOWN {hdr.idFrom=1100(ID_LIST), hdr.code=LVN_KEYDOWN, wVKey=VK_ESCAPE}`
4. SendMessageW(hwnd, WM_NOTIFY, idFrom, (LPARAM)&nmlv) — 触发 WndProc → OnNotify → case LVN_KEYDOWN
5. 验证 `PhrasesDialog::s_hwnd == nullptr` (Hide 已销毁 dialog)

### 4.3 沉淀已落(commit `3eb1cb7`)

- `.specify/memory/.learnings/ERRORS.md` Pattern-Key: `listview-escape-wndmock-false-positive` (Recurrence 1)
- `.specify/memory/lessons-learned.md` L100-VK-ESCAPE-FALSE-POSITIVE
- **教训**:ListView / TreeView / Edit 等 common control 焦点时,键盘事件走 WM_NOTIFY (`LVN_KEYDOWN` / `NM_KEYDOWN` 等),不走 dialog WndProc WM_KEYDOWN。test 必须模拟完整 focus chain,否则 false-positive

---

## 5. Working tree 状态(`git status -sb` 关键行)

```
## Fluxing...kizemo/Fluxing [ahead 77]
 M .claude/skills/git-workflow/SKILL.md
 M .claude/skills/spec-init/SKILL.md
 M .claude/skills/systematic-debugging/SKILL.md
 M .claude/skills/tdd/SKILL.md
 M .claude/skills/verification-before-completion/SKILL.md
 M CLAUDE.md
 m librime
 M task.md
?? $out/
?? .claude/agents/
?? .claude/apple-preview.html
?? .claude/design-md.html
?? .claude/hooks/
?? .claude/rules/ui-design-tokens.md
?? .claude/what-is-design-md.html
?? .specify/memory/.learnings/LEARNINGS.md
?? .specify/memory/.learnings/README.md
?? .specify/memory/.learnings/SHARED/
?? .specify/specs/040-fluent-ui-tokens/
?? CONTEXT.md
?? build_test_phrases.ps1
?? diag-quickpanel.ps1
?? docs/adr/
?? docs/agents/
?? docs/design/fluxing-logo_small.png
?? docs/design/quickpanel-v1~5/
?? docs/superpowers/
?? handoff-loop-engineering-2026-07-18.md
?? handoff-phase-f-bug3-vk_escape-2026-07-20.md         ← 已 ship, archive
?? handoff-phrases-dialog-2026-07-19.md                  ← Phase D 旧 handoff, archive
?? handoff-phase-g-v0.19.0.44-2026-07-21.md             ← 本文件,新 handoff
?? memory/
?? output/_smoke_test_v38.ps1
?? output/_smoke_test_v38_v2.ps1
?? output/fluxing-logo_small.png
?? release/v0_19_0_32_e2e.exe                           ← 历史 e2e binary,非 installer
?? release/v0_19_0_32_e2e.pdb
?? report-*.md                                           ← v0.19.0.33~36 时期 report
?? task-4bug-rootcause.md
?? task-phase-f-bug3-v0.19.0.40.md                       ← v0.19.0.40 ship checklist, archive
?? task-verify-v0.19.0.33.md
?? test/TestDarkModeBridge/
?? test/Verifier2_G2_G12/
```

**不动清单**(per handoff §10 + §9 已 ship 状态保留):
- `M .claude/skills/*` — 工作流 skill 累积改动
- `M CLAUDE.md` — 进站指引累积改动
- `m librime` — 子模块 dirty
- `M task.md` — 主 task plan 累积改动

**新 handoff 文件**: `handoff-phase-g-v0.19.0.44-2026-07-21.md` (本文件)— 不 commit,会话级快照。

---

## 6. CHANGELOG.md 当前段(按 commit 时间倒序)

```
## [0.19.0.44-fluxing] - 2026-07-21   ← 最新
## [0.19.0.43-fluxing] - 2026-07-21
## [0.19.0.42-fluxing] - 2026-07-21
## [0.19.0.41-fluxing] - 2026-07-21
## [0.19.0.40-fluxing] - 2026-07-21   ← Phase F Bug 3 fix
## [0.19.0.39-fluxing] - 2026-07-20
## [0.19.0.38-fluxing] - 2026-07-20
## [0.19.0.37-fluxing] - 2026-07-20
## [0.19.0.36-fluxing] - 2026-07-19
## [0.19.0.34-fluxing] - 2026-07-18
## [0.19.0.33-fluxing] - 2026-07-17
```

---

## 7. 不变量(本会话维持)

- `env.bat` 是 gitignored,**绝不 commit**(当前是 WEASEL_BUILD=44)
- `release/` 目录 installer 是 git-tracked,5 个版本(0.40~0.44)已 add + commit
- installer rebuild 前先 `rm -f` 旧 version(避免 dir 混淆)
- Phase E git gc 已完成,不重新跑(除非 user 反馈新的 dangling objects)
- 不动 `output/_extract_*`(已 gitignored,清理不优先)
- 维持 P4 module prefix: `fix(WeaselServer)` + `chore(release)`
- 装机命令必带 `taskkill + reg delete`(L13/L54/L66 强约束)
- working tree 累积改动(`.claude/skills/*` + `CLAUDE.md` + `librime` + `task.md`)不动
- 旧 handoff 文件(`handoff-phase-f-bug3-vk_escape-2026-07-20.md` + `handoff-phrases-dialog-2026-07-19.md`)作 archive,不 commit,不强删

---

## 8. 下一步建议(新会话)

按 user 装机反馈分流:

| 装机反馈 | 处理 |
|---|---|
| **v0.19.0.44 装机后 Esc / 按钮 / resize / reorder 全 work** | 接受 ship,等下一阶段 user 反馈(可能是 Phase H:UserDictionary / ShortcutSettings 等下一环节) |
| **v0.19.0.44 装机后 Esc 仍 fail** | 启动 systematic-debugging 5 阶段:可能 v0.19.0.41~44 的改动 regression 了 Esc 路径(虽然 line 1023 仍有 `case LVN_KEYDOWN`,可能 focus chain 变了 — 如 SetFocus 时机 / Show() 多次创建 / LVN_KEYDOWN 路径被新 state 拦截) |
| **v0.19.0.44 装机后 resize / reorder 有 issue** | 走 systematic-debugging,可能 WM_SIZE LayoutDialog / LVN_BEGINDRAG / LVN_ENDDRAG 路径有问题 |
| **装机顺利,要进 Phase H(下一功能)** | 按 user directive: "用户词典/快捷键等功能,推到下一环节执行" — Phase H 候选:UserDictionary 完整功能 / ShortcutSettings 验证 / 视觉 polish |

---

## 9. 装机命令模板(v0.19.0.44,L13/L54/L66 强约束)

```powershell
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.44-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

**装机后 verify 项**(per CHANGELOG v0.19.0.44 §Verify):
1. Module 1+2 md5 = `e0612f56e030ab1299b746e629e090c0`
2. Module 3 L66 4 keys 写出 (HKLM\...\KnownClasses + HKCU\...\Assemblies\0x00000804 + HKLM\...\CLSID\{A3F4CDED-...}\InprocServer32)
3. **Resize**: 拖 dialog 边界 → 中间 list 高度变 (input + btn 位置不变)
4. **Reorder**: 拖动 list item 到新位置 → 顺序变 + 持久化(打开 dialog 后顺序保留)
5. **回归**: Esc / ↑↓/Enter/DoubleClick / Alt+. / 按钮路径 全 work

---

## 10. 旧 handoff 归档标注

| 文件 | 状态 |
|---|---|
| `handoff-phase-f-bug3-vk_escape-2026-07-20.md` | ✓ RESOLVED by `4c8d47d` (v0.19.0.40) — Phase F Bug 3 fix ship |
| `handoff-phrases-dialog-2026-07-19.md` | ✓ Phase D 已 ship (`af13cbff` v0.19.0.36 installer),Phase E GC + v0.19.0.37~38 后续 |
| `handoff-loop-engineering-2026-07-18.md` | Loop Engineering 框架规则,长期 reference |
| `handoff-phase-g-v0.19.0.44-2026-07-21.md` | **(本文件)** — 反映当前 v0.19.0.44 现状 + Phase F Bug 3 已 ship |

旧 handoff 不删(项目根目录约定,作 archive),但顶部加 `✓ RESOLVED` 标注便于新会话快速识别。

---

## 11. 一句话总结

> Phase F Bug 3 (Esc 不退出 dialog) 已在 v0.19.0.40 ship fix,后续 v0.19.0.41~44 (UI 迭代 + Stop hook 修补 + UI 真修 + UI 迭代 2) 4 次 ship。当前 HEAD `be4f14a` = v0.19.0.44 installer (43,194,607 bytes, md5 `0ce7ae135a33cdbb8b4311a964157266`),TestPhrasesDialog 153 PASS / 0 FAIL。装机命令 + verify 项见 §9,装机反馈分流见 §8。

---

— END OF HANDOFF —