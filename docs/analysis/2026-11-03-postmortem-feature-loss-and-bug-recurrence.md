# 复盘：为什么「已开发好的功能」会丢、已修的 bug 会复现

> 日期：2026-11-03 ｜ 分支 `Fluxing` ｜ 触发事件：1.1.8 安装报错（`无法打开要写入的文件: D:\Program Files\fluxing\weasel\FluxingPhrasesDialog.exe`）
> 状态：**分析已落盘，机制未落地**。落地清单见 §5；落地需先写 ADR 0033（MADR）。

---

## 0. 一句话结论

本项目的「功能回退 / bug 复现」**不是代码质量问题，而是验收资产没有被当成产品资产来管理**：

- 验收脚本住在 git-ignored 的 `tmp/` ⇒ 下一会话拿不到 ⇒ 每会话重写探针 ⇒ 探针质量不可控、假阳性与副作用重复发生；
- 验收只在「干净环境」做过 ⇒ 真实用户环境（长期使用、残留进程、旧版本混杂）这一维度**从未被覆盖**；
- 已修缺陷没有沉淀成可执行断言 ⇒ 同类问题没有「复发闸门」；
- 验证靠人肉脚本序列，没有一键门禁 ⇒ 漏一步就漏验，且漏了没人知道。

修法不是「以后更小心」，而是**把「怎么证明它还对」变成和代码一样入库、受 CI 约束的一等资产**。

---

## 1. 本轮的两个真实案例

### 案例 A：1.1.8 安装报错 —— 「功能被环境吃掉」

| 环节 | 取证结论 | 证据性质 |
|---|---|---|
| 症状 | 交互安装报「无法打开要写入的文件：`…\weasel\FluxingPhrasesDialog.exe`」 | 用户报告 |
| 直接原因 | 目标 exe 被**独占锁**（`File.Open(ReadWrite, None)` 抛 being used by another process） | 已验证（独占打开探针） |
| 占用源 | **20 个** `FluxingPhrasesDialog.exe` 残留进程，窗口标题全空 ⇒ 隐藏孤儿，合计约 350 MB WS | 已验证（进程枚举 + WS 求和） |
| 为何成孤儿 | 19 个的**父进程早已消失**（`<gone>`，对应历次探针启动又被强杀的 server pid）；仅 1 个父 `WeaselServer.exe` 仍活 | 已验证（`Win32_Process.ParentProcessId` 比对） |
| 为何父会死 | server 被 `taskkill /F /IM WeaselServer.exe /T` 强杀 ⇒ 子进程不被回收 | 已验证（安装器与探针都这么干） |
| 为何产品留残留 | 短语窗口按 Esc 只 `Hide()`、**不退出进程**（L167 设计）；`PhrasesDialogIPC::TerminatePhrasesDialog()` 只在 server **优雅**退出时调用，server 被强杀 ⇒ 清理代码根本不执行 | 已验证（`PhrasesDialog.cpp:967` → `Hide()`；`PhrasesDialogIPC.cpp:112` 在 worker 线程清理路径上） |
| 为何安装器没兜住 | `output/install.nsi` 的 kill 覆盖面**只有 `WeaselServer.exe`**（`.onInit` 3 次 / `call_uninstaller` / install 段 / uninstall 段），**从未覆盖 `FluxingPhrasesDialog.exe`** | 已验证（全文检索） |
| 为何 `taskkill /T` 救不了 | `/T` 只沿**活着的**子进程树走；父已死的孤儿不在任何进程树里 | 已验证（行为解释 + 孤儿父 pid 全 `<gone>`） |
| 为何之前的「全绿验收」没挡住 | 验收脚本每次都从**干净环境**起步，且验收矩阵里**没有「残留进程 / 孤儿进程」这一维** | 已验证（验收脚本无任何残留清理前置条件） |
| **静默模式把失败吞掉了** | 同一场景下旧 1.1.8 安装器**退出码仍是 0**、注册表 `DisplayVersion` 还写到了尾，但**目标文件根本没被重写**（旧二进制留存）⇒ NSIS `File` 失败在静默模式下不反映在退出码上 | 已验证（A/B v2 实测：`rewrote_target=False` + `reached_install_tail=True` + `INSTALL_EXIT=0`） |

**归类**：这不是「回归」，是**边界从未被覆盖**。系统被设计成「假设 install 期间没有自家 UI 在跑」，而这个假设在真实机上第一天就破产。

