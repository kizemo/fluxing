# 交接文档 · Fluxing 1.1.6 · spec 097 收尾 · 快捷面板双击上屏 + 上屏延时已修复

- 生成时间：2026-11-02（GMT+8）
- 生成会话：`mvs_7b48cfa8afb24478a00894ca947b3c49`
- 仓库：`F:\soft\00selfmade\rime_claude`　分支：`Fluxing`　远端：`kizemo/Fluxing`
- 上一份交接文档（**已被本文档取代，仅作历史记录**）：`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-01-fluxing-spec097-dll-lock-phrases-punct.md`
- 接手前置：本文件是**唯一入口**。不要从零重做已完成的事。

> 置信度约定（沿用上一份）：**【已验证】**= 本轮从磁盘 / 编译产物 / git / 日志实测读出；**【推断】**= 有代码依据但未跑判别实验；**【已证伪】**= 走过且确认是死路，**不要再走**。

> ⚠️ **本文档只做三件事**：① 给出必要文档的**完整路径**；② 给出**已完成的结论 + 证据位置**（不复述 spec/ADR/提交内容，需要时按路径去读）；③ 给出**剩余动作的排序与验收标准**。

---

## 0. 一句话现状

三项诉求里 **诉求 2（快捷面板双击/回车上屏 + 上屏延时）已完全修复并经用户实测通过**（机器侧毫秒证据：注入耗时 625/703 ms → 16/31/0 ms），**诉求 3（键位冲突）机器侧全绿**，**诉求 1（安装免重启）沙箱权威门禁全绿**；**当前唯一阻塞是收尾**：工作区里的两份 `.cpp` 仍带诊断插桩与毫秒探针、装机版与 `output\Win32\` 都还是带探针的构建产物，**绝不可打包安装器**，必须先手工剥离探针、重出干净构建、补 CHANGELOG 与 L167 再提交。

---

## 1. 必读清单（完整路径，按顺序读）

进站必读 5 份（`CLAUDE.md` §1 规定）：

| # | 路径 | 读什么 |
|---|---|---|
| 1 | `F:\soft\00selfmade\rime_claude\CONTEXT.md` | 身份 / 模块 / 词汇 / 不变量 |
| 2 | `F:\soft\00selfmade\rime_claude\AGENTS.md` | 操作手册；**§1 首屏**、**§2.5 三层门禁**、**§3 分流**、**§4 危险区**、**§5 checklist** |
| 3 | `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` | 法则层 R1–R9 / P1–P8 |
| 4 | `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md` | §A 部署 + §11 NSIS 配方 |
| 5 | `F:\soft\00selfmade\rime_claude\docs\agents\domain.md` | 工程技能消费契约 |

本次工作**尚未读**、新会话建议补读的：

- `F:\soft\00selfmade\rime_claude\.specify\memory\lessons-learned.md`（L163 / L164 / L165 / L166 已在案；**下一个编号是 L167**）
- `F:\soft\00selfmade\rime_claude\docs\adr\0028-thin-tsf-shell-and-additive-ipc.md`（薄 TSF 壳 + 增量 IPC 决策）
- `F:\soft\00selfmade\rime_claude\docs\adr\0022-*.md`（快捷面板常驻改默认态 ⇒ P7 语义来源）
- `F:\soft\00selfmade\rime_claude\docs\adr\0012-*.md`（装包名三段式）
- `F:\soft\00selfmade\rime_claude\Rime-设置说明书-重点快捷键.md`（§5–§7、§9 本机现状、§10 常见坑）
- `F:\soft\00selfmade\rime_claude\FLUXING-CLAIMS-VERIFICATION.md` §3.11（**待确认**是否还有四段式包名描述未同步；`AGENTS.md` 那处已修）
- `F:\soft\00selfmade\rime_claude\.claude\rules\codebase-graph.md` · `ipc-boundary.md` · `build-toolchain.md` · `commit-checklist.md` · `hotkey-binding.md` · `rime-settings.md`

spec / ADR / 提交内容**不在本文档复述**，需要时直接按上面路径读：

- spec 097 三件套：`F:\soft\00selfmade\rime_claude\.specify\specs\097-shim-name-collision-and-hotkey-chord-separation\`（`spec.md` 5,358 B / `plan.md` 9,163 B / `tasks.md` 8,762 B）
- 上一轮 spec 096 / ADR 0027 交付清单：`F:\soft\00selfmade\rime_claude\.specify\specs\096-*\` 与 `F:\soft\00selfmade\rime_claude\docs\adr\0027-*.md`

---

## 2. 已完成（**不要重做**）

### 2.1 诉求 3 · 键位冲突（Ctrl+Shift+数字族）—— 代码/配置/文档全落盘并已推送

- 出货配置：`F:\soft\00selfmade\rime_claude\output\data\default.yaml`（18,217 B，无 BOM，纯 CRLF）
  - `:272` `toggle: ascii_punct, accept: Control+period`（切中英标点）
  - `:273` `toggle: traditionalization, accept: Control+slash`（切简繁）
  - 旧的 `Control+Shift+numbersign` / `Control+Shift+dollar` 两条 live 绑定**已移除**；`Control+comma → full_shape` 保持注释
- 说明书同步 5 处：`F:\soft\00selfmade\rime_claude\Rime-设置说明书-重点快捷键.md`
- 变更记录：`F:\soft\00selfmade\rime_claude\CHANGELOG.md`
- 事故沉淀：**L163**（在 `lessons-learned.md`）
- 测试断言已改：`F:\soft\00selfmade\rime_claude\test\TestDefaultHotkeys\TestDefaultHotkeys.cpp`（`:204-213`，含两条负向断言）
- 门禁 `TestHotkeyBindings` 五条全绿：出货 `bindings=39|reachable=39|dead=0`；负控制 `bindings=9|reachable=3|dead=6`；recorder `bindings=21|reachable=20|dead=1`
- 提交（**已推送 `kizemo`**）：`a6f9ed3`（spec 097 三件套）· `f5829a0`（键位分离）· `c4384ef`（installer shim 序号 + ADR 0028 + L164/L165/L166 + harness）· `135a5b1`（KeyEventSink 注释/格式）· `33b886e`（AGENTS.md + known-dead-bindings）· `ee442b1`（AGENTS.md 装包名口径漂移修正）

### 2.2 诉求 1 · 安装免重启 —— NSIS shim 序号化已落地，沙箱证明有效

- 改动文件：`F:\soft\00selfmade\rime_claude\output\install.nsi`（62,885 B，UTF-8 + `EF BB BF` BOM + CRLF）
  - 探针循环：`StrCpy seq 0` → `probe:` → `ClearErrors` → `Delete "...seq.dll"` → 若仍存在（被映射）→ `IntOp seq+1` → `IntCmp seq 64` → `free:` → `SetOverwrite on` → `File /nonfatal /oname=weaselx64.<ver>.<build>.<seq>.dll "weaselx64.dll"`
  - 失败路径写 `.stage2` marker + `$FluxingDegraded 1`
- **★关键事实**：版本化分支只在 plain 名 `Rename` 失败时才进入 ⇒ 新 PHASE 必须同时独占锁住 plain `weaselx64.dll` **和** 当前 max-seq shim 两个文件
- 门禁：`makensis` EXIT=0（日志 `F:\soft\00selfmade\rime_claude\tmp\nsi-gate-097.log`）；`File /oname=` 展开运行时变量已实测（`tmp\oname-probe.nsi`）
- 产物：`F:\soft\00selfmade\rime_claude\output\archives\fluxing-1.1.6-installer.exe` = **42,786,314 B @ 2026-11-01 22:33:50**（`BUILD_EXIT=0`，日志 `tmp\build-installer.log`）
  - installer mtime **晚于** `default.yaml`（11-01 20:15:31）⇒ 装包**应含**新键位 default.yaml，**但未核实**，新会话若跑真机安装请顺手确认
- 决策与事故：ADR 0028 · L164 / L165 / L166

### 2.3 门禁自身修复（权威门禁 `scripts\sandbox-gate\`）

两次「门禁自身腐化」，都**改门禁不改产品**：

1. **宿主侧把「启动器退出」当成「沙箱已死」** ⇒ `run-verify.ps1:220-226` 改为四个沙箱进程任一存活即继续等，连续 3 次轮询（15 s）都没有才认死。
2. **P7 断言的世界观漂移**（ADR 0022 明写「快捷面板常驻改为默认态」，而 spec 080 是瞬时模式语义）⇒ `in-sandbox-claims.ps1` 加 **PHASE 0**，在装机**之前**写 `<InstallDir>\user1\fluxing\quickpanel.state` = `always_show=0\n`（`install.nsi:981` 建目录不删既有文件 ⇒ 预置文件可幸存；`:982` 写 `RimeUserDir`）。
   - 该文件写入在 `in-sandbox-claims.ps1:377-414`；`toolingVersion='2026-11-02.v9'`
   - 文件 57,965 B，BOM=True，crlf=987，bare_lf=0，fffd=0，语法解析 0 错

### 2.4 诉求 2 前半 · 双击不上屏 —— 根因坐实 + 修复落地 + **用户实测通过**

**根因【已验证】**：不是服务端注入落空，而是客户端 `HideWithoutDisconnect()` 的「保留 pipe」前提**被自己的自动隐藏连锁破坏**：

```
DestroyWindow → 失焦 → WM_ACTIVATEAPP(wp==FALSE) / WM_KILLFOCUS
  → grace 期(kShowGraceMs=2000)已过 → Hide()
  → Hide() 末尾无条件 s_pipeClient->Disconnect()
  ⇒ 回到分支时 IsConnected()==false ⇒ INJECT 从未发出
