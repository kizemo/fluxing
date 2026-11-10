# 100 · 下一阶段执行序（2026-11-10 版）

> 上游三件套（2026-11-07 已落盘，**本文件不重复其内容**）：
> - `.specify/specs/100-fluxing-cloud-account-sync/spec.md`（用户视角，11,192 B）
> - `.specify/specs/100-fluxing-cloud-account-sync/plan.md`（选型 / 协议 / 算法 / 变更面，23,324 B）
> - `.specify/specs/100-fluxing-cloud-account-sync/tasks.md`（7 阶段 30 条任务 T101–T604，22,645 B）
> - `docs/adr/0039-cloud-account-sync-architecture.md`（架构决策）
>
> 本文件只解决 11-07 之后**新出现的三件事**：① 计划漂移（仓库已变，部分任务前提失效）；② 首个可开工批次（批次 X）的精确范围；③ 阶段门禁与不做清单。
>
> 状态：**阶段 A 可立刻开工**；阶段 E（账号页）被 §5 的 D1 决策阻塞。

---

## 0. 结论先行

| 结论 | 说明 |
|---|---|
| 可以立刻开工 | T101 / T102 / T103（纯取证，不写产品代码）+ T104（只加测试工程） |
| 唯一阻塞 UI 的决策 | **D1 账号页入口**（见 §5）—— 只阻塞阶段 E，不阻塞 A/B/C/D |
| 绝对不能提前开工 | **T503 合并实现** —— 硬前置 T102；T102 结论未落盘前不得凭推测写词频合并 |
| 首版最大风险 | 不是加密也不是服务端，而是**词库按条目合并的语义未知**（plan.md 风险 R1） |

---

## 1. 计划漂移：11-07 之后仓库已变（必须先承认）

| # | 漂移 | 影响 | 处置 |
|---|---|---|---|
| **D1** | `tasks.md` T401/T405 写「快捷面板**第 4 槽**升级为账号页」；但**批次 U（2026-11-09）已按用户指示把第 4 槽「用户登录(关于·账号说明)」摘掉**，面板现为 4 键帽且无账号位 | 账号页**没有任何 UI 入口** | **需用户决策**，见 §5 |
| **D2** | `tasks.md` T403 要改 `AboutDialog` 旧文案「本输入法不含云端账号。词库、配置与用户数据全部保存在本机，不上传、不同步。」；批次 W（2026-11-10，`f425526`）移除了语言栏项 ⇒ 其右键菜单四项一并失去入口，**该对话框可能已无用户可达路径** | 改文案收益为零；反过来「关于」信息也失去了展示面 | 与 D1 合并决策：入口定在哪，「关于 + 账号」就挂在哪 |
| **D3** | 批次 W 后快捷设置栏**只剩 `Alt+,`** 一个入口（`WeaselServer/WeaselServerImpl.cpp:81`） | 账号页若挂在面板内，用户发现成本高 | 在 D1 决策里一并交代；首版至少保证 `Alt+,` → 面板 → 账号区 |
| **D4** | `tasks.md` T602 引用 ADR 0030 三数字（进程 1 / 私有内存 < 40 MB / 空闲 CPU < 0.1%）；批次 V 已把面板改成分层窗口（`UpdateLayeredWindow`），基线需重测 | 拿 10 月数字当基线会误判破线 | T602 按**当前 HEAD** 重测，不引用历史数字 |

> 漂移不是 spec 写错了 —— 11-07 定稿时批次 U/W 还没发生。**处置方式是「改入口」而不是「改需求」**。

---

## 2. 首个批次（批次 X）：取证与底座

**目标**：把三条取证结论落盘 + 测试工程能编译。**不产生任何用户可见行为**，因此不涉及入口决策（D1 不阻塞它）。

| 任务 | 产出文件 | 预估 | 依赖 |
|---|---|---|---|
| T102 词库/短语合并语义 **（最高优先）** | `docs/research/fluxing-cloud-2026-11/userdict-merge-semantics.md` + `tmp/` 取证脚本（不入库） | 4 h（可拆两次会话） | 无 |
| T101 邮箱验证码通道 | `docs/research/fluxing-cloud-2026-11/email-verify-channel.md` | 2 h | 无 |
| T103 端点可达性 | `docs/research/fluxing-cloud-2026-11/endpoint-reachability.md` | 2 h | 无 |
| T104 测试工程骨架 | `test/TestFluxingCloud/TestFluxingCloud.{cpp,vcxproj}` + `xmake.lua` | 2 h | 无 |

**出口判据（可复跑）**：
1. 三份 md 都存在，且各含「实测输出片段路径」（不接受「我认为应该」）；
2. `test/TestFluxingCloud` 编译通过 + 冒烟用例通过，输出口径与 `tmp\build-qpanel-tests.cmd` 一致；
3. `winhttp.lib` / `bcrypt.lib` / `advapi32.lib` 已接进构建链。

