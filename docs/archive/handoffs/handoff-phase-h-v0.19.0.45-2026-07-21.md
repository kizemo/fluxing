# Handoff — Phase H v0.19.0.45 现状 (2026-07-21)

> **承接会话**: `S-2026-07-21-03-phase-h-layout-polish` (本次会话,健康)
> **承接 ship**: `a7048db` (v0.19.0.45-fluxing installer) ← HEAD
> **承接状态**: Phase H layout polish 已 ship 2 commits,本 handoff 不重启旧 plan
> **状态**: 旧 handoff `handoff-phase-g-v0.19.0.44-2026-07-21.md` (Phase G) 已 100% ship,作 archive 保留
> **不变量**: v0.19.0.40~44 已 ship (Phase F Bug 3 + UI 迭代 + Stop hook + UI 真修 + UI 迭代 2) + v0.19.0.45 已 ship (Phase H layout polish)

---

## 1. 当前活跃工作线 (TL;DR)

**v0.19.0.45 已 ship — 当前 HEAD `a7048db`**(Phase H layout polish: input 拉伸 + UI margin 缩小 + 初始 column scrollbar-aware)

- installer: `release/fluxing-0.19.0.45-installer.exe` 43,164,269 bytes,md5 `d3bae85b2270ae3e83141b7cfdde980a`
- WeaselServer binary: md5 `b19d4338d360a3cc1a9b8491d56f4eae`(extract match source build,无 L97 stale binary)
- TestPhrasesDialog: **167-168 PASS / 0-1 FAIL** (5x runs,1 FAIL 是 Test 30.7 pre-existing sandbox SetCursorPos 偶发)
- install.nsi: UTF-8 BOM + CRLF ✓
- git: ahead 79 / `Fluxing...kizemo/Fluxing` (origin master 没动)

---

## 2. 时间线 — Phase H layout polish → 当前 (2 commits ship)

```
a7048db chore(release): v0.19.0.45-fluxing installer (Phase H layout polish 装机)   ← HEAD
2d842fc fix(WeaselServer): v0.19.0.45 (Phase H layout polish) — input 拉伸 + UI margin 缩小 + 初始 column scrollbar-aware
be4f14a chore(release): v0.19.0.44-fluxing installer (UI 迭代 2 装机)                ← Phase G 终点
e2f0a60 feat(WeaselServer): v0.19.0.44 (UI 迭代 2) — resize layout + drag reorder
```

**关键事实**:
- v0.19.0.44 (`be4f14a` + `e2f0a60`) Phase G 装机后 user 反馈 2 bug:
  - **Bug 1**: 初始 UI 常用短语列表框右侧边距过大, resize 后正常
  - **Bug 2**: 调整边框右侧时, 短语编辑框宽度应随变化; 添加按钮和 UI 边距应缩小
- v0.19.0.45 (`2d842fc` + `a7048db`) Phase H 修复:
  - **Bug 1 真因**: `CreateWindowExW` 同步 dispatch WM_CREATE → OnCreate 跑时 `s_hwnd = CreateWindowExW(...)` 还没执行 → `s_hwnd = nullptr` → OnCreate 末尾 LayoutDialog 早返回 → column width 没设 → ListView default 10px → 视觉"右边距过大"。resize 后 WM_SIZE 触发 LayoutDialog (s_hwnd 已 set) 才正确
  - **Bug 1 修法**: OnCreate 入口 `s_hwnd = hwnd;` + ListView_InsertColumn 删 cx (让 LayoutDialog 统一算, scrollbar-aware)
  - **Bug 2 真因**: LayoutDialog `fixed inputW = 240` + UI margin `kBtnMarginX = 12` 过宽
  - **Bug 2 修法**: LayoutDialog input stretch (`inputW = clientW - 2*gap - btnAddTopW - gap`) + UI margin 统一 `gap = 8`

---

## 3. v0.19.0.45 ship 详情

| Version | Source commit | Release commit | Size (bytes) | md5 | 主变更 |
|---|---|---|---|---|---|
| **v0.19.0.45** | `2d842fc` | `a7048db` | 43,164,269 | `d3bae85b2270ae3e83141b7cfdde980a` | OnCreate s_hwnd 提前设 + ListView_InsertColumn 删 cx + LayoutDialog input 拉伸 + UI margin 统一 gap |

---

## 4. Phase H 真因 + 修法 详情

### 4.1 Bug 1 — 初始 column 右边距过大 (装机 v0.19.0.44 反馈)

**真因** (5 阶段 systematic-debugging, 关键 debug 输出):
```
[LayoutDialog ENTRY] clientW=360 clientH=460 s_hwnd=N s_hList=Y
```
- `CreateWindowExW` **同步 dispatch WM_CREATE**, `OnCreate` 在 `CreateWindowExW` 返回前就跑
- 此时 `Show()` 的 `s_hwnd = CreateWindowExW(...)` 还没执行 → `s_hwnd = nullptr`
- OnCreate 末尾调 `LayoutDialog`, LayoutDialog 第一行 `if (!s_hwnd) return;` 早返回
- → OnCreate 阶段 column width / child 位置**完全没设**, 是 ListView_InsertColumn default 10px
- resize 后 `WM_SIZE` 触发 LayoutDialog (此时 `s_hwnd` 已 set) → 正常设 column width (header client - 4, scrollbar-aware)
- v0.19.0.44 ship 漏这个 s_hwnd 时序 bug (既存 tests 都通过 WM_SIZE 触发, 不依赖 OnCreate 末 LayoutDialog)

