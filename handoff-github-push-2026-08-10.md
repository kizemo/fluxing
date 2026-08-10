# Handoff — Fluxing v0.21.0.0 GitHub push (divergence cleared) (2026-08-10)

> **状态**:v0.21.0.0 milestone 已从 local Fluxing 分支完整 push 到 kizemo/Fluxing GitHub remote。**divergence 从 948 ahead / 831 behind 变为 0/0**。tag v0.21.0.0 + backup tag 全上 remote。
>
> 主 ship handoff 仍以 [`handoff-v0.21.0.0-release-2026-08-10.md`](handoff-v0.21.0.0-release-2026-08-10.md) 为准(本地 + SFTP 那一段)。**本文件**专讲本次 GitHub push session 学到的 + 待办交接。

---

## 0. 大白话总结

| 项 | 值 |
|---|---|
| **push 前 divergence** | local Fluxing 948 ahead / kizemo/Fluxing 831 behind(merge-base = 2018-02-05, 8 年 fork) |
| **push 策略** | 5 批 × 200 commits + 1 批 × 148 commits(SSH chunked) |
| **session 末 divergence** | **0/0** ✓(36 commits 全 push,local HEAD = remote HEAD = `792cb47e`) |
| **post-session 状态** | L108 commit `65557d8` 在 session 末 (22:03) commit 到 local,**未 push**,divergence 暂时回到 ahead 1。详见 §8 补遗 |
| **remote Fluxing HEAD (session 末)** | `792cb47e78bdd312e25df15066fe0018b70281bd`(同 local) |
| **remote tag v0.21.0.0** | annotated `f1a0776cf6f03cb4622c515d094bb7a2ea06147f` → commit `54909ae37c56f259f6760055f5ffe1254992e844` |
| **remote backup tag** | `backup/kizemo-Fluxing-pre-push-2026-08-10` = `7d9f24b43bacdd63b5dff61410f4e12caa8f43f5` |
| **新加 remote alias** | `kizemo-ssh` (git@github.com:kizemo/fluxing.git)— `kizemo` (HTTPS) 保留 |

---

## 1. 关键发现 — GitHub 2.00 GiB pack hard limit

详见 `.specify/memory/lessons-learned.md` **L108**(2026-08-10 新增)。

**TL;DR**:
- 任何含 ≥30 个 release/fluxing-*-installer.exe (40MB+) 的 push,必先 chunked 到 < 200 commits / 批
- Windows + git 2.54 + 无 credential helper → `--force-with-lease` hang;改纯 `--force` 或 SSH
- SSH transport > HTTPS for big pushes(HTTPS 500 无 message;SSH 出 `pack exceeds maximum allowed size (2.00 GiB)`)
- Pre-push backup tag 必做且 push 上 remote — force-push 出错时旧 SHA 永久可达

---

## 2. 文件改动清单(本 session)

| File | 操作 | 是否 commit |
|---|---|---|
| `.specify/memory/lessons-learned.md` | Edit — 新增 L108 (~70 行) | ✅ commit `65557d8` (2026-08-10 22:03);详见 §6 补遗,push via SSH |
| `release/fluxing-0.21.0.0-installer.exe` | (existing,无变化) | ✅ commit `54909ae` |
| remote refs(`kizemo-ssh/Fluxing` + `kizemo/Fluxing`) | force-update `7d9f24b → 792cb47e` | n/a(remote) |
| remote refs 后续 update | `792cb47 → 65557d8`(L108 push) | n/a(remote,见 §6 补遗) |
| tag `v0.21.0.0` | push 上 remote | n/a |
| tag `backup/kizemo-Fluxing-pre-push-2026-08-10` | 创建 + push 上 remote | n/a |
| `_push_batches.sh` / `_push_final.sh` / `_push_30.sh` | 临时脚本 | ✅ 已删 |

---

## 3. 接下来要做的事(per 主 handoff §5)