> **这条更狠**：静默安装失败**不报错、还继续把安装段跑完** ⇒ 用户以为装成功了，机器上却是**旧二进制**。这就是「功能丢失」最隐蔽的一种形态：**静默降级**。⇒ 安装验收**不能只看退出码**，必须比对「安装后的文件内容/哈希 == 构建产物」+「注册表写到了尾」。本轮 A/B 的判据就是这么改的（退出码判据会给出「旧安装器也成功 ⇒ 复现无牙」的错误结论）。

> 顺带坐实一条旧假设已破产：`output/install.nsi` 原文注释写「不是 PPL 进程 → 走简单 File（无 L72-fix Rename-then-File 兜底）。**dialog 是短时 modal，install 期间被锁的概率极低**」。本例证明这个假设是错的 ⇒ 「文件锁」类问题在本项目是**常态路径，不是边缘情况**。

### 案例 B：探针 v2 → v3 —— 「bug 复现」其实是**假阳性**

- v2 探针在「瞬时态」上报红，差点击败修复；
- 复核发现：Esc 落点在 `show+3600ms`，而该态的**既有自动隐藏阈值约 3500ms** ⇒ 红是「窗口本来就该隐藏」被误读成「Esc 导致隐藏」；
- 修法不是改产品，而是把探针改成**三阶段归因**（A 自动隐藏对照 / B Esc 归因窗 / C 反向对照）并加「keeper」身份排除干扰。

**归类**：探针本身会腐化，而**没有对探针的回归**。一个假阳性探针一旦写进交接卡，下一会话会信它 ⇒ 「bug 复现」的错觉会被继承。

### 案例 C：功能「丢失」的三条已知通道

| 通道 | 机制 | 本轮/历史证据 |
|---|---|---|
| **静默降级** | 「新二进制没落盘、旧二进制留存」类问题不会报错，只是升级后行为还是旧的 | `install.nsi` 的 L72-fix Rename-then-File 与 stage2 兜底机制本身就是为它写的；`WeaselServer.exe.stage2` 标记文件机制 |
| **验收资产丢失** | 探针与验收脚本住在 `tmp/`（git-ignored）⇒ 跨会话不可继承 ⇒ 每会话重写 ⇒ 质量不可控 | `tmp/` 在 `.gitignore` 内；本会话的探针 v3、验收脚本全是本会话新建 |
| **假设腐化** | 代码里写死的假设（如「dialog 是短时 modal」）没人定期复核 ⇒ 假设失效时行为静默错误 | `install.nsi` 的那行注释（本轮才被证伪） |

---

## 2. 根因清单（8 条）

| 编号 | 根因 | 判定依据 |
|---|---|---|
| RC1 | **验收资产不在版本控制内** —— `tmp/` 被 ignore，探针/验收脚本不可跨会话继承 | 已验证（`.gitignore` + 仓库现状） |
| RC2 | **验收矩阵缺边界维度** —— 只在干净环境做；残留/孤儿/多会话/DPI/升级路径覆盖不全 | 已验证（案例 A） |
| RC3 | **探针无自证契约** —— 没有「有牙自检」（旧构建必须被抓出）与「反向对照」（反向口径不得破坏） | 已验证（案例 B） |
| RC4 | **探针有副作用且无清理** —— 打开短语窗口后不关，留下 20 个孤儿；探针反过来制造了新故障 | 已验证（案例 A 的孤儿来源） |
| RC5 | **缺陷 → 回归用例无映射** —— 修完即忘，没有「这条缺陷由哪条断言守住」的索引 | 已验证（仓库无此索引） |
| RC6 | **无一键门禁** —— build → 单测 → 变异 → 探针 → 装机 全靠人肉脚本序列，漏一步就漏验 | 已验证（本会话全部手搓命令） |
| RC7 | **版本号不入库** —— `env.bat` git-ignored ⇒ 换机/换会话重建不可复现 ⇒ 极易产出「同版本不同内容」的安装器（L107 事故） | 已验证（`git check-ignore`） |
| RC8 | **清理覆盖面靠手工维护** —— 安装器只杀「已知的映像名」，与真实进程树脱节 ⇒ 新增自家 UI 进程必然漏 | 已验证（案例 A 的 install.nsi 缺陷） |

> RC1–RC8 是**机制层**结论，不是「谁没注意」的指责。每一条都能给出可执行的反制动作（见 §3）。

---

## 3. 预防机制：七根支柱

> 落地时按 CLAUDE.md §3 分流：涉及构建流水线的改动 → 写 `docs/adr/0033-*.md`（MADR）。

### P1 · 回归用例入库（把 `tmp/` 升级为 `test/regression/`）