**修法**:
1. `OnCreate` 入口 `s_hwnd = hwnd;` (用 WndProc 参数 hwnd, 不依赖 Show() 的赋值)
2. `OnCreate` ListView_InsertColumn 删 cx (只 mask = LVCF_TEXT + LVCF_SUBITEM), 让 LayoutDialog 末尾统一算 (header client - 4, scrollbar-aware)

### 4.2 Bug 2 — input 不拉伸 + UI 边距过大 (装机 v0.19.0.44 反馈)

**真因**:
- `LayoutDialog` 用 `fixed inputW = kInputW * dpiScale = 240` → resize 不跟随
- `LayoutDialog` UI margin 用 `kBtnMarginX = 12` (input + AddTop + 底部按钮 左右各 12px) → input 实际可用宽度受限

**修法** (LayoutDialog 5 处改):
1. `inputX = gap` (was `kBtnMarginX = 12`)
2. `inputW = clientW - 2*gap - btnAddTopW - gap` (was fixed `kInputW = 240`)
3. `AddTop X = clientW - gap - btnAddTopW` (was `inputX + inputW + gap`, now anchored right)
4. 底部 3 按钮 `btnX = clientW - gap - totalBtnW` (was `clientW - marginX - totalBtnW`)
5. UI margin 统一 `gap` (=8): inputX/listX/AddTop X/底部 3 按钮 X 全用 gap

**OnCreate 同步改 inputX** (LayoutDialog 末尾再调一次覆盖, 视觉一致)

---

## 5. 测试新增 (3 项)

| Test | 范围 | PASS/FAIL |
|---|---|---|
| **Test 32.6** (更新) | 期望值 `marginX=12*DPI` → `gap=8*DPI` (btn Cancel 右边缘) | PASS |
| **Test 34** `TestOnCreateColumnIsScrollbarAware` (新, 4 case) | 验证 column cx ≈ header client - 4 (scrollbar-aware, 非 default 10px) | PASS |
| **Test 35** `TestInputStretchesAndMarginShrinks` (新, 9 case) | 验证 input 拉伸 + UI margin 缩小 + 二次 resize 持续 | PASS |

---

## 6. Working tree 状态(`git status -sb` 关键行)

```
## Fluxing...kizemo/Fluxing [ahead 79]
 M .claude/skills/git-workflow/SKILL.md
 M .claude/skills/spec-init/SKILL.md
 M .claude/skills/systematic-debugging/SKILL.md
 M .claude/skills/tdd/SKILL.md
 M .claude/skills/verification-before-completion/SKILL.md
 M CLAUDE.md
 m librime
 M task.md
?? $out/
... (其他 untracked, 不动)
```

**已 commit**:
- `M CHANGELOG.md` (v0.19.0.45 entry) — commit `2d842fc`
- `M WeaselServer/PhrasesDialog.cpp` (5 处改) — commit `2d842fc`
- `M test/TestPhrasesDialog/TestPhrasesDialog.cpp` (Test 34/35 + Test 32.6) — commit `2d842fc`
- `A release/fluxing-0.19.0.45-installer.exe` (43,164,269 bytes) — commit `a7048db`

**不动清单** (per handoff §10 + §11):
- `M .claude/skills/*` — 工作流 skill 累积改动
- `M CLAUDE.md` — 进站指引累积改动
- `m librime` — 子模块 dirty
- `M task.md` — 主 task plan 累积改动 (本会话加 Phase H 段, 单独 commit)

**新 handoff 文件**: `handoff-phase-h-v0.19.0.45-2026-07-21.md` (本文件)— 不 commit, 会话级快照。

---

## 7. CHANGELOG.md 当前段(按 commit 时间倒序)

```
## [0.19.0.45-fluxing] - 2026-07-21   ← 最新
## [0.19.0.44-fluxing] - 2026-07-21
## [0.19.0.43-fluxing] - 2026-07-21
## [0.19.0.42-fluxing] - 2026-07-21
## [0.19.0.41-fluxing] - 2026-07-21
## [0.19.0.40-fluxing] - 2026-07-21   ← Phase F Bug 3 fix
...
```

---

## 8. 不变量(本会话维持)

