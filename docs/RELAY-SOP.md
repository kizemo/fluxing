# 无人值守接力链 SOP（路径 A · session_create 自接力）

建立时间：2026-11-24 · 适用面：MiniMax Code 桌面端本机（用户级能力，跨项目通用）
配套文件：
- 状态机：`maintenance\relay\relay.json`
- Prompt 模板：`maintenance\relay\relay-prompt.md`（A 启动 / B 接力 / C 收链）
- 交接卡（既有）：`maintenance\task-state\LATEST.md`，由用户级 `Stop` Hook 每轮自动刷新

## 一、支点与边界

链条能自转**只依赖一件事**：每一环会话在自己的 `content` 里被写入了
「阶段收尾第 3 步必须调用 `session_create` 投递下一环」这条硬义务。
漏写 = 链断。产品侧没有「会话结束自动开新会话」的原生开关，故用 Agent 自接力替代。

不适用：需要图形界面人工验收、涉及提交/发布/删数据/付费等不可逆动作的阶段 —— 这些必须停在 `blocked`。

## 二、一条链的时序

1. 人工：写好 `plan-*.md`（阶段拆分 + 每阶段 acceptance 判据），把 `relay-prompt.md` 的 A 模板粘进一个会话。
2. 第 1 环：建/校 `relay.json` → 做阶段 1 → 结论落盘仓库 `docs/` → 回写 `relay.json` → `session_create` 投第 2 环。
3. 第 2..N 环：读 `relay.json` 为唯一进度真相 → 重复第 2 步（模板 B）。
4. 终止：全部完成写 `status=done`；遇停止条件写 `status=blocked` + 缺什么，并停下等人。

## 三、relay.json 状态机

| 字段 | 含义 | 写入方 |
| --- | --- | --- |
| `status` | `idle` / `running` / `blocked` / `done` | 每环收尾时 |
| `goal` / `plan_doc` | 最终目标一句话 / 计划文档绝对路径 | 第 1 环 |
| `workspace_dir` | 仓库绝对路径，`session_create` 每环沿用 | 第 1 环 |
| `stages[]` | `id` / `title` / `deliverable` / `acceptance` / `status` | 第 1 环建，逐环改 status |
| `current_stage` | 正在推进的阶段号（**接力前 +1**） | 每环 |
| `max_stages` | 硬上限，达到即停 | 第 1 环 |
| `auto_relay` | false 即刻停链（人工急停开关） | 人工 / 每环 |
| `sessions[]` | 链上全部会话 ID，便于回看与归档 | 每环追加自身 ID |
| `stop_conditions[]` | 判据清单，模板与状态机共用口径 | 第 1 环 |

一致性要求：`current_stage` 必须与 `stages[].status` 对得上；每环收尾要 `Test-Path` 回读自己写的文档路径，
接力 prompt 里的每个路径指针必须可解析（少后缀 = 真缺陷，当场修）。

## 四、安全阀（务必保留）

- `max_stages` 与 `auto_relay=false` 双闸门；急停只需把 `auto_relay` 改 false，下一环读到即停。
- **本轮结束前不得留未终态后台任务**，否则任务终态会唤醒属主会话、叠加接力造成额度失控。
- 成本口径：每环是一个完整新会话。按实测根会话约 89K cache_read/次、子会话约 47K/次估：
  12 阶段链 ≈ 数百次模型调用，跑长链前先确认额度余量。
- 每阶段工具调用预算 40 次内不收敛就停下写「未完成」，禁止用接力掩盖失败。
- 涉及提交/发布/删除/付费：停 `blocked` 等人工，不得自接力（模板已写死）。
- 不往 `relay.json` 与任何落盘文件写密钥、令牌、口令。

## 五、可选强化：Stop Hook 接力门禁（默认不启用）

若发现某一环「说要做接力却没调 `session_create`」，可加一个 `Stop` handler：
读 `relay.json`，当 `status=running` 且 `auto_relay=true` 且本阶段已 `done` 但 `sessions[]` 未新增 →
`exit 2` + stderr 写「立即按 relay-prompt.md 模板 B 调 session_create 完成接力，不得结束本轮」。
输入字段 `stop_hook_active` 为 `true` 时必须 `exit 0`，否则一轮最多被额外续跑 8 次。