- **目标形态**：`test/regression/<case-id>/` 或 `test/regression/<case-id>.ps1`，每个用例带：
  - `case-id` 与人类可读标题；
  - **前置条件声明**（依赖哪些进程/注册表/文件状态）；
  - **cleanup 钩子**（`finally` 块必须杀自己拉起的进程、还原自己改过的注册表/文件）；
  - **退出码即结论**（0 = 通过；非 0 = 失败；不依赖人眼看日志）。
- **硬约束**：**任何验收脚本不得只存在于 `tmp/`**。`tmp/` 只能放一次性诊断产物，不能放验收资产。
- **验收判据**：随机抽一条用例，**在干净 clone 上只跑这一条**，能独立给出结论。
- **落点**：`test/regression/`（新目录）+ `scripts/gate/run-all.cmd` 引用。

### P2 · 缺陷 → 回归用例映射索引

- **目标形态**：`test/regression/DEFECTS.yaml`（或 `docs/regression/defect-index.md`），每条已修缺陷登记：
  `defect-id | 一句话症状 | 根因类别 | 触发条件 | 对应用例 case-id | 门禁阶段 | 首次引入版本 | 修复版本`。
- **硬约束**：**新增缺陷时必须同时新增一条用例并登记映射**，否则不予合并（写进 commit checklist）。
- **验收判据**：随机抽一条历史缺陷，能在索引里找到对应 case-id，且该用例今天仍能跑（可跳过但不能缺失）。
- **落点**：`test/regression/DEFECTS.yaml` + `.claude/rules/commit-checklist.md` 增加一条。

### P3 · 一键门禁脚本（分阶段、可增量）

`scripts/gate/run-all.cmd` 的阶段划分（每阶段独立退出码，便于本地只跑一段）：

| 阶段 | 内容 | 判据 |
|---|---|---|
| L0 静态 | 编码 / BOM / CRLF / `install.nsi` 锚点唯一性 / `.ps1` 语法解析 | 0 错 |
| L1 构建 | `build.bat weasel` | 0 错误 |
| L2 单测 | `scripts/test-infra/run-test-suite.bat`（含 `TestWeaselIPC` 真值表） | 全绿 |
| L3 变异 | 对关键判定取反 ⇒ 必须翻红；还原 ⇒ 必须复绿 | 有牙 |
| L4 回归 | P1 的入库用例（自带清理） | 全绿且**无副作用残留** |
| L5 装机 | `scripts/sandbox-gate/run-verify.cmd`（权威沙箱门禁） | 退出码 0 |
| L6 打包 | `build.bat weasel installer` | 0 错误 |

- **硬约束**：**打包（L6）必须先过 L0–L5**。CI 与本地跑的是**同一份**脚本（本地是「本地那台 CI」）。
- **验收判据**：故意破坏一处判定（例如把 `IsOwnUiClient` 的 `==` 改成 `!=`），门禁必须红；还原后必须绿。

### P4 · CI 化

- GitHub Actions 跑 L0–L4（无 GUI 依赖的部分）；L5 沙箱门禁放 nightly（它重、慢、会起虚拟机）。
- **硬约束**：**CI 红灯 ⇒ 不得发版**；发版前必须有一次绿的 CI run。
- **注意**：`env.bat` 的版本号不入库 ⇒ CI 产物版本号需由 CI 自己注入（与 RC7 一并解决）。

### P5 · 打包前强制门禁

- `build.bat weasel installer` 的入口先调用 `scripts/gate/run-all.cmd --up-to L5`；或由发布脚本统一编排。
- **硬约束**：**手工直接跑 makensis 打出来的安装器不得发布**（会绕过 L09/门禁）。
- **配套**：安装器落盘时写一份 `manifest`（门禁各阶段结论 + 产物 sha256 + 代码 commit），发布时附在 Release 上 ⇒ 事后可追溯「这个 exe 是哪次门禁放行的」。

### P6 · 探针契约（写进 `docs/agents/domain.md` 或 `.claude/rules/`）

任何验收探针必须同时满足：

1. **有牙**：在「已知有缺陷的旧构建」上必须失败；
2. **反向对照**：与被测口径相反的方向必须仍成立；
3. **自清理**：`finally` 中终止自己拉起的进程、还原环境；**跑完后进程数/WS 必须回到基线**；
4. **归因隔离**：若被测行为与其它既有行为共享时间轴，必须有对照阶段能区分二者。

> 案例 B 的教训：**探针变红先问「是不是探针错了」**，再问「产品错没错」。顺序反了会白改产品。

### P7 · 安装前环境清场（覆盖面不再靠手工名单）

