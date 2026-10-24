# LEARNINGS — 用户纠正 + 最佳实践临时入站口

> Loop Engineering L3 正向经验沉淀。详见 `CLAUDE.md` §6.4。
> 跨任务适用时晋升到 `lessons-learned.md` L## 或 `project-knowledge.md`。

---

## Active

<!--
新条目 append 在此区域。条目模板见 README.md。

晋升触发：
- category: correction 被 user 反复提醒 → 晋升 lessons-learned.md L##
- category: best_practice 跨 ≥ 3 个任务复用 → 晋升 lessons-learned.md L##
- 通用项目知识 → 晋升 project-knowledge.md
-->

- [2026-07-18] category: best_practice
    场景: 任何 v0.18.4x.x 或 v0.19.0.x installer 的本地 build + NSIS + installer 任务。
    做法: Claude 全程负责 — 包括 `xbuild.bat` / `build.bat` 全跑、`output/install.nsi` NSIS bytecode generation、`release/fluxing-*-installer.exe` 输出验证。user 仅做装机手测一项。
    效果: 30-90 秒迭代 + 30-60 分钟 end-to-end 都由 Claude run；user 装机手测 → 「报告 OK」/「还需调整」二选一。
    验证: user-feedback 中 「v0.19.0.32 装机发现 3 bug」 → Phase A 修复 → Phase B 装机手测。预计节约 user 沟通成本 70%。
    任务引用: `feedback_build_installer_task.md` (user) + LE 元任务 Phase LE-2.2

- [2026-07-18] category: best_practice
    场景: D2D 透明窗口（候选框、QuickPanel 等）落地。
    做法: STRAIGHT alpha（不是 PREMULTIPLIED）+ DPI 缩放按 `MulDiv(d, dpi, 96)` + `SetLayeredWindowAttributes(hwnd, RGB_KEY, alpha, LWA_COLORKEY)` 三件套；视觉验证必走「真装 + 截图」二步，不靠 sandbox。
    效果: 候选框阴影、QuickPanel Mica 背景不糊。
    验证: `feedback_visual_direction_decisions.md` (双 accent: brand orange + systemBlue) + `d2d_translucent_window.md` 经验沉淀。
    任务引用: spec 040 + LE 元任务 Phase LE-2.2

---

## Promoted（已晋升）

（暂无）