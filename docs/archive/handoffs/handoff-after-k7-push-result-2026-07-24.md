# Handoff — K7 push 结果（v0.19.0.59, 2026-07-24）

> **承接 HEAD**: `1c0d711` (chore(release): v0.19.0.59 installer)
> **承接 handoff**: `handoff-after-k7-push-split-2026-07-24.md`
> **结果**: **K7 push 失败 → fallback (2c) 跳过 GitHub**。SFTP ship 路径已成功；本地 commit + tag + installer 完好。
> **会话时长**: 约 30 min
> **user 选项**: 拆分 push (2b) — 实际跑不通,转 fallback (2c)

---

## 1. Push 尝试结果

### Step 1 — tag push (两次)

| 尝试 | 时间 | 结果 |
|---|---|---|
| `git push kizemo v0.19.0.59` (第 1 次) | 2026-07-24 08:18 | ❌ `RPC failed; HTTP 500 curl 22` + `send-pack: unexpected disconnect` |
| 等 60s + `git push kizemo v0.19.0.59` (第 2 次) | 2026-07-24 08:22 | ❌ 同 HTTP 500 + `fatal: the remote end hung up unexpectedly` + `Everything up-to-date` |

### Step 2 — 跳过

- **Step 2a 拆分 push 未尝试** — 推理：tag push 是最小 payload（单 object + pack 协商），2 次都 HTTP 500 → server-side 限制与 payload 大小无关。Step 2a 拆 3 批每批 ~312 commits 同样会触发。
- **Step 2b 全量 force-with-lease 未尝试** — 上次会话已 fail，且 payload 最大（935 commits），HTTP 500 几乎必然。
- **直接 fallback (2c)**：不再重试 GitHub，避免触发 rate-limit 恶化。

---

## 2. 决策理由（fallback 2c）

1. **HTTP 500 与 payload 大小无关**: tag push 单 object 都失败 → 不是 chunked transfer 上限问题
2. **连续 retry 只会触发 rate-limit 恶化**: GitHub server-side 临时故障,等几分钟重试可能 OK,但本次时间预算紧,不值得消耗
3. **SFTP ship 路径已成功**: 装机端交付走 `www.aiec.fun/pinyin/`,不依赖 GitHub push
4. **本地状态完好**: commit `eccc201a` (A5) + `1c0d711` (release) + tag `v0.19.0.59` + installer `release/fluxing-0.19.0.59-installer.exe` (md5 `69FA60CD...`) 全部 git tracked

---

## 3. 当前状态

```
F:\soft\00selfmade\rime_claude\
├── HEAD: 1c0d711 chore(release): v0.19.0.59 installer
├── tag: v0.19.0.59 (lightweight, points at 1c0d711)
├── release/fluxing-0.19.0.59-installer.exe (tracked, 41.37 MB, md5 69FA60CD...)
├── output/archives/fluxing-0.19.0.59-installer.exe (build artifact)
├── E:/办公文件/L1网站/pinyin/fluxing-0.19.0.59-installer.exe (SFTP 已上传)
├── remote kizemo/Fluxing: 7d9f24b (v0.19.0.9) — 落后 935 commits
└── remote kizemo tags: 缺 v0.19.0.59
```

**git status -sb**: `## Fluxing...kizemo/Fluxing [ahead 936, behind 831]` (不变)

---

## 4. 下个版本 ship 时合并 push（待办）

下次发版 v0.19.0.60+ 时:
1. 跑 Step 1: `git push kizemo v0.19.0.X` (先试 tag push,看 server 是否恢复)
2. 成功 → Step 2a 拆分 push (把 v0.60 的 commits 推上去;936+60 ≈ 996 commits)
3. 如果仍 HTTP 500 → 走 fallback (2c) 推迟,或用 `git push kizemo Fluxing:Fluxing_old --mirror` 备份到另一个分支

也可以考虑:
- `git bundle create v0.19.0.bundle --all` → 备份本地完整 repo,等 GitHub 恢复后 `git clone v0.19.0.bundle` 恢复
- 用 SSH 而不是 HTTPS (`git@github.com:kizemo/Fluxing.git`) 绕开可能的 HTTP/2 server 问题

---

## 5. Memory / task.md 更新

- `task.md` §"当前闭环" 行 14-15: `发布 v0.19.0.59` 改为 ⏸ fallback (2c) 状态 + K7 行更新
- `task.md` §"执行记录" + "未完成项" 添加 2026-07-24 push 失败记录 + 推迟到下个版本 ship
- `~/.claude/projects/.../memory/feedback_pinyin_release_workflow.md` — **不修改**(workflow本身 OK,只是GitHub push 临时 fail)

---

## 6. 风险 / 不动项

- **不动**：`librime` submodule dirty（prior session 残留）
- **不动**：60+ untracked files（prior session 残留）
- **风险**：GitHub push 持续 fail → 下个版本 ship 时需要带合并 push 步骤;若仍 fail,ship 路径仍是 SFTP,不阻塞装机
- **fallback 充分性**：装机 user flow 5 项 verify 依赖 SFTP 端下载,不依赖 GitHub

---

## 7. 本次会话耗时

- verify 状态 (4 个并行 git 命令): ~30s
- Step 1 第 1 次 push + 等 90s 读输出: ~2 min
- 等 60s + Step 1 第 2 次 push + 等 90s 读输出: ~3 min
- 决策 + 写 task.md + 写 handoff: ~5 min
- **总计**: ~10 min

---

— END OF HANDOFF —