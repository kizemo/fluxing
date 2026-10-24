# K7 新会话 prompt — 拆分 push v0.19.0.59（2026-07-24）

承接上一个会话任务：Commit + Ship v0.19.0.59 完成（commit `eccc201a` + `1c0d711` + tag `v0.19.0.59` + SFTP `www.aiec.fun/pinyin/` 原子 upload 成功），见

`F:\soft\00selfmade\rime_claude\handoff-after-k7-push-split-2026-07-24.md`

我已选择下一步：拆分 push（先 push tag，后 push branch），避免单次 935 commits 大 push 触发 GitHub HTTP 500。

请按 handoff §2.2 / §5 推进。

---

## 当前状态（必须先 verify 再动作）

```bash
cd F:/soft/00selfmade/rime_claude
git status -sb              # 期望: ahead 936, behind 831
git log --oneline -3        # 期望: 1c0d711 chore(release)... + eccc201a + b0ccfe9
git tag -l v0.19.0.59       # 期望: v0.19.0.59
git rev-parse kizemo/Fluxing # 期望: 7d9f24b (v0.19.0.9) — 远落后本地
```

## Step 1 — Push tag 优先（必跑）

```bash
git push kizemo v0.19.0.59
```

- **成功** → server OK，进入 Step 2
- **失败（HTTP 500 再次）** → 等 5–10 分钟重试 1 次；再 fail → 标记 server 临时故障，转 fallback (2c) 跳过 GitHub（仍保留本地 commits + tag）

## Step 2 — Push branch（拆分增量尝试）

**先看差距**：
```bash
git fetch kizemo
git rev-list --left-right --count kizemo/Fluxing...HEAD
# 期望输出: 831 (remote-only) \t 936 (local-only)
```

### Step 2a — 拆分 push（首选）

如果差距仍是 ~935 commits，拆 3 批（每批 ~300 commits）：

```bash
# 计算分割点
TOTAL=$(git rev-list --count kizemo/Fluxing..HEAD)        # 936
B1=$((TOTAL / 3))                                         # 312
B2=$((TOTAL * 2 / 3))                                     # 624
PIVOT1=$(git rev-parse kizemo/Fluxing~$B1)
PIVOT2=$(git rev-parse kizemo/Fluxing~$B2)

# Push 第 1 批 (Pivot1 之前的所有新 commits)
git push --force-with-lease=kizemo/Fluxing:7d9f24b kizemo "$PIVOT1":Fluxing
if errorlevel 1 exit /b 1

# Push 第 2 批
git push --force-with-lease=kizemo/Fluxing:$PIVOT1 kizemo "$PIVOT2":Fluxing
if errorlevel 1 exit /b 1

# Push 第 3 批 (含 tag + release commit)
git push --force-with-lease=kizemo/Fluxing:$PIVOT2 kizemo Fluxing v0.19.0.59
```

**注意**：`kizemo/Fluxing:7d9f24b` 是 `--force-with-lease` 期望值（即远程当前 tip）；push 后远程 tip 会前进，下次 push 要更新 lease 期望。

### Step 2b — 直接 force-with-lease 全量（fallback）

如果拆分 push 步骤也遇 HTTP 500（说明 server 问题非 payload），可尝试单次大 push（上次已 fail，重试不一定 OK）：

```bash
git push --force-with-lease=kizemo/Fluxing:7d9f24b kizemo Fluxing v0.19.0.59
```

**只在 Step 2a 全部失败时用**，且**最多 2 次**避免 rate-limit。

### Step 2c — 跳过 GitHub（最终 fallback）

如果 Step 2a / 2b 都 HTTP 500：

1. **不重试** — server-side 问题，重试会触发 rate-limit
2. 写记录到 `task.md`：「K7 push 失败，已走 fallback (2c)；ship 路径 SFTP 已成功；下个版本 ship 时合并 push」
3. 不动本地 commits / tag / installer（全部 git tracked 完好）

## 验证 push 成功

```bash
git fetch kizemo
git rev-list --left-right --count kizemo/Fluxing...HEAD
# 期望: 0 (remote-only) \t 936 (local-only) — 全 push 上去了
# 或: 0 \t 0 — 若 push 包含 merge

git ls-remote kizemo refs/tags/v0.19.0.59
# 期望: v0.19.0.59 \t 1c0d711...

# 装机器校验
git ls-remote kizemo HEAD
# 期望指向 1c0d711 (release commit)
```

## 写 handoff（push 完成后）

写 `handoff-after-k7-push-result-2026-07-24.md`：
- Step 1 / Step 2 实际结果
- 任何中途 HTTP 500 / rate-limit
- 装机器校验（`git ls-remote kizemo HEAD` / `git ls-remote kizemo refs/tags/v0.19.0.59`）
- 若失败：fallback 决策记录

## 不要做

- ❌ 不要 `git push --force kizemo Fluxing`（无 lease 保护）
- ❌ 不要在 push 失败时改 commit / rebase / amend（本地状态完好，远程同步只是时间问题）
- ❌ 不要碰 `librime` submodule / 60+ untracked files（不在 K7 范围）
- ❌ 不要触发 `_check_install_v2.ps1` 或装机器验证（user 端操作）

## 风险

- **HTTP 500**：server-side 临时故障或 chunked transfer 限制。Step 2a 拆分理论上降低单次 payload，但 GitHub server 端可能有更深的限制
- **rate-limit**：连续重试会触发；最多 3 次失败就走 fallback
- **force-with-lease 误用**：lease 期望值必须等于 push 时 kizemo/Fluxing 的实际 tip；push 后 tip 前进，下次 push 要更新 lease

## 时间预算

- Step 1 (tag push)：1 分钟
- Step 2a (3 批 push)：3–5 分钟（每批 ~30s + 间隔）
- 验证：1 分钟
- 写 handoff：5 分钟
- **总：~10 分钟**

到点主动 `/exit` 或新开会话（按 `claude_desktop_session_health.md` < 2h 活跃时间预算）。