- **目标形态**：`install.nsi` 的清理覆盖面由「手工枚举映像名」升级为「**枚举 `$INSTDIR` 下全部自有 exe 产物**」或至少维护**一张与源码同源的清单**（避免源码改了名字、安装器没改）。
- **本次已落地**：4 处补杀 `FluxingPhrasesDialog.exe`（按映像名，不带 `/T`，能打到父进程已死的孤儿）。
- **配套**：`.onInit` 把「残留进程清单」（映像名 + pid + 父 pid）写进安装日志 ⇒ 装机失败时可直接从日志读出谁在占锁，不必再手工 `Get-CimInstance` 排查。
- **产品侧待办**：短语窗口在 **pipe 断开（server 死）时自行退出**；server 启动时**回收上一代遗留的自家 UI 进程**。两者都能从源头掐断孤儿。

---

## 4. 固定验收矩阵（每个发布至少覆盖这几维）

| 维度 | 含义 | 本轮状态 |
|---|---|---|
| 干净安装 | 无任何残留 | 已验（1.1.8 静默装机） |
| 升级路径 | 从已装版本覆盖安装 | 已验（1.1.8 升级装机） |
| **残留进程** | 安装时自家 UI 正在运行 | **本轮补齐（1.1.9）** |
| **孤儿进程** | 安装时自家 UI 的父进程已死 | **本轮补齐（1.1.9）** |
| 多登录会话 | 第二会话里 server / UI 的隔离 | 未验 |
| 96 DPI / 跨屏动态 DPI | 界面几何 | 未验 |
| 旧版残留目录 | 嵌套目录 / `.old` / `.bak` | 部分（受保护路径需人工） |

---

## 5. 立即可执行清单

**A. 本轮已完成（1.1.9）**

1. `output/install.nsi` 补杀自家 UI 进程（4 处：`.onInit` / `call_uninstaller` / install 段 / uninstall 段）；
2. 归一化 46 处裸 LF ⇒ 恢复 L09 的 100% CRLF；
3. 版本抬到 1.1.9（同版本不同内容 = L107 事故）；
4. 孤儿锁场景复现 ⇒ 新安装器安装成功（见交接卡 §2.3）。

**B. 下一会话必须做（按优先级）**

1. **P1 + P2**：把本会话的探针 v3、验收脚本、变异脚本**从 `tmp/` 提升为 `test/regression/`**，并建 `DEFECTS.yaml` 映射（先录 ADR 0032 与本次安装锁两条）；
2. **P3**：`scripts/gate/run-all.cmd`（先做 L0/L2/L3，探针入库后再做 L4）；
3. **P7 产品侧**：短语窗口在 pipe 断开时自退；server 启动时回收上一代自家 UI 残留；
4. **P7 配套**：`.onInit` 落残留进程清单日志；
5. **P4**：接 GitHub Actions 跑 L0–L4；
6. **RC7 彻底解**：版本号从 `env.bat` 移到入库的版本文件（避免换机重建不可复现）——需单独 ADR。

**C. 明确不做**

- 不再把「SANDBOX PASS」当验收通过（L100：4 项 sandbox PASS ≠ ship 成功）；
- 不再把验收脚本只放 `tmp/`；
- 不再让探针在结束时留下后台进程；
- 不再在没有「有牙自检」的情况下相信一个报红的探针。

---

## 6. 证据锚点（完整路径）

- 本轮安装阻塞诊断与处置：本会话 runtime 取证（20 进程 / 父 pid `<gone>` / 独占打开探针）；台账 `F:\soft\00selfmade\rime_claude\tmp\release-118-status.md`
- 探针三阶段归因分析：`F:\soft\00selfmade\rime_claude\tmp\evidence-0032v3\ANALYSIS.md`
- 安装器补丁脚本（字节级 + 锚点断言 + 幂等）：`F:\soft\00selfmade\rime_claude\tmp\patch-install-nsi-ownui-kill.py`
- 短语窗口 Esc 路径：`F:\soft\00selfmade\rime_claude\PhrasesDialog.cpp:967`
- 子进程回收调用点：`F:\soft\00selfmade\rime_claude\WeaselServer\PhrasesDialogIPC.cpp:112` / `:334`
- 自家 UI 名字常量（单一真相源）：`F:\soft\00selfmade\rime_claude\include\WeaselIPC.h:248`
- 安装器 kill 覆盖面：`F:\soft\00selfmade\rime_claude\output\install.nsi`（`.onInit` / `call_uninstaller` / install 段 / uninstall 段）
- 权威沙箱门禁：`F:\soft\00selfmade\rime_claude\scripts\sandbox-gate\run-verify.cmd`
- 单测套件：`F:\soft\00selfmade\rime_claude\scripts\test-infra\run-test-suite.bat`
