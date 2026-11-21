# 批次 BK · 账号页「最近同步」状态行（1017）可见性 —— 补齐 spec 100 FR-016 缺失的观测面

> 日期：2026-11-21 · 分支 `feat/100-cloud-sync` · 本批性质：**产品修复 + 门禁 + 出包装机 + 真机复验 + 提交**
> 上一批：[BJ · BH/BI 回写合并 + BG 资产重建 + A-4/A-5 探针索引修复 + push](handoff-2026-11-21-batch-bj-docs-merge-and-probe-fix.md)
> 触发：用户 2026-11-21 指令「**修 1017 可见性**」
> **未改 `librime/`、未改 IPC 协议消息类型、未新增依赖、未 bump 版本号**（装机版仍 **1.3.8.0**，判定一律用 sha16 / 字节数，版本号无判别力）。

---

## 1. 结论一句话

1017 原本**永远停在静态占位符**「最近同步：尚未同步。」——因为 `RefreshUi()` 从不写它；本批把「真实同步结果」接到该控件上，真机两轮均实测到**带时间戳 + 带结果**的内容（`VERDICT=SYNC_STATUS_OBSERVABLE EXIT=0`）。**顺带暴露了一个此前不可见的独立缺陷**：同步轮次缺 401→续期→重试，访问令牌一过期就持续失败（§7）。

---

## 2. 根因（代码级，可复算）

| # | 事实 | 落点 |
|---|---|---|
| ① | `kIdSyncStatus = 1017` 是「最近同步」状态行；`kIdSyncNow = 1016` 是「立即同步」按钮 | `WeaselServer/Cloud/CloudAccountUi.h`（枚举 `AccountControlId`；注释标明 T305/T505 时代存量 ID，**不得改值**） |
| ② | 布局表**有**它：已登录态 `{kIdSyncStatus, 12, 108, 456, 20}`；`AccountRoleLayout(kAccount)` = 未登录态 ∪ 已登录态 ⇒ **控件两态都被创建**（未登录态隐藏） | `AccountLoggedInLayoutBase()`；批次 BJ 真机 `GEOMETRY_MISMATCH_COUNT=0`、9 条 `G_*` 全 `ok=True` ⇒ 1017 确实被创建且可见 |
| ③ | `CloudAccountController::sync_status()` 返回 `Impl::sync_status_text`；该成员由 `Impl::SetSyncStatus()` 在 **7 个时点**更新（引擎启动前未登录 / 引擎启动失败 / 轮次前未登录 / 成功 / 已跳过 / 失败 / 登出复位） | `WeaselServer/Cloud/CloudAccountDialog.cpp` |
| ④ | **`RefreshUi()` 只写 1006（`controller.status(...)`）、1020 账号标签、1018 历史下拉、1007 设备列表 ⇒ 从不写 1017** | 同上 —— **本批根因** |
| ⑤ | 1017 的文本来自 `WidgetText()` 的**静态字面量**「最近同步：尚未同步。」⇒ 控件永远停在创建时的值 | 同上 |
| ⑥ | 规格 **FR-016** 明写「界面可见：账号页可查看登录状态、账号邮箱、设备列表（可移除设备）、**最近一次同步时间与结果**」 | `.specify/specs/100-fluxing-cloud-account-sync/spec.md` |

**定性**：⑥ 要求存在、④⑤ 使其不存在 ⇒ **规格不缺、实现缺 ⇒ 产品缺陷，不是探针判据问题** ⇒ 本批**不改 spec**。

**与批次 BC 登记的关系**：BC 的 CHANGELOG「1017 无可见承载面 ⇒ 判据无效、不是产品缺陷」在**事实层面成立**（当时确实没有任何承载面），但由此漏登了一个产品缺陷。本批修掉它，同时**维持**原有口径（§6.4）。

---

## 3. 修法与边界（ADR 0069）

`docs/adr/0069-account-sync-status-observability.md`（189 行）。

