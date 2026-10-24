# Handoff — v0.19.0.58 ship 之后（K6 收尾后的方向选择，2026-07-23）

> **承接 HEAD**: `b0ccfe9` (docs(spec): 051 out-of-process PhrasesDialog 首次入 git + ship 勾选同步)
> **状态**: v0.19.0.58 已 ship + SFTP 同步 + 装机端 Bug 3+4 验证 PASS
> **最近一次活动**: housekeeping A1（051 spec 文档 housekeep，30 min 完成）

---

## 1. 已完成的事（无需重做）

- v0.19.0.58 installer 已 ship + 4/4 md5 verify 装机端 PASS
- 051 spec 文档（spec.md + plan.md + tasks.md）首次入 git
- Phase K1-K5 P1 全部 14 个 task (T005-T018) + 5 个 verification (V001-V005) 勾选反映真实 ship 状态
- 仍 un-shipped（7 项保持 [ ]）：T101-T103 P2 + T201-T203 P3 + V006

---

## 2. 四个可选方向（任选一个或全跳过）

### A5：发版脚本通用化（30 min，最快见效）
- **解决**：现在每次发版要复制 `_sftp_v058.py` 改 4 文件名 + md5，重复劳动
- **做法**：1 个通用 `sftp_sync.py` + 1 个 JSON config，命令 `python sftp_sync.py --version=0.19.0.59` 自动完成
- **新文件**：`sftp_sync.py` + `sftp_config.json`；旧 `_sftp_v058.py` 可保留备用或删

### A6：给 AI 助手批量补"避坑书签"（1h+，元任务）
- **解决**：项目踩过同一坑 4+ 次（e.g. 改 WeaselServer 不重编译装机仍是旧版本、L107 x64 stale binary、L100 假阳 sandbox test）
- **做法**：找出 20+ 个 SKILL.md (`.claude/skills/`)，每个加"已知陷阱"占位段，文档化踩坑模式
- **新文件**：仅修改 `.claude/skills/*.md` (20+ files)

### B1：修用户词典界面（4-6h，业务推进）
- **解决**：装机端打开"用户词典"看到空框（chrome 画完了但 list 没内容 + 半透明 bug 让按钮不可见）
- **做法**：(a) 让 yaml 加载能让 list 填内容；(b) 修"WS_EX_LAYERED 透明"模式（跟 ShortcutSettings 改用直接 GDI paint 保持一致）
- **目标**：ship v0.19.0.59 = UserDict 真能用的版本
- **改动文件**：`WeaselServer/UserDictionary.cpp` + `WeaselServer/UserDictionary.h` + 测试

### D：等用户反馈
- 不动，等装机端 v0.19.0.58 验证 + 任何新 bug 报告
- 适合 user 想要休息 / 装机端验证未完成

---

## 3. 关键文件索引

| 路径 | 作用 |
|---|---|
| `b0ccfe9` | HEAD commit（051 首次入 git + ship 勾选） |
| `642cbb0` | 上一 commit（v0.19.0.58 SFTP sync） |
| `.specify/specs/051-out-of-process-phrases-dialog/tasks.md` | 刚 housekeep 完的勾选状态 |
| `release/fluxing-0.19.0.58-installer.exe` | 最近 ship 的装机包 |
| `www.aiec.fun/pinyin/` | aiec staging remote 4/4 已同步 |
| `_sftp_v058.py` | K6 一次性 SFTP 脚本（A5 可 refactor 此文件） |
| `.claude/skills/` | SKILL.md 目录（A6 目标） |
| `WeaselServer/UserDictionary.cpp` | 用户词典 UI source（B1 目标） |
| `_check_install_v2.ps1` | 装机端验证脚本（user 端跑） |

---

## 4. 已知风险 / 不动项

- **不动**: `librime` submodule dirty (prior session 残留)、60+ untracked files (prior session 残留)
- **风险**: 装机端 v0.19.0.58 完整 5+1 用户流验证仍 pending（V006 保持 [ ]）
- **风险**: 选择 A5/A6/B1 时若涉及 Rime 业务改动，必须先 `git status -sb` + 重新读 CLAUDE.md 强约束

---

## 5. 新会话开局 prompt 模板

```
承接上一个会话任务 v0.19.0.58 ship 之后的 direction analysis，见
F:\soft\00selfmade\rime_claude\handoff-after-k6-direction-2026-07-23.md

我已选方向：[A5 / A6 / B1 / D]

请按 handoff §2 推进。
```

— END OF HANDOFF —