主 handoff [`handoff-v0.21.0.0-release-2026-08-10.md`](handoff-v0.21.0.0-release-2026-08-10.md) §6.4 + §3 列了 3 类待办:

| 优先级 | Task | 谁做 | 备注 |
|---|---|---|---|
| **P1** | v0.21.0.1 ship: `output/start_service.bat` 内容反 fix(trivial)+ `output/data/user-custom/*.custom.yaml` key_binder `send: "《"` 等中文 parse error fix(需研究 librime `send_text` API vs `punctuator/half_shape` patch) | Claude | 修完 rebuild installer + SFTP + cross-source verify |
| **P1** | 并发压测:装 v0.21.0.0 后同时开 Claude Code + Explorer + dopus + 微信 + WeaselServer 重启,来回切 5+ min,看 `C:\fluxing-dumps\` 是否新增 0xc0000374 dump | 你 (manual) | spec 076 race fix 真不触发才算 PASS |
| **P2** | 跑 `rime-verify.ps1 -InstallerPath release\fluxing-0.21.0.0-installer.exe` | Claude | 注意 Win11 24H2 Sandbox LogonCommand flake 风险(memory `feedback_v074_stage_install_lessons`) |
| **记录** | (可选)L108 已写,无需额外 | — | — |

**特别提醒给 v0.21.0.1 fix session**:
- start_service.bat 是 1 行修改,无风险
- key_binder send 中文 fix 需先研究 librime API(参考 lessons-learned L03/L04 + L18-L21 已有的 key_binder 教训)
- 任何 silent-failure-prone 代码改动 → 加 IfErrors 检测 + retry + 备用路径(per L108 + PhaseM 5 incident 总结)
- rebuild 后做 md5 dual-verify(extract archive md5 == user 装机路径 md5)— ship-gate

---

## 4. 环境 / 分支 / 基线状态

| 项 | 值 |
|---|---|
| 工作目录 | `F:\soft\00selfmade\rime_claude` |
| Git branch | `Fluxing` (HEAD = `792cb47e78...`) |
| Git status | clean(除 librime submodule noise + 一堆 handoff/prompt untracked files) |
| Divergence | **0/0**(vs kizemo + kizemo-ssh) |
| Remotes | `kizemo` (HTTPS, 默认) + `kizemo-ssh` (SSH, 大 push 用) |
| 装机 base | `D:\Program Files\Fluxing\weasel\`(v0.21.0.0 verified) |
| 已 ship versions on server | v0.21.0.0 (current) |
| 已 ship versions on GitHub | v0.21.0.0 + 之前 31 个 tag |

---

## 5. 避坑提示

1. **不要** 直接 `git push kizemo Fluxing:Fluxing`(走 HTTPS)— 会被 chunked limit 卡。**用** `git push --force kizemo-ssh <sha>:Fluxing` 走 SSH。
2. **不要** 改 `output/install.nsi`(F3/F4 reliability fix 已 verified PASS,spec 076 真因已解决,不能动)。
3. **不要** 重新跑 `_nsis_v065.cmd` rebuild v0.21.0.0 installer(source 不变,只会生成相同 md5)。
4. **不要** 删 `release/fluxing-0.20.0.{0,1,3,4}-installer.exe`(release history,per `feedback_release_output_rule.md`)。
5. **不要** 在 `D:\Program Files\Fluxing\weasel\` 跑 `uninstall.exe`(会破坏装机 state)。
6. **不要** echo `sftp.json` password 到 log(脚本已隐藏,但 transform/analyze 时注意)。
7. **v0.21.0.1 改动** 走 standard NSIS rebuild + sandbox + SFTP 流程,不要尝试 chunked-push 这次 push 临时脚本的 pattern(那个是一次性的)。
8. **bash 工具 output capture** 对长 push 不可靠 — 以 `git ls-remote` 为 ground truth,不要靠 stdout/stderr 判断。

---

## 6. 必读顺序(新会话第一件事)

1. **本文档** (`handoff-github-push-2026-08-10.md`) — push session 教训 + 后续任务
2. **主 handoff** [`handoff-v0.21.0.0-release-2026-08-10.md`](handoff-v0.21.0.0-release-2026-08-10.md) — v0.21.0.0 ship + SFTP + 装机后验证全 record
3. **lessons-learned L108** — GitHub 2GB pack limit + chunked push 策略
4. **lessons-learned PhaseM v0.21.0.0**(line 9999-10085)— 5 incident 完整复盘
5. `output/install.nsi` line 500-535 (F3) + 777-794 (F4)— F3/F4 reliability fix 具体代码

---

## 7. prompt-next 文件

详见同目录 `prompt-after-push-next-session.md`(≤30 行)。新会话第一句话直接引用。

---

## 8. 补遗 — L108 push 收尾 (2026-08-10 后续 session)

本 session 末态:`L108 commit 65557d8` 已 commit 到 local 但未 push,divergence 临时回到 ahead 1。后续 session 按 §5 提示用 `kizemo-ssh` 走 SSH push:

| Action | 命令 |
|---|---|
| Push | `git push kizemo-ssh 65557d8c14c252ff03eecb678c09933f5f4182f5:Fluxing` |
| stdout 显示 | `792cb47..65557d8  65557d8c14c252ff03eecb678c09933f5f4182f5 -> Fluxing` |
| Ground truth (ls-remote) | `kizemo-ssh` + `kizemo` 两个 remote 的 `refs/heads/Fluxing` 都 = `65557d8c14c252ff03eecb678c09933f5f4182f5` |
| Refresh tracking ref | `git update-ref refs/remotes/kizemo/Fluxing 65557d8c...`(fetch 后 status -sb 仍 cached 显示 ahead 1,需手动 update-ref 或 `git fetch --prune`) |
| **最终 divergence** | **0/0** ✓ (local HEAD = `65557d8` = remote HEAD on both remotes) |

### 8.1 为什么 L108 没在本 session push

push session 是历史大 push (948 commits, 5 批 × 200),L108 在最后追加是 hot-priority 文档 commit (lessons-learned),不属 push session 范围。push session 的 producer/consumer 边界应保持清晰:大 push ≠ 单文档 push。

### 8.2 L108 的真相

- 22:03:39 +0800 commit `65557d8`,author `duanyi <duanyi@aiec.fun>`,body 详述 2.00 GiB pack limit 教训
- 共 7 条 lessons,均基于本 session 真实证据 + 修复路径
- Co-authored-by Claude Opus 4.8

### 8.3 本次补遗 commit (后续 session 已做)

1. 修改本文档 §0 表 (加 "session 末" + "post-session 状态" + 区分 HEAD) + §2 表 (L108 状态 ✅ commit + 后续 push)
2. 添加本 §8 补遗段
3. Commit message: `docs(handoff): L108 push 收尾 + stale 修正`
4. 走同样 SSH push 路径(L108 commit 已在 remote,所以**仅 push 本补遗 commit**即可)

### 8.4 不要做的事

- **不要**重 push `65557d8`(已在 remote,会触发 non-fast-forward 警告)— 用 `git fetch --all --prune` 刷新 tracking ref 即可
- **不要**修改 §1-§5 的 historic content(那是 push session 当时的 ground truth)
- **不要**修改 §0/§2 表中描述"session 末"的字段值 — 只加新字段

---

**作者**:Claude(本 session) · **日期**:2026-08-10 · **session 状态 (原文)**:P0 push 完成 + L108 已写,待你 review + commit + 开新 session 做 P1/P2
**补遗作者**:Claude(后续 session) · **日期**:2026-08-10 · **当前 divergence 状态**:**0/0** ✓ (L108 + 补遗 commit 都已 push 到 kizemo-ssh)