### 3.1 文案收进共享真相源函数组（R7）

`WeaselServer/Cloud/CloudAccountUi.h` 新增 7 个 inline 函数：

| 函数 | 文案 |
|---|---|
| `AccountSyncStatusTextNeverSynced()` | `最近同步：尚未同步。` |
| `AccountSyncStatusTextNotLoggedIn()` | `最近同步：未登录，无法同步。` |
| `AccountSyncStatusTextRoundNotLoggedIn()` | `最近同步：尚未登录，无法同步。` |
| `AccountSyncStatusTextEngineStartFailed()` | `最近同步：同步引擎启动失败。` |
| `AccountSyncStatusTextSuccess(when, pushed, pulled)` | `最近同步：<time> · 成功（推送 N 条，拉取 N 条）` |
| `AccountSyncStatusTextSkipped(when, reason)` | `最近同步：<time> · 已跳过（<reason>）` |
| `AccountSyncStatusTextFailed(when, reason)` | `最近同步：<time> · 失败（<reason>）` |

- 生产（`.cpp` 7 个 `SetSyncStatus` 调用点 + 默认初始化 + `WidgetText` 1017 分支 + 登出复位）与**单测同源**；单测内写**独立**期望值逐字比对 ⇒ 改生产字面量必红。
- 理由：原先七处文案散在 `.cpp`，**1017 文案零回归护栏**。
- 两处「未登录」vs「尚未登录」措辞**逐字保留**，不合并、不顺手统一。

### 3.2 写入点选 `RefreshUi()`

账号页**唯一**界面刷新入口（`WM_APP_REFRESH` / 登录态切换 / 每轮同步结束都会走到）⇒ 一处接线覆盖全部时机；**不加轮次、不加窗口消息、不加线程**。

```
HWND sync_status = ::GetDlgItem(hwnd, kIdSyncStatus);
if (sync_status != nullptr)
  ::SetWindowTextW(sync_status, controller.sync_status().c_str());
```

- **无条件写、不判登录态**：控件两态都创建 ⇒ 未登录态只影响隐藏控件的内容，登录后显示即最新值。
- **锁序安全**：`RefreshUi` 只取 `impl->mutex`（`sync_status()` 自带互斥），`SetSyncStatus` 也只取 `impl->mutex` ⇒ 同序无环；读的是内存 `wstring`，无 I/O ⇒ 不违反 P2「TSF 路径不做阻塞 I/O」。

### 3.3 明确不做的事

不动控件 ID / 不动布局表 / 不加控件 / **不改 IPC 协议**（不触发 IPC 四边同步红线）；不改 `librime/`、不新增依赖、不 bump 版本号。

---

## 4. 门禁（主会话实测，非子会话自述）

| 项 | 命令 | 实测 |
|---|---|---|
| 构建 | `build.bat release weasel installer` | **`BUILD_EXIT=0`**；weasel.sln Release/Win32 **0 警告 0 错误**；makensis 仅 8 条既有 `warning 7010`（ARM/ARM64 可选产物 + `data\*.gram` + `data\lua\*.txt`） |
| 云单测 | `TestFluxingCloud.exe`（**不在 `weasel.sln`**） | **`RESULT: PASS=624 FAIL=0 / VERDICT: ALL PASS`**（基线 614 ⇒ **+10**；`exe.mtime 22:21:49 ≥ src.mtime` ⇒ 非假绿） |
| L0 | `scripts/gate/L0-static.ps1` | **`checks=15 pass=15 fail=0 skip=0`** |
| 格式 | clang-format 18.1.8 `--style=file --lines=…` 3 个改动文件 | **0 漂移**（`REPLACEMENTS=0`；`RECHECK_REPLACEMENTS=0`） |

---

## 5. 出包 + 装机