启用位置：用户级 `hooks.json` 的 `hooks.Stop` 数组（顶层需 `"enabled": true`，Windows 用 `commandWindows`）；
改完从**下一轮**生效。该文件属你的线上配置，我不擅自改 —— 需要你明确说「加门禁」再动。

## 六、实测记录

### 6.1 已实测通过（2026-11-24，本机 Windows / MCode 桌面端）

**一跳**：`session_create(agent_name="me", title="relay-smoke-A")` → 返回 `ok=true`，
`turn.mode="activated"`（首条消息即开跑，**无需人工进新会话点发送**）。会话 `mvs_63e2bfe98c3e42768a7b127e1812c424`
于 20:34:40 收到指令、20:34:59 产出，回复 `RELAY-SMOKE-OK|mvs_63e2bfe…|Tue Nov 24 2026 20:34 GMT+0800`
—— 身份与时间均取自它自己的 `<system-reminder>`，说明**链上每环能自报 session id**，可观测性成立。耗时约 19s。

**三跳**：`mvs_1711739d…`(我) → `mvs_63e2bfe9…` → `mvs_f183cc0bb03745efbfb7af67f6201c51` → `mvs_37cdd2b755b54889bcd26f5d642e18f3`。
第二跳会话自行调用 `session_create` 建出第三跳，末梢回 `RELAY-HOP3-ALIVE|mvs_37cdd2b7…`（20:36:27 → 20:36:37，约 10s/环）。
**结论：无人干预下自动开新会话 + 投递接力 Prompt + 逐环续建，链路成立。**

调用形状：`mcp_invoke(tool_name="session_create", arguments={content, agent_name, title})`。
注意 `mavis` 工具的 `session create` 子命令**不存在**，会报 `SESSION_CREATE_MOVED`，必须走 tool_search + mcp_invoke。

### 6.2 实测暴露的三个硬约束（不遵守则链必断）

1. **工作区不会继承项目目录。** `location.mode="current"` 且未传 `workspace_dir` 时，新会话落在
   `…\.minimax\sessions\<sid>\workspace` 私有临时目录，**看不到仓库文件**。
   ⇒ 接力 prompt 里 `session_create` **必须显式传 `workspace_dir`（或 `location.project_id`）**，
   且 relay.json 的 `workspace_dir` 字段必须预先填好真实项目路径。这是本次落地最重要的发现。
2. **子会话会把结果回传给创建者，并唤醒创建者。** 每次回传都让上一环（若已 idle）再跑一轮 agent 循环。
   冒烟中三个子会话各调用了一次 `mavis: session send` 回传 —— 这是子会话的**结果投递契约**，
   不是额外任务动作，无法靠 prompt 禁止。
   ⇒ 成本按「每环 = 新会话一轮 + 父会话被唤醒一轮」估；接力 prompt 必须写死
   「被下游回执唤醒时，只输出一行进度摘要即结束，不得调工具、不得再建会话」。
3. **交接卡 LATEST.md 是 agent 级共享文件，会被任意会话覆写。** 冒烟期间本任务的卡片被 `relay-smoke-A` 的
   Stop Hook 直接覆盖（卡片头会话 ID 变成 `mvs_63e2bfe9…`）。
   ⇒ `relay.json` 与项目自己的 `docs/` 才是进度真相；`handoff_card` 只当提示，不可当依据。

### 6.3 仍未验证

- 链上会话在桌面端会话列表中的可见性/可接管体验（仅确认 API 层可查、可读消息）。
- 长链（>3 环）在真实项目里的累计成本与漂移率；带真实代码改动的接力（本轮只测了信令）。
- 路径 B（Stop Hook + `mcode exec`）：本机 `mcode` 不在 PATH，未安装、未测试。
- `/docs/cli/sdk.md`（TS / Python SDK 作编排器）：官方页抓取失败，仅知存在。