### 2.1 T102 的做法要点（防翻车，写死在这里）

- **必须用隔离的用户数据目录**（`RIME_USER_DATA_DIR` 指向 `tmp/` 下的副本），**不要动真机 `%APPDATA%\Rime`** —— 动真机数据会污染用户词库且不可逆。
- `RimeLeversApi::{export,import}_user_dict` **必须被 `StartMaintenance` / `EndMaintenance` 包裹**，否则 leveldb LOCK 失败（AGENTS.md 危险区已有此条）。
- 要产出**三条可复跑结论**：① 导出字段集（是否含词频 / 时间戳 / 编码）；② 同名词条导入覆盖规则（取较大 / 取后者 / 相加）；③ 导入期间锁与并发行为。
- **不写生产代码**，只写取证结论 + 脚本。

### 2.2 T101 的边界

只做「通道选型 + 每千封成本 + 首版是否强制验证 + `pending` 态降级行为」四件事。**不在本阶段申请任何密钥、不接任何外部账号**（避免提前引入付费与合规审查）。

---

## 3. 后续批次与硬门禁

| 批次 | 阶段 | 内容 | 开工门禁 |
|---|---|---|---|
| X | A | T101–T104 取证与底座 | 无 |
| Y | B | T201–T204 契约与纯逻辑（信封 / 合并 / HLC）+ `include/FluxingCloudEnvelope.h` | T104 |
| Z | C | T211–T215 Cloudflare Worker + D1 + R2 | T211 只依赖 T104，**可与 Y 并行**；T212 需 T101 |
| AA | D | T301–T305 客户端运行时（CNG / WinHTTP / 凭据 / 账户 / 词库 IO） | T204；**T305 需 T102**；T301 需 T102（口令强度下限） |
| AB | E | T401–T405 账号页与文案 | **D1/D2 决策** + T304 |
| AC | F | T501–T505 同步引擎与调度 | T502 需 T305+T213；**T503 需 T102 落盘** |
| AD | G | T601–T604 双机验收与发版 | E / F 全绿 |

**并行建议**：X 与 Y 串行；Z 与 Y 并行（一个写 TS、一个写 C++，冲突面为零）；AA 必须在 Y 之后（要 include 生产头）。

---

## 4. 明确不做（首版边界，避免反复讨论）

1. **不做**「同步自造词 / 短语 / 配置」之外的任何数据出向；未登录时出向请求数必须 = 0（AC-12 / R9）。
2. **不做**服务端解密：服务端只见密文，`enc_key` 永不出本机（ADR 0039）。
3. **不做** `WeaselDeployer` 账号入口 —— 会触发 IPC 四边同步（`include/WeaselIPCData.h` 变更 + 三端 + ADR + `test/TestWeaselIPC`），首版否决（T402 已记录）。
4. **不做**恢复 `WeaselServer` 托盘图标作为账号入口（`output/data/weasel.yaml:81 display_tray_icon: false` 是用户主动关的）。
5. **不做** Web 端管理后台；设备管理只在桌面端。
6. **不做**「口令找回」—— 口令派生密钥，遗忘即云端数据不可恢复，这条要在文案里明写（T403）。

---

## 5. 待用户决策（只阻塞阶段 E）

### D1：账号页的用户入口放在哪里？

| 选项 | 做法 | 代价 | 评价 |
|---|---|---|---|
| **A（推荐）** | 快捷面板第 4 槽重新成为「账号」入口 | 与批次 U「摘掉第 4 槽」的指令相反，需用户明确改口 | **唯一不新增 IPC、不改用户配置、且面板设计里本来就有的位置** |
| B | `WeaselServer` 托盘右键菜单加「账号…」 | 违反 `display_tray_icon: false`（用户主动关的） | 否决 |
| C | 部署器加账号入口 | 触发 IPC 四边同步 + ADR + 测试 | 首版否决（T402） |

**默认倾向 A**；若用户不同意，需要给出替代入口，否则阶段 E 无法开工（可以先做完 A/B/C/D 全部后端能力）。

### D2：`AboutDialog` 归到哪里？

与 D1 同源：入口定在哪，「关于」分区就挂在哪，然后才执行 T403 的文案改写（旧文案与新功能自相矛盾，必须改）。

---

## 6. 指针

- spec 100 三件套：`.specify/specs/100-fluxing-cloud-account-sync/`
- ADR 0039：`docs/adr/0039-cloud-account-sync-architecture.md`
- 上一轮规划落地 handoff：`docs/archive/handoffs/handoff-2026-11-07-fluxing-cloud-sync-plan.md`
- 批次 W（本期并行收尾，移除语言栏项导致入口收敛）：`docs/archive/handoffs/handoff-2026-11-10-batch-w-langbar-item-removed.md`
- 待建取证目录：`docs/research/fluxing-cloud-2026-11/`（6 份文件，目前**均不存在**）