| 项 | 值 |
|---|---|
| 载荷 | `output\Win32\WeaselServer.exe` = **2 791 424 B** / 22:16:06 / sha16 `3A0167CD429A671A` |
| 装机包 | `output\archives\fluxing-1.3.8-installer.exe` = **42 984 737 B** / 22:17:42 / sha16 `39D1761C2FEDCBE7` |
| 装机命令 | `cmd /c "…installer.exe /S /D=D:\Program Files\fluxing"` |
| 装机结果 | **`INSTALL_EXIT=0`** |
| 装机件 | `D:\Program Files\fluxing\weasel\WeaselServer.exe` = **2 791 424 B** / sha16 `3A0167CD429A671A` ⇒ **`SHA_MATCH_BUILD_VS_INSTALLED=True`** |
| 服务进程 | **pid 407040**（22:24:25 起，ver 1.3.8.0，单实例）；装机前 pid 479804（20:57:58 起） |
| 未动项 | `HKCU\Software\Fluxing\Weasel\RimeUserDir = D:\Program Files\fluxing\user1\fluxing` **未动**；**未登出、未登录、未改注册表、未杀宿主**（服务进程由安装器自己换掉） |

---

## 6. 真机复验（两轮，均 PASS）

新探针：`reports/fluxing-bk-syncstatus/probe-sync-status.ps1`（纯 ASCII · 483 行 · `PARSE_ERRORS=0` · `NON_ASCII_COUNT=0`）。

- 定位方式：**按控件 ID 1017** 找账号窗顶层窗口（**不按中文标题 / 窗口类名**）⇒ `ACCOUNT_WINDOW_CLASS=FluxingCloudAccountDialog_v1`（类名只作日志）。
- 桌面守卫：`GetThreadDesktop` + `UOI_NAME` ⇒ `DESKTOP=Default`。
- 读文本：`WM_GETTEXT` 跨进程封送；点击：`SendMessage(BM_CLICK)`（带 15 s 超时的工作线程）。
- 判据：文本必须**带时间戳 + 带结果关键词**（成功 / 失败 / 已跳过）且**非占位符**。

### 6.1 第 1 轮（`reports/fluxing-bk-syncstatus/sync-status-20261121-222455.txt`）

```
SYNC_STATUS_CONTROL_FOUND=True   SYNC_STATUS_CONTROL_CLASS=Static   SYNC_STATUS_CONTROL_VISIBLE=True
SYNC_STATUS_BEFORE=最近同步：尚未同步。
SYNC_CLICK_SENT=True
SYNC_STATUS_AFTER=最近同步：2026-11-21 22:24:54 · 失败（访问令牌已过期，请用刷新令牌续期。）
SYNC_STATUS_CHANGED=True   SYNC_STATUS_HAS_TIMESTAMP=True   SYNC_STATUS_HAS_RESULT=True
SYNC_STATUS_AFTER_IS_PLACEHOLDER=False   SYNC_STATUS_RESULT_KIND=FAILED
STATUS_LINE_1006_CHANGED=True   POLL_SECONDS=2   REASON=OK
VERDICT=SYNC_STATUS_OBSERVABLE   EXIT=0
```

⇒ **「修复前 = 静态占位」与「点击后 = 真实结果」在同一轮里同框取证**。

### 6.2 第 2 轮（`reports/fluxing-bk-syncstatus/sync-status-20261121-222555.txt`）

`SYNC_STATUS_BEFORE` = 上轮结果（`最近同步：2026-11-21 22:24:54 · 失败（…）`）⇒ **值跨窗口开关保留**（`RefreshUi` 每次打开都重写）；`AFTER` = `最近同步：2026-11-21 22:25:54 · 失败（…）` ⇒ `VERDICT=SYNC_STATUS_OBSERVABLE EXIT=0`（第 2 次独立复现）。

### 6.3 导出文件 mtime（BG 口径交叉核对）

`<RimeUserDir>\sync\<installation_id>\rime_ice.userdb.txt` mtime：**22:24:53 → 22:25:53**（两轮均前进）⇒ 本次失败**不是**闸门跳过，是**鉴权**失败。

