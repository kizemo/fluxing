# ERRORS — 单次错误临时入站口

> Loop Engineering L3/L4 临时层。`Recurrence-Count ≥ 3` 自动晋升到 `lessons-learned.md` L##。
> 写入规则见同目录 `README.md`。

---

## Active（Recurrence-Count < 3，待观察）

<!--
新错误 append 在此区域。条目模板见 README.md。

晋升检查：每次新增条目后 grep Pattern-Key，若历史已有同 Pattern-Key：
  - Recurrence-Count += 1
  - 更新到对应位置
  - 若 ≥ 3：移到 lessons-learned.md 并在本文件留一条 "已晋升 → L##" 占位
-->

- [2026-07-18] Pattern-Key: `nsis-registry-fallback-nested`
    现象: NSIS 静默安装后 Fluxing 不出现在语言切换器，QuickPanel 不显示。根因：安全网注册表写入（KnownClasses + HKCU\0x00000804）被嵌套在 `${If} regsvr32 weasel.dll failed}` 块内，成功路径上一个写入都没执行。
    原因: 「safeguard nested inside failure block」 — spec 066 v0.18.41.0 ship (commit ef6eb024) 误把 success-path 也需要的写入挂在失败分支里。
    修复: 把 KnownClasses / HKCU\0x00000804 写入移出 `${If failed}` 块；同时 `reg query` 后置验证强制；`uninstall.nsi` 镜像应用同 fix；按 `.claude/rules/nsis-registry-writes.md` (L66) 守门。
    防重犯: NSIS Edit 完成后，PR diff 自查 «每个 `WriteRegStr` / `WriteRegDWORD` / `WriteRegBin` 是否落在 success-path 外» + CI 层 PreToolUse hook 拦截 `If regsvr32-failed` 内写注册表。
    Recurrence-Count: 1
    来源: `.specify/memory/lessons-learned.md` L66 + `.claude/rules/nsis-registry-writes.md`

- [2026-07-18] Pattern-Key: `verification-skip-on-precommit`
    现象: 7 月 17 日装机 v0.19.0.32 后 user 反馈 3 个失败 — QuickPanel 按钮 2 路由错、按钮 3 图标撞 + 空白 UI、Alt+. 不能调出常用短语。sandbox e2e 全 PASS 但 ship 后 user 装机手测失败。
    原因: 单证据放行 — sandbox e2e 模拟不真启动 WeaselServer + `SendMessage WM_HOTKEY` 触发，没 verify callback 真调到 `PhrasesDialog::Show()`。
    修复: 自检改成「双证据」(sandbox PASS + 真 user flow 步骤)，写入 commit message 必含一段 user-flow 证据。
    防重犯: `verification-before-completion` skill 已在已插入第一条已知陷阱，git-workflow / tdd skill commit-checklist 必勾此项。
    Recurrence-Count: 1
    任务引用: `task.md` Phase A.5 (v0.19.0.33 bug fix) + LE 元任务 Phase LE-2.1

- [2026-07-21] Pattern-Key: `listview-escape-wndmock-false-positive`
    现象: v0.19.0.39 Test 24 (`SendMessageW(hwnd, WM_KEYDOWN, VK_ESCAPE, 0)`) sandbox GREEN 但装机仍 fail — Esc 键不退出 Phrase UI dialog。装机实测反馈 (commit c6d8b35 ship 后)。
    原因: Test 24 直接 dispatch WM_KEYDOWN 到 dialog WndProc，绕过 ListView 焦点路径。真 keyboard event 路由：ListView focus → 按 Esc → ListView 自身不处理 → 转 `LVN_KEYDOWN` 给 parent (PhrasesDialog WndProc 通过 WM_NOTIFY) → OnNotify `case LVN_KEYDOWN` 处理 (v0.19.0.39 OnNotify 缺此 case → switch fallthrough → return 0 → Esc 无响应)。
    修复: v0.19.0.40 OnNotify 加 `case LVN_KEYDOWN` 处理 `wVKey == VK_ESCAPE` → Hide()；Test 25 模拟完整路径 (ListView focus + SendMessage WM_NOTIFY + LVN_KEYDOWN + VK_ESCAPE)。
    防重犯: sandbox test 用 SendMessageW 涉及 common control (ListView / TreeView / Edit) 时，**必须** 选对 message：dialog 直接 dispatch 走 WM_KEYDOWN，child control dispatch 走 WM_NOTIFY (LVN_KEYDOWN / NM_KEYDOWN 等)。test 必须模拟完整 focus chain，否则 false-positive。写入 `verification-before-completion` skill 与 `tdd` skill 的「已知陷阱」章节。
    Recurrence-Count: 1
    任务引用: `task-phase-f-bug3-v0.19.0.40.md` + `handoff-phase-f-bug3-vk_escape-2026-07-20.md` + lessons-learned.md L101 candidate

---

## Promoted（已晋升到 lessons-learned.md）

<!--
晋升后保留一行索引，避免下次重复触发：
- [Pattern-Key] → lessons-learned.md L##
-->

（暂无）