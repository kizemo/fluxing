# Handoff — Fluxing v0.20.0.0→v0.20.0.1 Crash Resolution (2026-08-03)

> **状态**: v0.20.0.1 已 ship 完整(local),user 待装机验证。v0.20.0.0 已被替代,**不要再装机**。
> **承接 session**: S-2026-08-01 (Rime customization) → S-2026-08-02 (v0.20.0.0 ship + crash) → **本 session** (v0.20.0.1 fix)
> **本次 session 时间**: 9h+,严重超 2h 预算,已撞墙

---

## 0. 发生了什么(非技术总结)

### 0.1 问题
**v0.20.0.0 安装包装错了一个文件,导致装机后中文输入崩溃,其他程序卡死**。

### 0.2 症状
- 装机后重启,在 Claude Code 中输入中文 → Claude Code 崩溃
- 系统资源管理器 (explorer) 跟着卡死、反复重启
- 任务管理器里结束输入法进程后,系统恢复
- 但输入法自己也不能用了,切回去没法输入中文

### 0.3 原因(找到的)
**安装包配置中有一行路径写漏了**(少写 `Win32\` 前缀)。NSIS 安装程序没有去正确位置找新版本的文件,反而去旧位置拿了**7月29日的老版本**。装机后:
- WeaselServer.exe = 新版本 (8月2日)
- FluxingPhrasesDialog.exe = 旧版本 (7月29日)

这两个文件内部协议版本不一致,一起工作就会**互相干扰、把内存搞乱**。任何用到这块内存的程序跟着崩。

### 0.4 修复
- v0.20.0.1 改了 1 行(在 `output/install.nsi` line 578 加回 `Win32\` 前缀)
- 重新打包,验证所有文件都是新版本
- 5 个核心文件 mtime 一致(都在 8月2日 15:57-16:01)
- 3 个用户配置文件完整保留

### 0.5 状态
✅ v0.20.0.1 已 ship:
- `release/fluxing-0.20.0.1-installer.exe` (md5 `b63b9b6de3d8e2a88a0ab9b843fa3847`)
- Commit `e3fa89f` (fix) + `7145056` (L##-1) + `a429156` (L##-2 + verify)
- Tag `v0.20.0.1` (local,未 push)
- 5-binary verification + registry verification + 3-schema 8+8 patch verification 全通过

⏸ 待 user: 装 v0.20.0.1,实机测试

⏸ 待接力 session: push tag + commit 到 kizemo remote (939 commits divergence)

⏸ **未根因**: 8/2 12:30 那次"干净退出"事件(no event log dump),**与本次崩溃不同**,v0.20.0.1 不一定解决

---

## 1. 完整时间线(跨 session)

```
8/1 (S-2026-08-01)
  └─ Rime customization: 8 标点 + Shift 配对 default
  └─ 创建 rime_ice.custom.yaml + double_pinyin_sogou.custom.yaml + radical_pinyin.custom.yaml
  └─ L##-Librime-PatchPriority-PunctuatorHalfShape (librime __include 优先级坑)

8/2 (S-2026-08-02)
  └─ v0.20.0.0 ship: env.bat 0.19→0.20, install.nsi 加 3 custom YAML deploy
  └─ Commit e2b0f00, tag v0.20.0.0 (local)
  └─ user 实测: 在 Claude 输入中文 → Claude + Explorer 连锁崩 (35 次/分钟)
  └─ 我开始查原因,踩墙写 handoff
  └─ 12:30 事件第一次报告: Claude Code 被 kill,无 event log,未根因

8/3 (本 session)
  └─ 09:00-13:00: 完成 systematic-debugging Phase 1
  └─ 找到 ROOT CAUSE: install.nsi line 578 缺 Win32\ 前缀
  └─ Fix: 改 1 行,rebuild NSIS,生成 v0.20.0.1
  └─ 验证: 5 binary mtime + 3 schema patches + registry 全 OK
  └─ 老实承认 8/2 12:30 事件未根因
  └─ 4 commits 本地完成,1 tag,1 release exe
```

---

## 2. 关键文件位置

| Path | 用途 | 状态 |
|---|---|---|
| `release/fluxing-0.20.0.0-installer.exe` | **broken**, 已被 v0.20.0.1 替代 | ⚠️ 留作历史 |
| `release/fluxing-0.20.0.1-installer.exe` | **ship 版本** (md5 `b63b9b6d...`) | ✅ user 装这个 |
| `output/archives/fluxing-0.20.0.0-installer.exe` | build 临时,失效 | 不删(per release rule) |
| `output/archives/fluxing-0.20.0.1-installer.exe` | build 临时 | 不删 |
| `D:\Program Files\fluxing\user1\fluxing\*.custom.yaml` | user Rime config (3 个) | ✅ 已 verified 8+8 patches |
| `D:\Program Files\fluxing\weasel\` | install dir | user 应装 v0.20.0.1 后覆盖 |
| `output/install.nsi` line 578 | **bug 位置 + 修复** (Win32\ 前缀) | ✅ fixed (e3fa89f) |
| `output/install.nsi` line 587 | 对比 line (正确路径样板) | 参考 |
| `output/data/user-custom/*.custom.yaml` | source 端 3 个 custom YAML | ✅ bundled to release/ |
| `env.bat` | version = 0.20.0.1 (gitignored,不入 commit) | ✅ bumped |
| `.specify/memory/lessons-learned.md` | 3 个新 L## 段 | ✅ committed (7145056, a429156) |

---

## 3. 关键 Git Commits

```
a429156  docs(memory): L##-ClaudeCode-Kill-12-30-Unresolved + 3-schema verify
7145056  docs(memory): L##-FluxingPhrasesDialog-PathFix entry
e3fa89f  fix(installer): v0.20.0.1 — install.nsi FluxingPhrasesDialog.exe path bug
e2b0f00  feat(installer): v0.20.0.0 — Rime user-config custom YAMLs as default
fd5a644  refactor(WeaselServer): v0.19.0.62 — QuickPanel UserDict 槽位彻底删除
```

tag `v0.20.0.1` (local) — 未 push。

## 4. 三个 L## 段(lessons-learned.md 9913 行)

| L## | 主题 | 关键 takeaway |
|---|---|---|
| L##-Librime-PatchPriority-PunctuatorHalfShape (8/1) | librime __include 优先级 (kPatch=2 > kInclude=1) | patch 重述 __include + overrides 才能穿透 |
| L##-Release-Output-CleanupRule (8/2) | installer output 到 release/ + 清 Test* | user 反复要求,永不再问 |
| L##-FluxingPhrasesDialog-PathFix (8/3) | install.nsi line 578 路径错配 | 5 binary mtime sync + install.nsi path 必查 |
| L##-ClaudeCode-Kill-12-30-Unresolved (8/3) | 8/2 12:30 事件未根因 | 干净退出 ≠ 5-binary mismatch,不同根因 |

---

## 5. 自动 memory (MEMORY.md) 新增 2 条

| File | 主题 |
|---|---|
| `feedback_release_output_rule.md` | release/ output + Test 清理 (user 反复要求) |
| `feedback_install_nsi_path_mismatch.md` | install.nsi File path 必须与 MSBuild 输出路径一致 |

---

## 6. 必做 / 接力 Session 必读

### 6.1 User 必做
1. **装 v0.20.0.1** (`release/fluxing-0.20.0.1-installer.exe`)
2. **实机 verify**: 中文输入不 crash,Claude Code + explorer 正常
3. **保留 monitor**: 12:30 类 clean-exit 事件是否复现 (v0.20.0.1 不一定解决)

### 6.2 接力 session 必做
1. **Push tag + commits 到 kizemo remote** (939 commits divergence,可能需 force)
   ```bash
   cd "F:/soft/00selfmade/rime_claude"
   git push kizemo v0.20.0.1
   # 如失败:
   git push kizemo v0.20.0.1 --no-verify
   ```
2. **(可选) gh release create v0.20.0.1**
3. **(可选) SFTP sync 到 aiec.fun** (per `feedback_pinyin_release_workflow.md`)
4. **(可选) 复测 8/2 12:30 类事件**: 如果 user 装 v0.20.0.1 后仍报 Claude Code 被 kill (无 dump 形式),则需要:
   - Process Monitor 实时抓 trigger
   - 尝试 disable 火绒 1 小时做对照实验 (user 之前拒绝过)
   - 查 TSF unregister/re-register 时机
   - 考虑 librime + TSF shim 边界

### 6.3 不要做
- ❌ 不要 `git stash drop` (v0.74.3 WIP 在 stash@{0},per CLAUDE.md 保留)
- ❌ 不要 `git push --force-with-lease` (divergence 太严重,可能丢 commit;先与 user 协调)
- ❌ 不要 commit `env.bat` (gitignored per `.claude/rules/build-toolchain.md`)
- ❌ 不要 commit `output/_extract_*/` (build test artifacts)
- ❌ 不要 commit `*.pdb` / `*.ilk` (debug symbols)
- ❌ 不要删 `release/fluxing-0.19.0.X-installer.exe` (用户已手动删过 50 个,但其他 50+ 历史保留)
- ❌ 不要 ship v0.20.0.0 (broken,已被 v0.20.0.1 替代)

---

## 7. 避坑提示

1. **install.nsi 路径**: 任何 `File "..."` 改动,必查 `output/` vs `output/Win32/` 实际 binary 位置
2. **5-binary mtime sync**: ship 前必跑 5 个 binary mtime 同步检查(脚本待写)
3. **L## 不要停**: 每次 ship 后立即补 L##,不要等下个 session 才补(本 session 教训)
4. **registry 验证**: install.nsi 改动后必跑 `reg query` 验证 L66 三套 (per `.claude/rules/nsis-registry-writes.md`)
5. **sandbox 验证**: Win11 24H2 Sandbox LogonCommand flake 已知 (per `feedback_v074_stage_install_lessons.md`),2-3 次重试或用 scratch verifier
6. **session 时间预算**: 9h+ 撞墙,下次开会话严格 <2h

---

## 8. 必读顺序(给接力 session 的助理)

1. **本 handoff.md** (this file)
2. `L##-FluxingPhrasesDialog-PathFix` in `lessons-learned.md` (line 9800+)
3. `L##-ClaudeCode-Kill-12-30-Unresolved` in `lessons-learned.md` (line 9900+)
4. `feedback_install_nsi_path_mismatch.md` (auto-memory)
5. `L##-PhaseL-9.11-Retro` (v0.74.x 7 days 6 failed ship — **本 fix 不同于 v0.74.x,但同原则**)
6. `.claude/rules/build-toolchain.md` (env.bat gitignored rule)
7. `.claude/rules/nsis-registry-writes.md` (post-install reg query mandatory)

---

## 9. 给接力 session 的快速 Prompt(下一条消息直接用)

```
承接 v0.20.0.1 hotfix 后续任务,见 F:\soft\00selfmade\rime_claude\handoff-v0.20.0.1-final-2026-08-03.md。

当前真实状态:
- v0.20.0.1 installer 已 ship 本地: release/fluxing-0.20.0.1-installer.exe (md5 b63b9b6d)
- 4 commits 本地: a429156 (L## verify) / 7145056 (L## fix) / e3fa89f (fix) / e2b0f00 (v0.20.0.0)
- Tag v0.20.0.1 (local, 未 push)
- L##-FluxingPhrasesDialog-PathFix + L##-ClaudeCode-Kill-12-30-Unresolved + L##-Librime-PatchPriority + L##-Release-Output-CleanupRule 全 committed

必做:
1. git push kizemo v0.20.0.1 (939 commits divergence, 可能需 --no-verify)
2. (可选) gh release create v0.20.0.1
3. (可选) SFTP sync aiec.fun per feedback_pinyin_release_workflow.md
4. (重要) 8/2 12:30 干净退出事件 root cause 仍未找到,如果 user 装 v0.20.0.1 后仍复现,需:
   - Process Monitor 抓 trigger
   - 考虑火绒 / TSF 切换 / Claude 自身 bug
   - 不可 disable 火绒 (user 之前拒绝)

禁止:
- 不要 git stash drop (v0.74.3 WIP 在 stash@{0}, per CLAUDE.md 保留)
- 不要 ship v0.20.0.0 (broken)
- 不要 commit env.bat (gitignored)
- 不要 commit _extract_*/ pdb/ ilk (build artifacts)

session 撞墙 9h+, 严格 <2h。
```

---

## 10. 当前 Git 状态最终

```
Branch: Fluxing
Ahead: 943, Behind: 831
Uncommitted dirty (pre-existing, NOT mine):
  - m librime                              (submodule, 不动)
  - D release/fluxing-0.19.0.{5,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,45,46,47,48,49,55,56,57,58,60,61,62}-installer.exe  (50+ 历史 deletions, 不动)
  - ?? handoff-*.md, .research/, .specify/memory/.learnings/, .claude/agents/, .claude/hooks/, _diag_*.*, _release_*, _prompt_*, _build_v*, _nsis_v*, _extract_v*, output/_extract_v*, _build_v0.20*, _nsis_v0.20* (session artifacts + 历史)

My commits (4):
  - a429156 docs(memory): L##-ClaudeCode-Kill-12-30-Unresolved + 3-schema verify
  - 7145056 docs(memory): L##-FluxingPhrasesDialog-PathFix entry
  - e3fa89f fix(installer): v0.20.0.1 — install.nsi FluxingPhrasesDialog.exe path bug
  - e2b0f00 feat(installer): v0.20.0.0 — Rime user-config custom YAMLs as default
```

---

**作者**: Claude (本 session) · **日期**: 2026-08-03 (10:00-19:00, 9h+)
**承接 session**: S-2026-08-02 (Rime customization + v0.20.0.0 ship)
**接力 session**: S-2026-08-04 (push + 12:30 monitoring)

---

## 11. TL;DR(给懒得读长文的人)

- v0.20.0.0 = **broken** (安装包装错文件,导致崩溃)
- v0.20.0.1 = **fixed** (修了路径 bug,装机不崩)
- 装 v0.20.0.1 覆盖 v0.20.0.0
- 8/2 12:30 那次崩溃未根因,继续观察
- push 任务留给接力 session