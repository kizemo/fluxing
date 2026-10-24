# v0.19.0.59 — Stable Baseline Milestone

**Installer attached:** `fluxing-0.19.0.59-installer.exe` (43,379,147 bytes, md5 `69fa60cd5a30763c970ad54867ed10d7`)
**Source commit (local only):** `1c0d711` (2026-07-24, **NOT pushed to remote** — see note below)
**Tag name:** `v0.59-stable-baseline`
**Release target_commitish:** remote `Fluxing` branch (because commit 1c0d711 is local-only, see "Caveat" below)

---

## ⚠️ Caveat: commit 与 branch 的关系

由于 2026-08-01 GitHub `git-receive-pack` 端点持续返回 HTTP 500,本 release 创建时:
- ✅ Release 创建走的是 GitHub **API**(api.github.com),**工作正常**
- ❌ `git push` 走的是 `git-receive-pack`,**4 次 500**
- 因此 commit `1c0d711` **未能 push 到 remote**

**本 release 的 target_commitish 是 remote `Fluxing` 分支 HEAD** (commit `7d9f24b`),不是 `1c0d711`。
GitHub UI 会显示 "v0.59-stable-baseline" tag 指向 `7d9f24b` (或 release 创建时的 Fluxing HEAD)。

**但这不影响核心目的**:attached 的 `fluxing-0.19.0.59-installer.exe` 是 **真实的 v0.59 binary**,可独立下载重装,不需要 git 仓库。User 已装机验证 OK。

---

## 这是什么

v0.19.0.59 是 **2026-07-24 ship 的最后一个 user-verified OK 版本**,也是 v0.66 disaster chain 之前的最后一个 stable milestone。

本次 release 的目的不是 "宣布 v0.59 是新版本",而是**锁死一个永久回滚点**:
- 如果未来 v0.75 / v0.76 任何新 ship 失败,`git checkout v0.59-stable-baseline` 或重新装这个 installer 就能回到这个好用的状态
- 配套 `release/fluxing-0.19.0.59-installer.exe` 是 user 装机验证 OK 的 binary,可独立下载重装,不需要 git 仓库

---

## v0.59 包含的稳定特性

| 特性 | 状态 | 备注 |
|---|---|---|
| QuickPanel manual `Alt+,` | ✅ | spec 070 v3-rev3 (pure GDI + WIC) |
| 常用短语 out-of-process | ✅ | Phase K3 spec 051 |
| A5 sftp_sync 通用化 | ✅ | 56 tests PASS |
| User install verified OK | ✅ | 2026-07-31 真机验证通过 |

---

## v0.59 **不**包含(后续版本引入,均有问题)

| 特性 | 引入版本 | 已知问题 |
|---|---|---|
| FocusIn Show re-enable | v0.65 (`84f01c5`) | 崩溃源 |
| System32 shim DLL pollution | v0.66 (`c9bb103`) | ctfmon/TextInputHost load 路径污染,跨进程 0xc0000374 |
| Stage 2 atomic install | v0.74.1 (`4694a1d`) | silent fail |

> WeaselPanel GDI+ per-input draw 在 v0.59 已存在(WeaselPanel.cpp:107 `GdiplusStartup()`),user 在 v0.59 装机中**未报告** per-input crash。这与 v0.66+ 事故不同:user 装机时 system state clean + v0.59 在 system32 pollution 之前 ship。

---

## 事故链参考

完整 4 周事故链(`v0.66 → v0.67 → v0.68 → v0.69 → v0.70 → v0.71 → v0.74.1 → v0.74.2 → v0.74.3 → v0.62 rollback`)详见:
- `.specify/memory/lessons-learned.md` L##-PhaseL-9.11-Retro v0.74.x
- handoff: `handoff-v0.59-stable-baseline-2026-08-01.md`

---

## 装机验证(post-install state)

装机后 3 个关键 registry key 验证结果(2026-08-01):

| Key | Status |
|---|---|
| `HKLM\SOFTWARE\Microsoft\CTF\KnownClasses\{A3F4CDED-...}` | ✓ "Fluxing Text Service" |
| `HKCU\Software\Microsoft\CTF\Assemblies\0x00000804` | ✓ Phrase + IPC assemblies |
| `HKLM\SOFTWARE\Classes\CLSID\{A3F4CDED-...}\InprocServer32` | ✓ points to `D:\Program Files\fluxing\weasel\weaselx64.dll` |

State pollution audit(System32 shim / crash dir / Stage 2 residue / scheduled tasks):**全部 CLEAN**。