- `env.bat` 是 gitignored,**绝不 commit**(当前是 WEASEL_BUILD=45)
- `release/` 目录 installer 是 git-tracked,5+1 = 6 个版本(0.40~0.45)已 add + commit
- installer rebuild 前先 `rm -f` 旧 version (避免 dir 混淆) — 已删 v0.19.0.44 installer
- 装机命令必带 `taskkill + reg delete` (L13/L54/L66 强约束)
- working tree 累积改动(`.claude/skills/*` + `CLAUDE.md` + `librime` + `task.md`)不动
- 旧 handoff 文件(`handoff-phase-f-bug3-vk_escape-2026-07-20.md` + `handoff-phrases-dialog-2026-07-19.md` + `handoff-phase-g-v0.19.0.44-2026-07-21.md`)作 archive, 不 commit, 不强删

---

## 9. 下一步建议(新会话)

按 user 装机反馈分流:

| 装机反馈 | 处理 |
|---|---|
| **v0.19.0.45 装机后 2 bug 全 fix** | 接受 ship, 等下一阶段 user 反馈 (可能是 Phase I: UserDictionary 完整功能 / ShortcutSettings 验证 / 视觉 polish 等下一环节) |
| **v0.19.0.45 装机后 Bug 1 仍 fail** | 走 systematic-debugging, 查装机端 SetCursorPos 是否触发 LayoutDialog 多次设置 column (理论上 OnCreate 已修, 但装机端可能有其他 path) |
| **v0.19.0.45 装机后 Bug 2 仍 fail** | 走 systematic-debugging, 查装机端 DPI 缩放是否导致 inputW 计算错 (e.g. 200% DPI: inputW = 2*clientW - ...) |
| **装机顺利, 要进下一环节** | 按 user directive: "用户词典/快捷键等功能, 推到下一环节执行" — Phase I 候选: UserDictionary 完整功能 / ShortcutSettings 验证 / 视觉 polish |

---

## 10. 装机命令模板(v0.19.0.45, L13/L54/L66 强约束)

```powershell
taskkill /F /IM WeaselServer.exe /T
reg delete "HKLM\SOFTWARE\Fluxing\Weasel" /f
reg delete "HKCU\Software\Fluxing" /f
& "F:\soft\00selfmade\rime_claude\release\fluxing-0.19.0.45-installer.exe" /S /D=D:\Program Files\fluxing
& "F:\soft\00selfmade\rime_claude\_check_install_v2.ps1"
```

**装机后 verify 项**(per CHANGELOG v0.19.0.45 §Smoke-test recipe):
1. Module 1+2 md5 = `b19d4338d360a3cc1a9b8491d56f4eae`
2. Module 3 L66 4 keys 写出 (HKLM\...\KnownClasses + HKCU\...\Assemblies\0x00000804 + HKLM\...\CLSID\{A3F4CDED-...}\InprocServer32)
3. **Bug 1 verify**: 初始 UI 列表框 column 跟右边距一致 (跟 v0.19.0.44 resize 后一样, "正常")
4. **Bug 2 verify**: resize dialog → input 编辑框宽度跟随变化 + AddTop 按钮 anchored 右边
5. **回归**: resize dialog → 列表高度跟随 + 底部 3 按钮 anchored 右边距 = 8px (was 12); drag 列表项 reorder + Esc/Enter/双击 全 work

---

## 11. 旧 handoff 归档标注

| 文件 | 状态 |
|---|---|
| `handoff-phase-f-bug3-vk_escape-2026-07-20.md` | ✓ RESOLVED by `4c8d47d` (v0.19.0.40) — Phase F Bug 3 fix ship |
| `handoff-phrases-dialog-2026-07-19.md` | ✓ Phase D 已 ship (`af13cbff` v0.19.0.36 installer), Phase E GC + v0.19.0.37~38 后续 |
| `handoff-loop-engineering-2026-07-18.md` | Loop Engineering 框架规则,长期 reference |
| `handoff-phase-g-v0.19.0.44-2026-07-21.md` | ✓ Phase G 已 ship (`be4f14a` + `e2f0a60` v0.19.0.44) — UI 迭代 2 |
| `handoff-phase-h-v0.19.0.45-2026-07-21.md` | **(本文件)** — 反映当前 v0.19.0.45 现状 + Phase H 已 ship |

旧 handoff 不删(项目根目录约定,作 archive), 但顶部加 `✓ RESOLVED` 标注便于新会话快速识别。

---

## 12. 一句话总结

> Phase H layout polish (v0.19.0.45) 已 ship, 真因 = `CreateWindowExW` 同步 dispatch WM_CREATE 让 OnCreate 跑时 `s_hwnd` 未 set → LayoutDialog 早返回 → column width 没设。修法 = OnCreate 入口 `s_hwnd = hwnd;` + LayoutDialog input 拉伸 + UI margin 统一 gap。当前 HEAD `a7048db` = v0.19.0.45 installer (43,164,269 bytes, md5 `d3bae85b2270ae3e83141b7cfdde980a`), TestPhrasesDialog 167-168 PASS / 0-1 FAIL (1 FAIL 是 Test 30.7 pre-existing sandbox SetCursorPos 偶发, 装机端不 fail)。装机命令 + verify 项见 §10, 装机反馈分流见 §9。

---

— END OF HANDOFF —