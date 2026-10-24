# Handoff — A5 sftp_sync 通用化 + 8 blocker 修复（2026-07-23）

> **承接 HEAD**: `b0ccfe9` (docs(spec): 051 out-of-process PhrasesDialog 首次入 git + ship 勾选同步)
> **承接 handoff**: `handoff-after-k6-direction-2026-07-23.md`
> **状态**: A5 任务完成；4 轮 stop hook review 累计 8 个 blocker 全修；56/56 tests pass；端到端 dry-run 验证 OK；**未 commit，待 user 决定**
> **会话时长**: 本会话跨度 ~1.5h（与 K6 handoff 之后）

---

## 1. 已完成的工作

### 1.1 A5 主体 — 发版脚本通用化

| 文件 | 行数 | 状态 |
|---|---|---|
| `sftp_sync.py` | 552 | 新建，从 `_sftp_v058.py` refactor |
| `sftp_config.json` | 27 | 已存在（user-local 保留） |
| `sftp_config.example.json` | 29 | 新建，git-tracked template |
| `.gitignore` | +5 行 | 加 `sftp_config.json` + `sftp.json` |
| `test/sftp_sync/test_sftp_sync.py` | 746 | 新建，56 tests |

**设计要点**：
- CLI `--version=X.Y.Z.W` 注入 + `--dry-run` 只预检不传
- 配置文件驱动（`files` 数组加项即增文件，不改代码）
- 凭据从 `sftp.json`（gitignored）读，不硬编码
- expected installer md5 从 `_check_install_v2.ps1`（git-trusted）parse

### 1.2 8 个 Blocker 修复（4 轮 stop hook review 累计）

| # | 位置 | 问题 | 修复 |
|---|---|---|---|
| 1 | `render_files` | expectedKey 配置但 ps1 缺失 → silent downgrade to "no expected" | fail-fast sys.exit + 显式错误信息 |
| 2 | `connect_sftp` | WarningPolicy 接受 unknown host（silent TOFU） | 永远 RejectPolicy；known_hosts 只用于 load |
| 3 | `upload_atomic` | 覆盖式 upload 失败会半更新 public release set | 6 步 atomic：snapshot · stage · verify · rename · final-verify · cleanup |
| 4 | 缺 tests | R6 "new code paths need tests" | 56 tests 覆盖所有路径 |
| 5 | `restore_preship` | rename(preship, live) 在 Windows SFTP 上不覆盖 → 丢失 preship | `swap_restore()` 3-step: live → tmp → preship → live → remove tmp |
| 6 | 同 version preship 残留 | rename(live, preship) 与上次残留冲突 | `make_run_id()` = `<version>-<unix_timestamp>`，每次 ship 唯一 suffix |
| 7 | `server_md5` | 单引号路径突破 shell 引用 | `shlex.quote()` + `validate_remote_path_component()` 严格白名单 `[A-Za-z0-9._/-]` |
| 8 | `sftp_config.json` 泄露 | 提交 user 绝对路径 | `.gitignore` 加 `sftp_config.json` + `sftp.json`；新建 `sftp_config.example.json` template；`validate_config_no_placeholders()` 拒绝 `REPLACE_WITH_` / `<path-to` / `${YOUR_` |

### 1.3 测试覆盖

```
Ran 56 tests in 2.160s OK
```

- 单元：VERSION_RE / validate_md5 / md5_file / load_json / load_sftp_credentials / parse_expected_md5 / render_files / precheck_local / remote_exists / validate_remote_path_component / validate_config_no_placeholders / make_run_id
- 集成（mock）：server_md5（shlex.quote + 拒绝 unsafe path）/ upload_atomic（happy / staged-mismatch / rename-failure / final-mismatch → rollback）/ connect_sftp（with/without known_hosts → RejectPolicy）

---

## 2. 端到端 smoke 验证