### 6.4 口径维持（两条并存，不互相取代）

- 1017 = **人眼可见**的观测面。
- 「维护闸门是否放行 = **导出文件 mtime 是否前进**」的机读口径**继续有效**（本批不改它）。
- **1017 不升级为闸门判据**（文案不是机读信号）。

---

## 7. ⚠️ 副产物发现（独立缺陷，本批**未修**，待拍板）

**现象**：两轮同步结果都是「**失败（访问令牌已过期，请用刷新令牌续期。）**」。

**代码级根因**：`WeaselServer/Cloud/CloudAccount.cpp` 里，`EnsureFreshToken`（= 401 后用 refresh token 续期）的**调用点只有「设备管理」三处**：

- `GET /v1/devices`（`ListDevices`）
- `DELETE <device_id>`（`RevokeDevice`）
- `DELETE /v1/account/data`（清空云端数据）

**同步轮次的上传 / 下载路径没有这层 401→续期→重试** ⇒ 访问令牌一过期，**同步就持续失败**（除非先触发一次设备列表刷新顺带续期）。

**与本批的关系**：**无关** —— 本批只写界面文本，不碰网络与鉴权。但**此前因 1017 无观测面而不可见**，现在被如实暴露出来 ⇒ 这正是 FR-016 要的效果（界面应当显示最近一次同步的**时间与结果**，失败也要显示）。

**未决**：是否另起一个批次修「同步路径补 401→续期→重试」——**待用户拍板**。

---

## 8. 提交与 push

| commit | 内容 |
|---|---|
| `6f464b5` | `fix(WeaselServer): 账号页「最近同步」状态行(1017)写入真实同步结果（ADR 0069）` —— 9 files changed, 880 insertions(+), 13 deletions(-)：产品码 2 文件 + 单测 1 文件 + ADR 0069 新建 + `docs/adr/README.md` 索引 + `CHANGELOG.md` + 探针与两轮原始证据 3 文件 |
| `485e89f` | `docs(fluxing): 批次 BK 交接文档与交接指针` —— 新建本文件 + `CLAUDE.md` 插入 BK 指针行（BJ 降级为「上一批交接指针（BJ）」） |
| `fbfce22` | `docs(fluxing): 修 ADR 0069 第 25 行「存在」乱码（U+FFFD）` —— 1 file changed, 1 insertion(+), 1 deletion(-)，成因与修法见 §9 第 8 条坑 |

**三判据同取**（不认 `commit_exit=0`）：

```
git cat-file -t HEAD                                  => commit
git show --stat --oneline HEAD                        => 6f464b5 … 9 files changed, 880 insertions(+), 13 deletions(-)
git grep -F 'AccountSyncStatusTextSuccess' HEAD -- WeaselServer/Cloud/CloudAccountUi.h   => 1
git grep -F 'kIdSyncStatus' HEAD -- WeaselServer/Cloud/CloudAccountDialog.cpp             => 3
git grep -F '0069' HEAD -- docs/adr/README.md                                             => 1

# fbfce22（ADR 0069 乱码修复）
git cat-file -t HEAD                                  => commit
git show --stat --oneline HEAD                        => fbfce22 … 1 file changed, 1 insertion(+), 1 deletion(-)
git grep -F '事实 2 — 真实同步结果一直存在，但只活在一个成员变量里' HEAD -- docs/adr/0069-account-sync-status-observability.md   => 1
```

**push**：通道探测（每轮重探）⇒ 首轮直连 `-NoProxy` **timeout**、代理 `http://127.0.0.1:7897` **200**（末轮两条通道**均 200**）；三次 `git push kizemo feat/100-cloud-sync` ⇒ `320c68b..6f464b5`、`6f464b5..485e89f`、`485e89f..fbfce22`，三次 `PUSH_EXIT=0`；`git ls-remote kizemo refs/heads/feat/100-cloud-sync` = `fbfce223eb924d69e22797bda3eacfbf8f92eab8`（末轮实测）。

