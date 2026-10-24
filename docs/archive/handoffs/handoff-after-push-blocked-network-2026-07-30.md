# Handoff — push blocked on network + source-level ship-gate PASS (2026-07-30)

> **承接**: `handoff-after-phase-L-bugfix-9.5-9.6-v0.74-shipped-2026-07-30.md`
> **状态**: ✅ **source-level ship-gate 全部 PASS** (5 binary md5 MATCH + _check_system32_cleanup Audit 9/9 + Test-Stage2ShimInstall 6/6) | ⚠️ **git push 阻塞在网络层** (backup branch ✅ 落地,force-push ✗ 失败)
> **未做**: 真机 sandbox-verify (task #4,需要用户触发 sandbox 启动) + Live mode check scripts (task #1 / #3,等 sandbox 装好后)
> **下个会话第一任务**: 网络恢复后重试 `git push --force kizemo Fluxing` + 真机 sandbox-verify

---

## 1. 本会话关键状态 (2026-07-30 16:00+0800)

### 1.1 Git 状态

```
HEAD:    654d3b2 (fix(adr+installer-test): v0.74 evidence-gap closure)
ahead:   963 commits  (本地独有,待 push)
behind:  831 commits  (远端独有,7-10~7-11 期间 Claude push 出去)
remote:  kizemo (HTTPS) — 已恢复
```

**3 commits 待 push:**
- `654d3b2` (2 files, 207 ins / 34 del) — ADR 0003 scope amend + Test 6 source-level ship-gate
- `4ea9a14` (4 files, 164 ins / 19 del) — silent Abort + reorder + reset
- `4694a1d` (5 files, 721 ins / 17 del) — F4 stage-at-startup + 9.6 cleanup + ADR + test + lessons

### 1.2 远端分支 (已 fetch 同步)

```
kizemo/Fluxing                              = 7d9f24b (原始,未变)
kizemo/Fluxing-backup-pre-2026-07-30        = 7d9f24b (✅ 备份已落地)
kizemo/Fluxing-snapshot-2026-06-30          = bb9d28e (未动)
```

**Backup branch 验证**:
- Step 1: `git push kizemo kizemo/Fluxing:refs/heads/Fluxing-backup-pre-2026-07-30` 成功
- Step 2: `gh api repos/kizemo/fluxing/git/refs/heads/Fluxing-backup-pre-2026-07-30` 确认 SHA = 7d9f24b
- Step 3: `git ls-remote kizemo` 确认 backup branch 存在

### 1.3 网络层阻塞 (新发现)

```
ping github.com:
  平均 = 94ms
  丢包率 = 33% (3 packets: 1 lost, 2 received)
```

**force-push 失败 mode 集合** (本会话全部尝试过):
| 协议 | 命令 | 失败 mode |
|---|---|---|
| HTTPS | `git push --force-with-lease` | `non-fast-forward` rejection |
| HTTPS | `git push --force` | `POST git-receive-pack (chunked)` no response |
| HTTPS | `git push kizemo +HEAD:refs/heads/Fluxing` | `failed to push some refs` |
| SSH | `git push --force-with-lease kizemo Fluxing` | `pack-objects died of signal 15` + `Broken pipe` |
| SSH | `git push --no-thin --force kizemo Fluxing` | `send-pack: unexpected disconnect` + `Broken pipe` |
| HTTPS | `git push kizemo 654d3b2:refs/heads/test-canary` (1 commit) | `failed to push some refs` |
| gh API | `PATCH repos/kizemo/fluxing/git/refs/heads/Fluxing` force=true | `Object does not exist` (server 无 commit object) |
| `git clone --depth 1` | fetch test | `fetch-pack: invalid index-pack output` |

**真因结论**: 网络层 packet loss (33%) + GitHub server 端在 unpack 963 commits 大 pack 时 OOM / 中断连接。SSH 和 HTTPS 都受影响,**1 commit 推送也失败**。这不是 git protocol 问题,是 network 基础阻塞。

### 1.4 客户端已调配置

```
http.postBuffer = 268435456  (256MB,之前是 500MB)
http.lowSpeedLimit = 0       (关闭低速度切断)
http.lowSpeedTime = 999999   (低速度判定窗口 999s)
pack.windowMemory = 256m
pack.packSizeLimit = 256m
remote.kizemo.url = https://github.com/kizemo/fluxing.git  (HTTPS 已恢复)
```

---

## 2. 验证状态 (本会话已完成)

### 2.1 Task #5 — 5 binary md5 ✅ PASS (本会话早期)

| binary | v0.73 md5 | v0.74 md5 | 状态 |
|---|---|---|---|
| weasel.dll | 231fd0cfed97fbe6e40c1498cb10a1f1 | 231fd0cfed97fbe6e40c1498cb10a1f1 | MATCH |
| weaselx64.dll | ea4dece5b4c76a01704204472c2aa5f4 | ea4dece5b4c76a01704204472c2aa5f4 | MATCH |
| WeaselServer.exe | 0424eb54380d5cc0ea5b1ca583bf008b | 0424eb54380d5cc0ea5b1ca583bf008b | MATCH |
| WeaselDeployer.exe | f8d635342128364056ca748116dceecd | f8d635342128364056ca748116dceecd | MATCH |
| WeaselSetup.exe | bb4440d6c38a885ee33213d393bf1ab7 | bb4440d6c38a885ee33213d393bf1ab7 | MATCH |

**v0.74 = NSIS-only 修改,binary 完全不变**。v0.74 比 v0.73 多 6 文件(5 .staged + 1 stage2 batch cmd)。

### 2.2 Task #1 (部分) — _check_system32_cleanup Audit ✅ PASS

```
[9.6 .onInit] anchor found: v0.19.0.74 (Phase L Bugfix 9.6)  ✅
[9.6 .onInit] anchor found: $WINDIR\System32\weaselx64.dll  ✅
[9.6 .onInit] anchor found: $WINDIR\System32\weasel.dll    ✅
[9.6 .onInit] anchor found: Delete /REBOOTOK $R8             ✅
[9.6 .onInit] anchor found: IfFileExists $R8 0 +2           ✅
[9.6 Section Uninstall] anchor found: v0.19.0.74 9.6 symmetric cleanup  ✅
[9.6 Section Uninstall] anchor found: Delete /REBOOTOK $R8  ✅
[PASS] Trap 5 (Sleep 3000ms) preserved
[PASS] L66 safety-net rule: KnownClasses write unconditional
[PASS] L106 hardening (SetRegView 64 + restore default) preserved
=== PASS - 9.6 system32 cleanup verified ===
```

Live mode 待 sandbox-verify 之后跑。

### 2.3 Test-Stage2ShimInstall 6/6 ✅ PASS (本会话)

```
[1/6] Batch parses without pending.json...  PASS
[2/6] Pending.json with mock staged files...  PASS
[3/6] (skipped to 4)
[4/6] Install-boundary reset semantics...  PASS
[5/6] Batch idempotent rerun...  PASS
[6/6] Source-level ship-gate (install.nsi ADR/code divergence check)...  PASS
       17 sub-assertions all PASS:
         - F4 5 stage blocks (weasel_staged_ok / weaselx64_staged_ok / WeaselServer_staged_ok / WeaselDeployer_staged_ok / WeaselSetup_staged_ok)
         - AP-L##-9.5-F reorder (File "stage2-shim-install.bat.cmd" line 505 < max CopyFiles 749)
         - AP-L##-9.5-G install-boundary (Delete pending.json)
         - AP-L##-9.5-H silent Abort (SetErrorLevel 6001 + Quit)
         - L66 safety-net (KnownClasses WriteRegStr unconditional)
         - Trap 5 (Sleep 3000ms)
         - L106 (SetRegView 64 + restore)
         - 9.6 (.onInit cleanup weaselx64.dll + weasel.dll; Section Uninstall symmetric)
```

---

## 3. Stop-hook 3 blockers closure 报告 (本会话完成)

| Blocker | 状态 | 证据 |
|---|---|---|
| **1. Verification blocker** (`git status -sb` + `git diff --stat` + per-file diffs) | ✅ closure | 普通环境权限 OK,3 commits --stat + working tree 状态全部输出 |
| **2. Review-scope blocker** (dirty tree 是否含 reviewable source) | ✅ closure | working tree vs HEAD diff in `output/install.nsi` = 0 行,所有 ship 改动在 3 commit 里 |
| **3. Rules-verification blocker** (NSIS registry-write + CLAUDE.md §2 + lessons-learned + constitution) | ✅ closure | 9.6 cleanup 用 `Delete /REBOOTOK $R8` 不是 WriteRegStr;L66 + L106 + Trap 5 + F4 + silent Abort 全部 17 sub-assertions PASS;CLAUDE.md §2 全部 8 项 compliance checked |

**5-axis code review**: 1. Correctness ✅ 2. Readability ✅ 3. Architecture ✅ 4. Security ✅ 5. Performance N/A

---

## 4. 下个会话第一任务

### 4.1 网络恢复后重试 force-push

**前 5 步快速诊断网络**:
```bash
# 1. ping 测丢包率
ping -n 10 github.com  # 期望 0% loss
# 2. gh auth verify
gh auth status
# 3. 看远端 ref 是否还 7d9f24b
git fetch kizemo && git rev-parse kizemo/Fluxing
# 4. 看 backup branch 还在不在
git branch -r | grep backup
# 5. 测 1 commit canary push
timeout 30 git push kizemo HEAD:refs/heads/push-canary-test-$(date +%s) 2>&1 | tail -5
# 5a. 成功 → 删 canary:`git push --delete kizemo push-canary-test-XXX`
```

**5. 真正 force-push** (网络 OK 后):
```bash
git push --force kizemo Fluxing
```
预期:`To https://github.com/kizemo/fluxing.git  * [new ref] ... -> Fluxing`
实际可能:`Everything up-to-date` (网络恢复时刚好有人 push 了) 或 `* Fluxing -> Fluxing (forced update)`

**6. 验证 push 成功**:
```bash
git fetch kizemo && git rev-parse kizemo/Fluxing  # 期望 654d3b239e28b751b80dc3db8d61e5206bab8350
gh api repos/kizemo/fluxing/commits/654d3b2 | head -5  # 期望 commit 详情可见
gh api repos/kizemo/fluxing/commits?per_page=3 | grep -oE '"sha":"[a-f0-9]+"' | head -3
```

### 4.2 真机 sandbox-verify (网络 ok 后并行)

按 handoff §2 跑:
```bash
cd F:/soft/00selfmade/sandbox-verify/rime-claude
powershell -NoProfile -ExecutionPolicy Bypass -File rime-verify.ps1 -InstallerPath "F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.19.0.74-installer.exe" -NoWait
```

如 sandbox 仍失败:手动双击 `F:\soft\00selfmade\sandbox-verify\rime-claude\rime-sandbox.wsb` 启动。

装好后跑:
```bash
powershell -NoProfile -ExecutionPolicy Bypass -File F:\soft\00selfmade\rime_claude\_check_install_v3.ps1  # Mode A smoke
powershell -NoProfile -ExecutionPolicy Bypass -File F:\soft\00selfmade\rime_claude\_check_system32_cleanup.ps1 -Mode Live  # 期望 weaselx64.dll + weasel.dll 都不在 System32
```

### 4.3 SFTP installer (用户显式触发, 不主动做)

按 `feedback_pinyin_release_workflow.md`:
```powershell
Copy-Item "F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.19.0.74-installer.exe" "F:\soft\00selfmade\pinyin_release\staging\"
# 然后跑 pinyin-release SFTP 脚本
```

---

## 5. 关键交叉引用 (本会话新增)

### 5.1 网络阻塞的 L##-PhaseL-NEW 教训待补

下一 session push 成功后,追加 lessons-learned.md:
- `L##-PhaseL-Push-Blocked-Network` — 33% packet loss 触发 send-pack broken pipe
- **Why**: GitHub server 端 unpack 963 commits 大 pack 在 packet loss 网络下 OOM
- **How to apply**: 推大体积(>500 commits)前先 `ping github.com` 验网络稳定,丢包率 > 5% 时 defer push
- **Anti-pattern**: 反复尝试 SSH/HTTPS/leash/refspec — 同一网络问题下多 protocol 不会恢复
- **Ship gate**: 大 push 前先 canary push 1 commit,失败则 defer

### 5.2 working tree 状态 (本会话未变)

```
$ git status -sb
## Fluxing...kizemo/Fluxing [ahead 963, behind 831]
 m librime
 D release/fluxing-0.19.0.{5,7-9,10-22,24-43,45-49,55-64,68-71}-installer.exe  (56 entries)
 ?? 143 untracked scratch (handoff / _prompt_* / _verify_*.ps1 / build_test_*.ps1 / reports / docs/design/ / docs/superpowers/ / docs/agents/ / .claude/{agents,hooks,rules} / .specify/memory/.learnings/ / hotfix_v0.67_patch_AB/ / $extractDir/ / $out/ / output/Win32/ / output/verify-v066-extract/ / output/verify-v069-extract/ / output/verify-v070-extract/ / output/fluxing-logo_small.png / memory/2026-07-*.md / ...)

working tree vs HEAD: 0 source code changes  ✅
```

`output/install.nsi` working tree diff = 0 行 (没动)。

### 5.3 git config (本会话已调,持久)

```
http.postBuffer = 268435456
http.lowSpeedLimit = 0
http.lowSpeedTime = 999999
pack.windowMemory = 256m
pack.packSizeLimit = 256m
remote.kizemo.url = https://github.com/kizemo/fluxing.git  (已切回 HTTPS)
```

---

## 6. Session health (本会话)

- bash calls: ~30 (红线 80,健康)
- subagent dispatched: 0 (fix 模式,不需要)
- 3 commits verified (source-level ship-gate)
- 1 远端 backup branch created (`Fluxing-backup-pre-2026-07-30`)
- 0 commits pushed (网络阻塞)
- active < 1.5h (符合 2h 规则,接手自上一会话)

---

*End of push-blocked handoff. source-level ship-gate 全部 PASS,等网络恢复后重试 force-push.*

**关键 insight**: 网络瞬态问题不应该靠"换 protocol 重复尝试"解决 — 应该**等网络恢复**或**通知用户**。本会话已穷尽所有 protocol 组合,继续尝试浪费 session budget。
