# Handoff — K7 拆分 push v0.19.0.59（2026-07-24）

> **承接 HEAD**: `1c0d711` (chore(release): v0.19.0.59 installer)
> **承接 handoff**: `handoff-after-a5-sftp-sync-2026-07-23.md` (A5 完成)
> **状态**: Commit + Ship 全部本地完成；SFTP atomic upload 到 `www.aiec.fun/pinyin/` 已成功；**仅 GitHub push 失败待 K7 拆分重试**。
> **会话时长**: 跨 2026-07-23 23:10 → 2026-07-24 00:35，约 1.5h
> **user 选择**: 选项 (c) 拆分 push — 先 push tag，后 push branch

---

## 1. 已完成的工作（不在 handoff 内再展开）

| 项 | 状态 | 证据 |
|---|---|---|
| A5 commit | ✓ | `eccc201a` feat(scripts): sftp_sync generic release tool + 56 tests |
| v0.19.0.59 build | ✓ | `build_v059.ps1` 双步：`_buildflow.cmd` Exit 0 + `_nsis_only.cmd` Exit 0 |
| v0.19.0.59 installer | ✓ | `release/fluxing-0.19.0.59-installer.exe` 43,379,147 bytes, md5 `69FA60CD5A30763C970AD54867ED10D7`, sha256 `30DC6AA3ABCEA5192A2C6C420F1EB1FE73A5768EC27DFEE12965695CFB658CF8` |
| release commit + tag | ✓ | `1c0d711` chore(release): v0.19.0.59 installer + tag `v0.19.0.59` |
| `_check_install_v2.ps1` expected md5s 更新 | ✓ | ps1 内 4 行 expected 指向 v0.19.0.59 (`448DE044...` / `C969AEB3...` / `69FA60CD...` / `2026-07-24 00:04:00`) |
| `sftp_sync.py --version=0.19.0.59` 原子 upload | ✓ | `www.aiec.fun/pinyin/` 上 4 个文件 cross-source verify 全 PASS |
| L107 lessons 沉淀 | ✓ | L107-A / L107-B / L107-C / L107-D 写入 `1c0d711` commit message + task.md |

---

## 2. 唯一待办 — K7 拆分 push

### 2.1 Push 失败回顾

| 尝试 | 时间 | 结果 |
|---|---|---|
| `git push kizemo Fluxing v0.19.0.59` | 2026-07-24 00:11 | ❌ `RPC failed; HTTP 500` + `non-fast-forward` rejected |
| `git fetch kizemo` | 2026-07-24 00:21 | ✓ 同步本地视图：ahead 936, behind 831 |
| `git push --force-with-lease kizemo Fluxing v0.19.0.59` | 2026-07-24 00:26 | ❌ 同 HTTP 500（935 commits 大 push 触发 server 限制） |

**根因**:
- kizemo/Fluxing 远程停在 `7d9f24b` (v0.19.0.9)，远落后本地
- 本地 `1c0d711` 是 v0.19.0.59 release commit
- merge-base = `0a226d1` (v0.18.x era)，差距 935 commits
- 单次 push 935 commits → GitHub server 抛 HTTP 500
- 推测 server-side rate limit 或 chunked transfer 上限

### 2.2 K7 拆分 push 策略（user 已选）

**Step 1: Push tag 优先**（轻量、单 object、验 server 状态）
```bash
cd F:/soft/00selfmade/rime_claude
git push kizemo v0.19.0.59
```
- 成功 → server OK，继续 Step 2
- 失败（HTTP 500 再次）→ 标记 server 临时故障，等几分钟后重试

**Step 2: Push branch 增量**（force-with-lease，1 个新 commit 在 v0.19.0.9 之上）

先看 remote 当前 branch tip：
```bash
git fetch kizemo
git rev-parse kizemo/Fluxing  # 当前 remote tip
```

策略选项：
- **(2a) 直接 force-with-lease 全量**（935 commits）：重试上次失败 — 风险高
- **(2b) 拆分 push 增量**：
  ```bash
  # 把 935 commits 拆成 ~300 commits 一批
  git push --force-with-lease kizemo Fluxing^300:Fluxing  # 第一批
  git push --force-with-lease kizemo Fluxing^150:Fluxing  # 第二批
  git push --force-with-lease kizemo Fluxing:Fluxing      # 最后一批
  ```
  复杂度高，每步要确认 force-with-lease 状态正确
- **(2c) 跳过 GitHub**：ship 路径是 SFTP `www.aiec.fun/pinyin/`，装机端交付不依赖 GitHub。本地 commit + tag 已 git tracked 完好，等下个版本 ship 时再合并 push

**user 选项**: (c) 拆分 push = (2b) 拆分增量。先 tag 后 branch。**Step 1 (tag) 必跑；Step 2 (branch) 试 (2b) 拆分增量；若仍 HTTP 500 → fall back to (2c) 跳过 GitHub**。

### 2.3 关键文件状态

```
F:\soft\00selfmade\rime_claude\
├── HEAD: 1c0d711 chore(release): v0.19.0.59 installer
├── tag: v0.19.0.59 (lightweight, points at 1c0d711)
├── release/fluxing-0.19.0.59-installer.exe (tracked, 41.37 MB)
├── output/archives/fluxing-0.19.0.59-installer.exe (build artifact)
├── E:/办公文件/L1网站/pinyin/fluxing-0.19.0.59-installer.exe (SFTP 已上传)
├── remote kizemo/Fluxing: 7d9f24b (v0.19.0.9) — 落后 935 commits
└── remote kizemo tags: 缺 v0.19.0.59
```

**git status -sb 期望**: `## Fluxing...kizemo/Fluxing [ahead 936, behind 831]`

---

## 3. Memory 已更新

- `~/.claude/projects/.../memory/feedback_pinyin_release_workflow.md` 第 9 步：sftp_sync 已 ship（无需修改）
- `task.md` 第 1 行表 K7 拆分 push 已挂上

---

## 4. 已知风险 / 不动项

- **不动**：`librime` submodule dirty（prior session 残留）
- **不动**：60+ untracked files（prior session 残留）
- **风险**：HTTP 500 可能是 GitHub 临时故障；拆分 push (2b) 仍可能 fail
- **fallback**：若 push 持续 fail，ship v0.19.0.59 实际已通过 SFTP 完成；装机端不依赖 GitHub 同步

---

## 5. K7 执行清单（新会话必跑）

1. 读 `~/.claude/projects/F--soft-00selfmade-rime-claude/memory/MEMORY.md`
2. 读本 handoff
3. 读 `_prompt_k7_split_push_2026-07-24.md`（包含完整 step-by-step）
4. **必跑 Step 1**: `git push kizemo v0.19.0.59`（tag first）
5. 若 tag OK → 跑 Step 2a / 2b（拆分 push branch）
6. 若 branch push 仍 HTTP 500 → 走 fallback (2c) 跳过，记录决策到 task.md
7. **不要**重试上次失败的全量 force-with-lease

---

— END OF HANDOFF —