---

## 9. 本轮工具坑（勿重走）

1. **`TestFluxingCloud` 不在 `weasel.sln`** ⇒ 必须显式 msbuild **且带 `/p:SolutionDir=$repo\`**；否则 `RimeWithWeasel/FluxingLocalChangeHook.cpp(14,10): error C1083: 无法打开包括文件: "FluxingLocalChangeHook.h"`（BG 批同坑，只是报错文件不同）。
2. **NSIS `InstallDirRegKey` 指向 32 位注册表视图**：`HKLM\Software\Fluxing\Weasel\InstallDir` 在 64 位 PowerShell 里**看不到**（真实位置 = `HKLM\Software\WOW6432Node\Fluxing\Weasel\InstallDir`）。本轮实测其值 = `D:\Program Files\fluxing`，与目标一致 ⇒ `/D=` 生效 ⇒ **无需清注册表**；清 `HKCU\Software\Fluxing` 反而会丢 `RimeUserDir`（= 丢用户数据指针）。
3. **`CHANGELOG.md` 是 CRLF-only**（实测 `crlf=7612 bareLF=0`）⇒ 插条目必须**字节级**写入并把 LF 转 CRLF，否则 L0 的 `bareLF` 检查红；而 `docs/archive/handoffs/*.md` 相反是 **LF-only**（BJ handoff `crlf=0 bareLF=179`）⇒ 两种文件别用同一套行尾策略。
4. **整文件 clang-format 有既有漂移**（本批 3 个改动文件分别 38 / 24 / 267 处）⇒ **必须**用 `--lines=起:止` 定点查（区间从 `git diff -U0` 的 hunk 取），否则永远假红。
5. **单测新增代码的格式漂移是真漂移**（本批 6 处）⇒ `clang-format -i --lines=` 就地修后必须再跑一次 `RECHECK_REPLACEMENTS=0` 才算收口。
6. **`Get-Content` 对目录会抛错**，在 `$ErrorActionPreference='Stop'` 下会中断脚本 ⇒ 先 `Test-Path` + `-PSIsContainer` 判型。
7. **`Add-Type` 在同一 PS 会话重复调用会类型重名** ⇒ 探针一律用独立 `pwsh -NoProfile -File` 跑。
8. **中文文件落盘后必须做 U+FFFD 字节级扫描** —— 本批 `write`/`edit` 把 ADR 0069 第 25 行的「存」写成了 **2 个 U+FFFD**（字节 `EF BF BD EF BF BD`）⇒ 判据 = 用严格 UTF-8 解码后统计 `[char]0xFFFD` 出现次数（必须为 0）；修法 = **字节级**替换（`EF BF BD EF BF BD` → `E5 AD 98`，11019 B → 11016 B，行数不变 189）。**`docs/adr/0037-sogou-user-dictionary-import.md` 另有 3 个既有 FFFD（非本批产生，未动）**。

---

## 10. 遗留 / 下一批

| # | 项 | 状态 |
|---|---|---|
| 1 | **同步路径缺 401→续期→重试**（§7） | **独立缺陷，待用户拍板是否另起批次** |
| 2 | 双机 AC 7 条 + restore 端到端 + 输入延迟两组对照 + 缺陷 A 反向判据 G-3/G-5 | ⛔ 需两台机器 + 用户在场 |
| 3 | 18 份 `run-*.log` 被 `.gitignore` 挡住，是否 `git add -f` | 待用户定 |
| 4 | `tmp\bh-c2d-fmt-check.cpp`（83 767 B）本机策略删不掉 | 需用户手动处置 |
| 5 | 账号侧访问令牌已过期（§7 的表象） | 随 §7 一并处置 |

**勿再重跑**：1017 探针两轮（已有 2 轮 PASS 证据）、A-4/A-5、B-1~B-5、`/verify-client-pid`、175%/200% DPI 证据。