```
# 真实 config — OK
$ python sftp_sync.py --version=0.19.0.58 --dry-run
=== Local md5 pre-check ===
  OK fluxing-0.19.0.58-installer.exe: 312053473F9D41FCA65EF659F2185B9A
[DRY-RUN] skip upload — pre-check OK

# example template — 拒绝（按设计）
$ python sftp_sync.py --version=0.19.0.58 --config=sftp_config.example.json --dry-run
  [ABORT] config field 'sftpConfigPath' contains placeholder marker 'REPLACE_WITH': 'REPLACE_WITH_PATH_TO_sftp.json'
          Copy sftp_config.example.json → sftp_config.json and edit paths.
```

v0.19.0.58 installer md5 `312053473F9D41FCA65EF659F2185B9A` 与 `_check_install_v2.ps1:69` `expectedInstallerMd5` 完全一致。

---

## 3. 未 commit 文件（待 user 决定时机）

```
?? sftp_config.example.json
?? sftp_sync.py
?? test/sftp_sync/
 M .gitignore
```

`git status -sb` 确认上述 4 项。注意 `sftp_config.json`（真实路径）已被 `.gitignore` 保护，不会被误提交。

**建议 commit 信息**（待 user 决定是否采纳）：

```
feat(scripts): sftp_sync generic release tool with atomic upload + 56 tests (A5)

Refactor _sftp_v058.py into reusable sftp_sync.py + sftp_config.json driven by
--version CLI arg. Adds 4-round stop-hook review hardening:
- fail-fast on missing/malformed expectedKey (L30/L107)
- paramiko.RejectPolicy always (no silent TOFU)
- atomic upload with explicit-swap rollback (Windows SFTP safe)
- shlex.quote + strict charset validation (defense in depth)
- per-ship unique run-id suffix (no preship collisions)
- sftp_config.example.json template + .gitignore for sftp_config.json/sftp.json

Test: 56/56 pass in test/sftp_sync/test_sftp_sync.py
```

---

## 4. 关键文件索引（不复读，按需读取）

| 路径 | 作用 |
|---|---|
| `sftp_sync.py` | 主脚本 552 行 |
| `sftp_config.example.json` | git-tracked template（placeholder 路径） |
| `sftp_config.json` | user-local 真实路径（gitignored） |
| `test/sftp_sync/test_sftp_sync.py` | 56 tests |
| `.gitignore:128-132` | 新加 `sftp_config.json` + `sftp.json` 规则 |
| `_sftp_v058.py` | K6 一次性脚本（可保留作 backup 或删） |
| `_check_install_v2.ps1:69` | v0.19.0.58 expectedInstallerMd5 source |

---

## 5. Memory 已更新

`~/.claude/projects/F--soft-00selfmade-rime-claude/memory/feedback_pinyin_release_workflow.md` 第 9 步已同步 A5 新脚本（路径 + 凭据读取 + security hardening + dry-run + 新增文件无需改代码）。

---

## 6. 已知风险 / 不动项

- **不动**：`librime` submodule dirty（prior session 残留）
- **不动**：60+ untracked files（prior session 残留）
- **风险**：未在真实 ship 中验证 atomic upload（仅 mock test）→ 建议下版 v0.19.0.59 ship 时观察 SFTP server 行为
- **风险**：`_sftp_v058.py` 与 `sftp_sync.py` 并存 → 下次 ship 时只用新脚本；若 6 个月内未 ship，可考虑删 `_sftp_v058.py`

---

## 7. 下一步候选（user 自选）

| 选项 | 内容 | 时长 |
|---|---|---|
| **Commit** | 上述 4 文件 commit | 5 min |
| **Ship v0.19.0.59** | 跑 `python sftp_sync.py --version=0.19.0.59`（需先 build new installer） | 1-2h（含 build） |
| **A6** | 给 AI 助手批量补"避坑书签" | 1h+ |
| **B1** | 修 UserDict 真能用的版本 | 4-6h |
| **D** | 等装机端 v0.19.0.58 验证 + 用户反馈 | — |

— END OF HANDOFF —