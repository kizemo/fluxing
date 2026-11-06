# 接力 prompt · v1.2.1 发布与导入验收（2026-11-06）

> 用法：新会话首条消息直接发下面「一句话 prompt」即可；方括号内为占位，按需替换。

## 一句话 prompt

```
先完整阅读 F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-06-fluxing-121-release-and-sogou-import.md，
再按顺序读 F:\soft\00selfmade\rime_claude\CLAUDE.md、F:\soft\00selfmade\rime_claude\AGENTS.md、
F:\soft\00selfmade\rime_claude\CONTEXT.md、F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md、
F:\soft\00selfmade\rime_claude\docs\adr\0037-sogou-user-dictionary-import.md，
然后自行掌握背景、目标、进度与下阶段计划，接着按 handoff §八 的建议顺序继续推进火流猩输入法的工作。
```

## 可选追加（只在需要时加）

- 要收候选排序反馈：`本轮先做 handoff §七 Blocker 1：调 kImportCommits 之前，先确认用户真实打字体感。`
- 要处理「词搬不过来」：`按 handoff §八-2 处理，先做 L194 三问，不许直接改解析器。`
- 要清安装残留：`跑 F:\soft\00selfmade\rime_claude\tmp\cleanup-install-leftovers.cmd（需管理员）。`

## 交接边界

- 本棒 HEAD `0b575ac`（已推 `kizemo/Fluxing`，ahead 0）；`git status -sb` 仅 ` m librime` 子模块假脏。
- 本机已装 v1.2.1.0 并已重启 `WeaselServer`；`rime_ice.userdb` 内已有 94,241 条搜狗自造词。
- Release v1.2.1 已发布（非 draft / 非 prerelease）。
- **不要**重做安装、不要重复发 Release、不要以为 `env.bat` / `weasel.props` 该被提交（它们是 gitignore 的未跟踪文件）。