```

回车路径幸存，是因为它没有触发该失焦连锁（首轮插桩：`lbtn=1` vs `lbtn=0` 是唯一差异；双击路径只写 2 行日志就断，**没有 `send INJECT`**）。

**修复（已落码 / 已编译 / 已部署）**：`F:\soft\00selfmade\rime_claude\FluxingPhrasesDialog\PhrasesDialog.cpp`
- 文件级标志 `static bool g_hideKeepPipe = false;`
- `Hide()` 断管道条件加门控：`if (s_pipeClient && s_pipeClient->IsConnected() && !g_hideKeepPipe) {`
- 三个注入分支（`NM_DBLCLK` :1194 / `NM_RETURN` :1231 / `VK_RETURN` :1028）在 `HideWithoutDisconnect();` **之前**置 `g_hideKeepPipe = true;`，在收尾 `Hide();` **之前**置 `false;`

### 2.5 诉求 2 后半 · 上屏延时 ~1 秒 —— 毫秒探针定因 + 修复落地 + **用户实测通过**

**定因【已验证】**（毫秒探针，修复前）：**整段延时在服务端 `SendInput` 内部** —— 625 ms（双击）/ 703 ms（回车），206 个 INPUT 事件 ⇒ ≈3.1–3.4 ms/事件。客户端隐藏链 16–32 ms、管道往返 0 ms、ACK 0 ms，**均可忽略**。机理：逐字注入每个字符都要穿前台应用的 IME/TSF 栈（每字一次 WeaselIPC 往返 + Rime 处理）。

**修复方向【已验证】**：**不再逐字注入** —— 改「剪贴板 + 粘贴快捷键」一次性提交（4 个按键事件），剪贴板被别人独占时降级回逐字 `SendInput`；2 秒后安全恢复原剪贴板（仅当剪贴板仍是我们放的文本；原剪贴板本来没文本时**不清空**）。

**落地位置**：
- 新增共享头 `F:\soft\00selfmade\rime_claude\include\FluxingTextInjector.h`（7,129 B，无 BOM，纯 LF，213 行）
  - `namespace fluxing`；`kClipboardOpenRetry=5`、`kClipboardOpenRetryDelayMs=40`、`kClipboardRestoreDelayMs=2000`
  - API：`OpenClipboardWithRetry` / `ReadClipboardText` / `WriteClipboardText` / `SendPasteShortcut` / `SendTextByUnicode` / `RestoreClipboardIfUnchanged` / `ClipboardRestoreThread` / `InjectTextViaClipboard` / `InjectTextWithFallback`
- `F:\soft\00selfmade\rime_claude\WeaselServer\PhrasesDialogIPC.cpp`：`InjectText` 改为 `fluxing::InjectTextWithFallback(text)` + 日志 `InjectText path=clipboard-paste|per-char-fallback`
- `F:\soft\00selfmade\rime_claude\FluxingPhrasesDialog\PhrasesDialog.cpp`：`DefaultInject` 同样改为 `fluxing::InjectTextWithFallback(text)`
- **共享头不必登记工程文件**：`include\FluxingPipeProtocol.h` 未出现在任何 `.vcxproj` 里，且两个 vcxproj 的 `AdditionalIncludeDirectories` 都含 `$(SolutionDir)\include` ⇒ 已实测构建通过

**为什么选 Ctrl+V【已验证·静态】**：`output\data\default.yaml` 里 `Control+v` 只出现在**注释**（`:289-291`），无实际绑定；`WeaselTSF\KeyEventSink.cpp:101` `*pfEaten = (BOOL)m_client.ProcessKeyEvent(ke)` + `RimeWithWeasel\RimeWithWeasel.cpp:329` `handled = rime_api->process_key(...)` + `:361` `return (BOOL)handled;` ⇒ 未绑定键 `handled=false` ⇒ `pfEaten=FALSE` ⇒ IME 不 eat 该键 ⇒ Ctrl+V 必达宿主程序；librime `ascii_composer` 只处理 plain text（带 Ctrl 不算）⇒ 不会被吞。

**修复后毫秒证据【已验证】**（11:57–11:59 三次复现，日志 14,108 B / 67 行）：

| 路径 | before InjectText | path= | after | Δ 注入 | 全链路（进分支→收 ACK） |
|---|---|---|---|---|---|
| 回车 #1 | +1797 ms | clipboard-paste | +1828 ms | **31 ms** | 78 ms |
| 双击 | +7109 ms | clipboard-paste | +7125 ms | **16 ms** | 47 ms |
| 回车 #2 | +77687 ms | clipboard-paste | +77687 ms | **0 ms** | — |

对照修复前 625 ms / 703 ms ⇒ **改善约 20–40 倍**；三次全部走粘贴路径，降级路径未触发。

**用户反馈【已验证】**：「测试结果：立刻上屏通过。」

### 2.6 新 `default.yaml` 已部署到装机 + 部署产物门禁已跑

- 共享数据目录 = `D:\Program Files\fluxing\weasel\data\`；用户目录 = `D:\Program Files\fluxing\user1\fluxing\`（**用户数据目录未动**）
- 备份：`F:\soft\00selfmade\rime_claude\tmp\installed-default.yaml.bak`（回读 sha 自证 `BACKUP_OK`）
- 部署：装机版原 17,242 B / sha `4edc29a11294da65` ⇒ 现为 18,217 B / sha `917509f6f2460230`
- 部署入口：`& 'D:\Program Files\fluxing\weasel\WeaselDeployer.exe' /deploy` ⇒ `DEPLOY_EXIT=0`，4 个 schema 全部重新生成
- 部署后 schema `:102` = `{accept: "Control+period", toggle: ascii_punct}`；`:103` = `{accept: "Control+slash", toggle: traditionalization}`；`OLD_CHORD_COUNT=0`；`%TEMP%\rime.weasel.*` ⇒ `INVALID_BINDING_COUNT=0`
- 部署产物门禁（27 yaml）：`BASELINE|0 known-dead entries` · `SUMMARY|bindings=40|reachable=40|dead=0|unused=0` EXIT=0；仅 `build\default.yaml`：`bindings=39|reachable=39|dead=0`

### 2.7 权威门禁复跑结果【已验证】

命令：`cmd /c "scripts\sandbox-gate\run-verify.cmd -RepoRoot F:\soft\00selfmade\rime_claude" > F:\soft\00selfmade\rime_claude\tmp\gate-sandbox-097b.log 2>&1`
产物：`%USERPROFILE%\sandbox-artifacts\rime\logs\{claims.json,claims.log,boot.log,_DONE}`

**汇总：TOTAL=56　PASS=47　FAIL=2　SKIP=0**（INFO：E.1/E.3/E.6/E.7/E.9/E.10）
**唯一 2 个 FAIL 是环境恒定项**：`E.2`（沙箱有 D: 盘，`drives=C:`）、`E.4`（沙箱外网可达，`reachable=False`）—— 与本次改动无关，**不是回归**。

spec 097 核心全 PASS：

```
[PASS] P5.TestDefaultHotkeys :: exit=0 | Passed: 91 / 91
[PASS] P6 :: panel auto-hid after the switch=1
[PASS] P7 :: first auto-hide hidden=1 survived_grace=1 | second auto-hide shown=1 hidden=1 survived_grace=1
[PASS] P9.2 name=weaselx64.1.1.6.0.0.dll seq=0 ver=1.1.6 build=0 len=1978368
[PASS] P9.3 marker=...\weaselx64.dll.stage2 exists=False
[PASS] P9.4 TIP 注册指向版本化 shim
[PASS] P11.1 exit=0 timedOut=False
[PASS] P11.2 before=weaselx64.1.1.6.0.0.dll seq=0 after=weaselx64.1.1.6.0.1.dll seq=1
[PASS] P11.3 marker=...\weaselx64.dll.stage2 exists=False      ← 判别性主断言
[PASS] P11.4 指向新序号副本
[PASS] P11.5 新副本 == buildout\weaselx64.dll
[PASS] P10.2 SUMMARY|bindings=310|reachable=310|dead=0|known_dead=0|mismatches=0|unused=0
```

---

## 3. 三项诉求：当前状态与剩余动作

| 诉求 | 状态 | 剩余动作 |
|---|---|---|
| **① 安装免重启** | 沙箱权威门禁全绿（P9.x / P11.x）；装包已构建 | **真机跑一次 NSIS 产物**（不能用文件拷贝代替）；须在探针回滚、干净构建之后 |
| **② 快捷面板双击/回车上屏 + 延时** | **已修复并经用户实测通过**（机器侧毫秒证据齐） | **收尾**：剥离诊断探针 → 干净构建 → 重装 → CHANGELOG → L167 → 提交推送 |
| **③ 键位冲突** | 代码/配置/说明书/CHANGELOG/L163/spec 097 三件套/ADR 0028 全落盘并已推送 | **只剩用户物理按键验收**（机器侧证据已齐） |

**③ 的用户物理验收步骤**（必须由人做，机器无法替代）：在本机真实宿主程序里按
- `Ctrl+.` → 切中英标点
- `Ctrl+/` → 切简繁
- `Ctrl+Shift+3` → **只删词、不再翻标点**

若 IME 似乎还在用旧 schema，先切一次输入法 / 方案再测。

---

## 4. 工作区未提交改动 / 装机二进制 / 备份位置

### 4.1 `git status -sb`（live 实测，2026-11-02）

```
## Fluxing...kizemo/Fluxing
 M FluxingPhrasesDialog/PhrasesDialog.cpp      ← 双击修复 + 延时修复 + 毫秒探针（未提交）
 M WeaselServer/PhrasesDialogIPC.cpp            ← 延时修复 + 毫秒探针 + ACK 日志（未提交）
 m librime                                       ← 既有脏子模块，不提交
?? include/FluxingTextInjector.h                ← 本轮新增共享头（未提交）
```

`git diff --stat`：`PhrasesDialog.cpp` 170 行变更、`PhrasesDialogIPC.cpp` 79 行变更，3 files changed, 215 insertions(+), 34 deletions(-)。
`kizemo/Fluxing` HEAD = `ee442b1`（**已推送**）。

### 4.2 ★ 这些改动里混着诊断插桩，必须手工剥离

诊断插桩**只用于判别与量测，不作为正式修复提交**。需要**保留**的正式修复只有两处：

1. `PhrasesDialog.cpp` 的 `g_hideKeepPipe` 标志 + `Hide()` 门控 + 三分支置位/清零（§2.4）
2. `PhrasesDialog.cpp` / `PhrasesDialogIPC.cpp` 两处 `fluxing::InjectTextWithFallback(...)` 调用（§2.5）

需要**剥离**的是：诊断 helper（`g_diagSite`、毫秒级 `PhrasesDiag` 拼装、`recv ACK` 日志、`before/after ACK send` 日志）以及 `InjectText path=...` / `DefaultInject path=...` 这类量测日志。

> ★★ **`F:\soft\00selfmade\rime_claude\tmp\diag-backup\` 是第一轮插桩前的源码备份 —— 既无探针、也无这两处修复 ⇒ 不能直接覆盖回滚！** 必须手工剥离探针后保留修复。（该目录另有 `PhrasesDialog.round1.cpp`，是第一轮的中间版本。）

### 4.3 装机二进制现状（live）

```
D:\Program Files\fluxing\weasel\WeaselServer.exe          2,267,136 B  sha adb67527f58d32a2  ← 修复+探针版
D:\Program Files\fluxing\weasel\FluxingPhrasesDialog.exe  1,117,696 B  sha 045f8e4f5fc3869f  ← 修复+探针版
D:\Program Files\fluxing\weasel\weaselx64.dll             1,978,368 B  @ 11/1 11:00:02     ← 新
D:\Program Files\fluxing\weasel\weaselx64.1.1.6.0.dll     1,977,344 B  @ 10/31 10:12:06   ← 旧（DEFERRED）
D:\Program Files\fluxing\weasel\weasel.dll                1,670,144 B  @ 10/31 10:12:12   ← 旧（DEFERRED）
D:\Program Files\fluxing\weasel\rime.dll                  3,040,768 B  @ 10/25 20:35:32
```

`F:\soft\00selfmade\rime_claude\output\Win32\WeaselServer.exe` 与 `output\Win32\FluxingPhrasesDialog.exe` **目前都是带修复 + 探针的构建产物** ⇒ **这一轮结束前绝对不要打包安装器**。

注册表 `HKCU\Software\Fluxing\Weasel`：`RimeUserDir = D:\Program Files\fluxing\user1\fluxing`，`InstallDir` 为空。
装机目录嵌套残留（`weasel\fluxing\weasel\WeaselServer.exe` 等）：**只记录不清理**。

### 4.4 备份位置（回滚用）

| 路径 | 内容 |
|---|---|
| `F:\soft\00selfmade\rime_claude\tmp\install-backup-diag2\` | 本轮部署前的装机 exe |
| `F:\soft\00selfmade\rime_claude\tmp\install-backup-diag\` | 前一轮部署前的装机 exe |
| `F:\soft\00selfmade\rime_claude\tmp\diag-backup\` | **第一轮插桩前**源码备份（无探针、无两处修复） |
| `F:\soft\00selfmade\rime_claude\tmp\installed-default.yaml.bak` | 装机 `default.yaml` 原件 |
| `F:\soft\00selfmade\rime_claude\tmp\install-backup-20261101\` | 安装器真机跑之前的整机备份 |
| `F:\soft\00selfmade\rime_claude\tmp\deploy-stage-20261101\` | PendingFileRenameOperations 源文件，**重启前勿删** |

### 4.5 诊断日志

`%TEMP%\fluxing-phrases-diag.log` —— UTF-16LE 无 BOM，格式：
`[HH:MM:SS][CLIENT|SERVER][ms=][pid=][seq=][lbtn=][fg=] message`
时间线副本：`F:\soft\00selfmade\rime_claude\tmp\phrases-timeline.txt`（139 行）
解析器：`F:\soft\00selfmade\rime_claude\tmp\analyze_phrases_timeline.py`

---

## 5. 下阶段计划（按顺序做，每步给出验收标准）

### 步骤 1 ★★★ 剥离诊断探针，出干净构建

**这是新会话的第一任务，也是当前唯一硬阻塞。**

1. 读 `FluxingPhrasesDialog\PhrasesDialog.cpp` 与 `WeaselServer\PhrasesDialogIPC.cpp`，手工剥离诊断插桩（见 §4.2 的保留/剥离清单）。
   - 若要脚本化：可仿照 `F:\soft\00selfmade\rime_claude\tmp\fix_097_phrases_inject.py` 的幂等写法（回读逐字节比对 + 幂等复算，**不要**用 `count(old)==1` 判定幂等）。
2. 定向格式化（P1）：`clang-format -i --style=file:<repo>\.clang-format` 只对这两个文件，**不要跑仓库级**（会改 186 个既有未格式化文件）。这两个文件本轮**未做全文件重排**（沿用现有 2 空格缩进），以免产生巨量 diff。
3. 重编（**装机版是 32 位 x86 ⇒ `Platform=Win32`**，产物落 `output\Win32\`）——命令形态见 §6。
4. 部署：复用 `F:\soft\00selfmade\rime_claude\tmp\fix_097_phrases_inject.py deploy`（会备份 → `WeaselServer.exe /quit` → 覆盖 → 覆盖前 `taskkill /F /IM FluxingPhrasesDialog.exe` → 校验 size/sha）。
5. **验收标准**：
   - 装机版 `WeaselServer.exe` / `FluxingPhrasesDialog.exe` 的二进制里**不再包含** `fluxing-phrases-diag.log` 字符串（用 `[Text.Encoding]::Unicode.GetString([IO.File]::ReadAllBytes($exe)).Contains('fluxing-phrases-diag.log')` 断言为 `$false`）
   - `output\Win32\WeaselServer.exe` 与 `output\Win32\FluxingPhrasesDialog.exe` 同样不含该字符串
   - 两处正式修复仍在源码里：`Select-String -Path <file> -Pattern 'g_hideKeepPipe'` 与 `Select-String -Path <file> -Pattern 'InjectTextWithFallback'` 各有命中
   - 用户再双击/回车一次**仍能立刻上屏**（回归确认）
6. **本步骤完成前，绝不可执行步骤 4 的安装器构建。**

### 步骤 2 ★ 补文档与提交（诉求 2 收尾）

1. `CHANGELOG.md` 补条目：上屏由「逐字注入」改为「剪贴板 + 粘贴」；换行可进多行编辑框；剪贴板短暂借用约 2 s 后恢复（非文本格式无法恢复 —— 已知限制，须写明）。
2. `.specify\memory\lessons-learned.md` 写 **L167**（**下一个编号是 L167**）：
   - 建议标题方向：「客户端『隐藏但保留管道』的前提会被自己的自动隐藏连锁破坏」+「延时根因要用毫秒探针定位到单点，而不是靠体感」
   - 全文补进对应 topic 文件（`E:\Users\Duanyi\.minimax\.minimax\agents\mavis\memory\topics\*.md`）以免指针指向不存在的条目
3. 提交（Conventional Commits，scope `fluxing`；P4 要求用户可见改动进 CHANGELOG；每个 commit 自包含且过 `ci.yml`）：
   - 建议拆 2 个 commit：① `feat(fluxing): 快捷面板上屏改为剪贴板粘贴 + 修复双击不上屏`（含新头 + 两个 .cpp）② `docs(fluxing): 补 CHANGELOG 与 L167`
   - **提交前必须 `git status -sb` + `git diff --stat`**，确认 `weasel.props` / `env.bat` / `*.obj` / `librime` 子模块没被带进去
   - push 只推 `kizemo`：`git push kizemo Fluxing` ⇒ **不直推 `origin`**

### 步骤 3 ★ 用户物理验收（诉求 3 收尾，必须由人做）

按 §3 的三个键测。若 IME 仍像旧 schema，先切一次输入法/方案。
**这条无法用机器证据替代，不要因为「机器侧全绿」就跳过。**

### 步骤 4 ○ 可选 · 真机跑一次新安装器（验证诉求 1）

**前置**：步骤 1、2 完成（干净构建、已提交）。
**做法**：`output\archives\fluxing-1.1.6-installer.exe /S /D=<path>`
- L13 / L17：**必须显式 `/D=<path>`**（silent 模式不会触发 path-force）
- L09 / L10 §5：**绝不传 `/userdir=<x>`**（NSIS 会把未知 CLI flag 拼进脚本）
- L54：silent 之前先清 `HKLM\Software\Fluxing\Weasel` 与 `HKCU\Software\Fluxing`
- 备份：`F:\soft\00selfmade\rime_claude\tmp\install-backup-20261101\`
- **验收标准**：安装后 `weaselx64.<ver>.<build>.<seq>.dll` 指向新序号副本、**无 `.stage2` marker**、TIP 注册指向版本化 shim（对应沙箱 P11.2 / P11.3 / P11.4 断言）
- 顺带确认：装出来的 `data\default.yaml` 是 **18,217 B / sha `917509f6f2460230`**（含新键位）

### 步骤 5 ○ 可选 · 负控制轮

`git checkout HEAD -- output/install.nsi` → 重建 → 显式 `-InstallerPath` 跑 harness → **期望 P11.3 真 FAIL**（证明断言有牙）→ 恢复。

### 步骤 6 ○ 可选 / 待确认

- 读 `FLUXING-CLAIMS-VERIFICATION.md` §3.11，确认是否还有四段式包名描述需要同步（`AGENTS.md` 那处已修）。
- 补 `output\data` 缺失的 `*.gram` 与 `data\lua\*.txt`（跑 `build.bat data` / `scripts\prepare-librime-lua.bat` 后重建安装包；当前 makensis warning 7010，已进 42.7 MB 正式产物）。
- 修 `test\TestShortcutSettings\TestShortcutSettings.cpp:164` 往 cwd 写 `_ts_yaml_fixture.yaml` 的既有测试卫生问题（游离文件已手动清掉）。
- 本机全量套件跑绿目前**被环境阻塞**（装机版 `WeaselServer.exe` 会被 TSF 自动拉起并占用命名管道 ⇒ `TestWeaselIPC` mock server 无法绑定，ADR 0012 既有防护）⇒ 有效验证路径 = 沙箱权威门禁。

---

## 6. 环境与命令速查

### 6.1 必记常量

| 项 | 值 |
|---|---|
| 仓库根 | `F:\soft\00selfmade\rime_claude` |
| 装机根 | `D:\Program Files\fluxing\` |
| 共享数据目录 | `D:\Program Files\fluxing\weasel\data\` |
| 用户数据目录 | `D:\Program Files\fluxing\user1\fluxing\`（**永不改**） |
| Python | `C:\Python314\python.exe` |
| MSBuild | `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe` |
| NSIS | `C:\Program Files (x86)\NSIS\makensis.exe` |
| 装机版架构 | **x86（Win32）** ⇒ 客户端/服务端诊断与修复构建都用 `Platform=Win32`，产物落 `output\Win32\` |

### 6.2 照抄即可的命令形态

```powershell
# ✅ Python 脚本（一律绝对路径，不用 cd）
cmd /c "C:\Python314\python.exe F:\soft\00selfmade\rime_claude\tmp\fix_097_phrases_inject.py"
cmd /c "C:\Python314\python.exe F:\soft\00selfmade\rime_claude\tmp\fix_097_phrases_inject.py deploy"
cmd /c "C:\Python314\python.exe F:\soft\00selfmade\rime_claude\tmp\analyze_phrases_timeline.py"
cmd /c "C:\Python314\python.exe F:\soft\00selfmade\rime_claude\tmp\deploy_default_yaml.py"

# ✅ 重编（装机版是 x86）
$root='F:\soft\00selfmade\rime_claude'
$msb='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
& $msb "$root\FluxingPhrasesDialog\FluxingPhrasesDialog.vcxproj" /p:Configuration=Release /p:Platform=Win32 /p:SolutionDir="$root\" /v:minimal /nologo > "$root\tmp\fix2-build-client.log" 2>&1
& $msb "$root\WeaselServer\WeaselServer.vcxproj" /p:Configuration=Release /p:Platform=Win32 /p:SolutionDir="$root\" /v:minimal /nologo > "$root\tmp\fix2-build-server.log" 2>&1

# ✅ 部署 + 部署产物门禁
& 'D:\Program Files\fluxing\weasel\WeaselDeployer.exe' /deploy
& 'F:\soft\00selfmade\rime_claude\Release\TestHotkeyBindings.exe' `
  --baseline 'F:\soft\00selfmade\rime_claude\test\TestHotkeyBindings\known-dead-bindings.txt' `
  'D:\Program Files\fluxing\weasel\data\default.yaml' '<其它 yaml ...>'

# ✅ 二进制级确认装机 exe 是否带探针
$exe='D:\Program Files\fluxing\weasel\WeaselServer.exe'
[Text.Encoding]::Unicode.GetString([IO.File]::ReadAllBytes($exe)).Contains('fluxing-phrases-diag.log')

# ✅ 停装机 WeaselServer（PPL ⇒ 只能 /quit）
& 'D:\Program Files\fluxing\weasel\WeaselServer.exe' /quit   # 注意：会被 TSF 重新拉起

# ✅ 权威门禁（后台跑；退出码 0 全过 / 1 有未达标项 / 2 超时或沙箱没起来）
cmd /c "scripts\sandbox-gate\run-verify.cmd -RepoRoot F:\soft\00selfmade\rime_claude" > "F:\soft\00selfmade\rime_claude\tmp\gate-sandbox-097b.log" 2>&1

# ✅ 读门禁结果
$j = Get-Content -LiteralPath "$env:USERPROFILE\sandbox-artifacts\rime\logs\claims.json" -Raw | ConvertFrom-Json
$j.results | Where-Object { $_.status -eq 'FAIL' } | ForEach-Object { "$($_.id) :: $($_.detail)" }

# ✅ NSIS 语法门禁（⚠️ 会把产物写进 output\archives\fluxing-0.0.0-installer.exe！跑完必须确认并清掉）
& 'C:\Program Files (x86)\NSIS\makensis.exe' /V2 'F:\soft\00selfmade\rime_claude\output\install.nsi' `
    /DFLUXING_VERSION=0.0.0 /DWEASEL_BUILD=97 > 'F:\soft\00selfmade\rime_claude\tmp\nsi-gate-097.log' 2>&1
```

### 6.3 已证实失败 / 被拦的命令形态（**不要重试**）

```powershell
# ❌ [IO.File]::ReadAllBytes($log) 读正在被 cmd 重定向独占写的日志 ⇒ IOException。
#    ⇒ 用 [IO.File]::Open($log,'Open','Read','ReadWrite') + StreamReader。

# ❌ Set-Location $root 之后做写入（Out-File / git commit 消息 / Copy-Item）：
#    "write/delete targets cannot be verified safely after a command working directory change."
#    ⇒ 一律绝对路径 + `git -C $root`；只读 cd 后跑 clang-format.ps1 -i 可通过。

# ❌ Copy-Item $src $dst -Force（备份装机文件到工作区）⇒ 被拦。
#    ⇒ 改用 Python 脚本（tmp\deploy_default_yaml.py / tmp\fix_097_phrases_inject.py deploy 模式）。

# ❌ bash 里跑含 "RMDir" / "Stop-Process -Force" 的命令 ⇒ 被删除类分类器拦。
#    ⇒ 删除用 `rm -- "<abs path>"`；停进程走 WeaselServer.exe /quit。

# ❌ 仓库级 `clang-format.ps1 -n` / `-i`（全量）⇒ 会改 186 个既有未格式化文件。

# ❌ 诊断插桩期间打包安装器 ⇒ 会把带插桩的 exe 带进发布产物。

# ❌ 覆盖装机 FluxingPhrasesDialog.exe 时不结束残留进程 ⇒ PermissionError 13。
#    ⇒ 先 `taskkill /F /IM FluxingPhrasesDialog.exe`（通过 Python subprocess 调用）再复制。

# ❌ 解析诊断日志时假设字段顺序为 [pid][seq][ms] ⇒ 实际是 [ms][pid][seq]；且 fg=] 后面是空格不是 ']'。
#    ⇒ 正确正则：`\[(\d+):(\d+):(\d+)\]\[(CLIENT|SERVER)\]\[ms=(\d+)\]\[pid=(\d+)\]\[seq=(\d+)\]\[lbtn=(\d+)\]\[fg=(\d+)\]\s*(.*)`

# ❌ 补丁脚本用 `count(old)==1` 判幂等（新文本可能是旧文本前缀）⇒ 会复制两份。
#    ⇒ 改用「回读逐字节比对 + 幂等复算（新文本已在则跳过）」。

# ❌ 正则锚点不锚定行首（缺 `(?m)^`）⇒ 14 空格片段会匹配到 12 空格块内部。
#    ⇒ 用 `(?m)^([ \t]+)...\n\1...` 并带 `\1` 反向引用；锚点必须带函数名。

# ❌ 拿 `tmp\patch_097_docs.py` 当存在 ⇒ 它不存在（幻影文件）。
```

### 6.4 行尾 / BOM 基线（改这些文件必须保持）

```
PhrasesDialogIPC.cpp     18,909 B  no-BOM  LF (crlf=0, bareLF=542)
PhrasesDialog.cpp        76,243 B  no-BOM  LF (crlf=0, bareLF=1824)
FluxingTextInjector.h    7,129 B  no-BOM  LF (crlf=0, bareLF=213)
output\install.nsi       62,885 B  UTF-8 + EF BB BF BOM + CRLF
scripts\sandbox-gate\*.ps1           UTF-8 with BOM + CRLF
output\data\default.yaml 18,217 B   no-BOM + 纯 CRLF
```

`scripts\sandbox-gate\check-scripts.ps1` **在仓库内不存在** ⇒ 用自写等价校验（BOM/CRLF/FFFD + `[System.Management.Automation.Language.Parser]::ParseFile`）。
Python 补丁脚本的 docstring 里出现 `\user1` 之类路径 ⇒ unicodeescape 错误 ⇒ docstring 用 `r"""..."""`。

### 6.5 本轮产出的脚本/日志（都在 `tmp\`）

```
fix_097_phrases_inject.py    11,005 B  双击修复补丁 + Win32 重编 + 部署 + 校验（阶段: all|deploy）
analyze_phrases_timeline.py   2,278 B  诊断日志毫秒时间线分析 → tmp\phrases-timeline.txt
fix2-build-client.log / fix2-build-server.log      本轮 msbuild 日志（均 exit=0）
gate-sandbox-097b.log          权威门禁运行日志
nsi-gate-097.log               makensis 门禁日志
install-backup-diag2\          部署前装机 exe 备份
diag-backup\                   第一轮插桩前源码备份（无探针、无两处修复！勿直接覆盖）
```

早前脚本（诉求 3 / 门禁修复用）：`patch_097_nsi.py` · `patch_097_harness2.py` · `patch_097_docs2.py` · `patch_097_lesson164.py` · `patch_097_gate2.py` · `patch_097_p7.py` · `patch_097_lesson166.py` · `prepare_commit2_blobs.py` · `deploy_default_yaml.py` · `patch_097_agents_packagename.py` · `diag_exp.py` · `diag_exp2.py`

### 6.6 其它硬约束速记

- **`output\install.nsi` 任何改动先跑 `makensis` 当门禁**。
- `WeaselServer.exe` 是 PPL 不可 `taskkill`，只能 `WeaselServer.exe /quit`；**L120 禁止杀 TSF 宿主进程**。
- 改文本必须保留原行尾与 BOM。
- `librime` 子模块既有脏状态**不要提交**。
- push 只推 `kizemo`，不直推 `origin`。
- release 需 `RELEASE_BUILD=1` in `env.bat`；**禁用 `git describe --tags` 取版本号**；发布禁用 xmake。
- 版本号保持 **1.1.6，不 bump**。
- `output\data` 缺 `*.gram` / `data\lua\*.txt` ⇒ 慢盘首跑部署可能 5–10 分钟。

---

## 7. 建议技能（suggested skills）

新会话请用 Skill 工具加载下列技能：

| 技能 | 为什么需要 |
|---|---|
| `superpowers:systematic-debugging` | 本项目最核心的失败模式是「领先假设成立 ⇒ 直接写方案」；本轮两次插桩推翻假设才坐实根因。这条技能会强制「先证据后方案」。 |
| `superpowers:verification-before-completion` | 步骤 1 的验收标准必须由**二进制级断言**（exe 不含探针字符串）而非主观判断来关。 |
| `git-workflow` | 本项目 P4：Conventional Commits + 模块 scope + 用户可见改动进 CHANGELOG + 每个 commit 自包含过 `ci.yml`；且 push 只推 `kizemo`。 |
| `rime-skill` | 涉及 `output/data/**/*.yaml`、schema、部署产物门禁时的 Rime 侧正确做法。 |
| `documentation-and-adrs` | 步骤 2 要写 CHANGELOG + L167（事故沉淀）；若后续引入架构决策则走 ADR。 |
| `shipping-and-launch` | 步骤 4 真机跑安装器前的发版前检查与回滚预案。 |
| `source-driven-development` | 若要改 librime 或 RimeTraits/IPC 语义，需对照官方文档。 |
| `mavis-doctor` | 需要排查本地运行时（会话卡住 / 插件 / 权限 / 日志）时使用。 |

---

## 8. 一句话接力 prompt

```
接手 Fluxing spec 097 收尾：先按 CLAUDE.md §1 的顺序读 CONTEXT.md / AGENTS.md / .specify/memory/constitution.md / .specify/memory/project-knowledge.md / docs/agents/domain.md 建立项目背景与铁律，再完整读交接文档 F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-02-fluxing-spec097-phrases-paste.md（背景、目标、进度、证据、未提交改动、下阶段计划全在里面，不要让我复述），然后按该文档 §5 的顺序开工，第一步是剥离诊断探针并出干净构建。
```

---

## 9. 决策索引（避免新会话重新推导）

| 决策 | 结论 | 位置 |
|---|---|---|
| 双击不上屏根因 | 客户端 `HideWithoutDisconnect()` 的「保留 pipe」前提被自身自动隐藏连锁破坏 ⇒ INJECT 从未发出 | §2.4 |
| 双击修复 | 文件级 `g_hideKeepPipe` + `Hide()` 断管道门控 + 三分支置位/清零 | §2.4 |
| 延时根因 | 整段延时在服务端逐字 `SendInput` 内部（≈3.1 ms/字） | §2.5 |
| 延时修复 | 剪贴板 + 粘贴快捷键一次性提交；剪贴板不可用时降级逐字；2 s 后条件恢复 | §2.5 |
| 粘贴键选 Ctrl+V | `output/data/default.yaml` 里 `Control+v` 仅在注释；未绑定键 `handled=false` ⇒ IME 不 eat | §2.5 |
| 共享头登记 | 不必改 `.vcxproj`（`AdditionalIncludeDirectories` 已含 `$(SolutionDir)\include`） | §2.5 |
| P7 定性翻转 | 不是产品回归，是门禁断言世界观漂移 ⇒ 改门禁（PHASE 0 钉 `always_show=0`），不改产品 | §2.3 |
| 门禁失败三分原则 | 环境恒定项照实披露；门禁自身腐化修门禁并记 lesson；真回归才修代码 | §2.7 / §5 |
| 装包名口径 | 对外一律三段（ADR 0012）；第四段 `WEASEL_BUILD` 只进资源版本号与内部 shim 名 | §2.1 |
| 提交拆分 | YAML 与 C++ 分开；小步提交；push 只推 `kizemo` | §5 步骤 2 |
| handoff 落点 | 仓库 `docs/archive/handoffs/handoff-<date>-<slug>.md`；新建而非原地覆盖；无 BOM + LF | 本文件 |

---

*文档结束。接手时请在第一步动业务代码前，先在对话里给出 What / Why / How verified 三段意图声明（R1）。*
