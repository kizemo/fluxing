# Plan 098 — 云候选（联网补全候选词）

- **Spec**: [spec.md](spec.md)
- **ADR**: [0030](../../../docs/adr/0030-continue-fluxing-fork-no-rewrite-perf-baseline.md)（决策 2 性能基线、决策 4 网络边界）·
  [0028](../../../docs/adr/0028-thin-tsf-shell-and-additive-ipc.md)（决策 1 薄壳白名单、决策 2 协议只增）·
  [0023](../../../docs/adr/0023-shift-page-local-select-and-server-side-candidate-ignore.md)（决策 4 候选唯一点）
- **研究依据**: `docs/research/sogou-reverse-2026-11/REPORT.md` §7（云候选对应第三方"云输入"能力；HTTP 拉取版可先行）
- **Lessons**: —（立项阶段；实现期若踩坑按 `lessons-learned.md` 补 L##）
- **Status**: Draft，未开工

---

## Constitution Check

逐条对照 `.specify/memory/constitution.md` 的 Hard Rules R1–R9 与 Principles P1–P8。

### Hard Rules

| 规则 | 本 spec 的落法 | 状态 |
| --- | --- | --- |
| R1 改代码前先声明 What / Why / How verified | 每个 task 的「输入文件 / 产出 / Done」即三段声明；M0 整组任务是先取证再动手 | ✅（计划层） |
| R2 spec.md 不出现技术栈词 | `spec.md` 用「常驻服务 / 前端 / 出货配置 / 进程间协议」等中性表述；实现语言与库名只出现在本文件与 tasks.md | ✅ |
| R3 每个 task 单一优先级 + `[US#]` 标签 | `tasks.md` 每条都带 `P1/P2/P3` 与 `[US1]…[US8]` | ✅ |
| R4 plan.md 有 `## Constitution Check` 覆盖全部 principles | 本节两张表 | ✅ |
| R5 单 task ≤4h、1–3 文件、有明确输入输出 | 每条列输入与产出；唯一例外是 T1.3（IPC 四边同步），豁免理由见「提交拆分」C5 | ⚠️ 一处豁免 |
| R6 「Done」必须贴测试输出或写明手工验收步骤 | `tasks.md` 每条 Done 给了具体命令 + 期望字符串，或用户按键步骤 | ✅ |
| R7 概念单一真相 | 云候选开关状态只有一份真相（配置键），面板与出货配置读写同一份；「哪些词是云端来的」只由 `CandidateInfo` 的新增数组承载，不在多处复制判定 | ✅ |
| R8 spec 进 git 不进聊天 | 本目录三件套随代码提交 | ✅ |
| R9 查优于记 | 本文件所有落点均给 `文件:行号` 实测证据；查不到的写【待核实】并排进 M0，不凭记忆 | ✅ |

### Principles

| 原则 | 相关性 | 判定 |
| --- | --- | --- |
| P1 格式与命名 | 涉及 `RimeWithWeasel` / `WeaselServer` / `WeaselIPC` / `WeaselUI` 源码，改动须跑既有格式化并保留原行尾（本目录三件套为无 BOM + LF） | ✅ |
| P2 用户可见字符串简体中文 + TSF 回调内禁阻塞 I/O | **本 spec 的关键约束**：网络只在常驻服务进程内发生，前端输入处理回调零网络、零同步等待；开关文案与首次明示全部简体中文 | ✅ |
| P3 不新增第三方依赖 | HTTP 客户端走系统自带 WinHTTP（倾向，最终在 T2.4 核实后定）；测试用的本地假端点是仓库内脚本，不进产品链接 | ✅ |
| P4 提交规范（Conventional Commits + 模块前缀） | 见「提交拆分」，scope 用 `WeaselServer` / `WeaselIPC` / `RimeWithWeasel` / `WeaselUI` / `docs` / `fluxing` / `ci` | ✅ |
| P5 不静默改默认键位 | 不改任何键位；云端词复用既有数字键/翻页语义（不新增 chord 家族），规避 097 类撞车 | ✅ |
| P6 用户数据目录 | 不新增用户侧定制副本；开关与白名单落在哪一处见 T0.3【待核实】，落点确定前不引入第三处状态 | ✅（受 T0.3 约束） |
| P7 唯一 supervisor，不派生新长寿命进程 | **本 spec 的硬边界**：请求器是常驻服务进程内的工作线程（既有先例见下），不新建进程；独立进程方案明确不采纳 | ✅ |
| P8 品牌 fork 允许范围 / 不改 librime 子模块与 IPC *语义* | librime 子模块保持不动（方案 D 不采纳）；IPC 只**追加**字段、不改既有字段语义（ADR 0028 决策 2） | ✅ |

---

## 技术方案

### 方案 A（推荐）：常驻服务进程内工作线程 + 候选唯一点合并 + 延迟一拍回填

三块拼起来就是全部主干：

1. **请求器落点 = `WeaselServer` 进程内一条工作线程**
   仓内已有同类先例，不需要新开进程：
   - `WeaselServer/UserDictionary.cpp:54` `std::thread UserDictionary::s_deployThread`（服务进程内的长寿命工作线程）
   - `WeaselServer/PhrasesDialogIPC.cpp:88` `CreateThread(..., PipeThreadProc, ...)`
   - `WeaselService.cpp:70` `boost::thread{[this]{ app.Run(); }}`（服务主循环）

   开关关闭时**不创建**该线程，从而空闲 CPU 与内存与现状等价（NFR-002/003）。
   线程只在「有新请求」时被条件变量唤醒，不做轮询；待处理槽位固定 1 个，
   输入推进时旧槽位直接**丢弃覆盖**（FR-011，不排队、不重放）。

2. **合并点 = `_GetCandidateInfo` 这个唯一出口**
   `RimeWithWeasel/RimeWithWeasel.cpp:712–778` 是服务端唯一的候选产出/过滤点，
   ADR 0023 决策 4 已把「候选的加工只允许发生在这一点」写进该函数的中文注释；
   同函数里 `_RefreshIgnoreList(session_id)`（`RimeWithWeasel.cpp:722`）已经示范了
   理想降级形态 —— **名单为空时本函数的输出与改动前逐字节一致**，读盘失败静默降级。
   云候选合并沿用同一语义：白名单空/无结果 ⇒ 输出与现在逐字节一致。

3. **回填 = 延迟一拍，随下一次应答带出**
   应答在 `_Respond` 内 `get_context → has_candidates → _GetCandidateInfo`
   （`RimeWithWeasel.cpp:1267–1273`）组装。云端结果不主动推送，而是在**下一次应答**
   随 `CandidateInfo` 一并带出。这样首版**不需要** server→client 的异步推送通道
   （该通道是否存在，`search_graph` + 定向 grep 均**零匹配**，见「【待核实】」）。
   代价是云端词晚一拍出现，列为 v1 已知限制（见「风险」R-8）。

**为什么必须动 IPC**：`include/WeaselIPCData.h:71` 的 `CandidateInfo`
（`candies` / `comments` / `labels` / `highlighted` / `currentPage` / `is_last_page`）
没有任何"来源"维度，把云端词直接塞进 `candies` 会破坏可见序↔引擎序映射
`_MapVisibleIndexToEngine`（`RimeWithWeasel.cpp:813–828`，越界返回 false 时调用方必须什么都不做），
进而破坏数字键与 Shift 选词语义（ADR 0023）。
所以走 **ADR 0028 决策 2：新增字段只能追加 + 双向安全降级** ——
在 `CandidateInfo` 末尾追加一组云端候选数组，老前端读到空数组即等同于没有云候选。
追加字段属于 `WeaselIPCData.h` 变更 ⇒ 触发**四边同步硬规则**
（`include/WeaselIPCData.h` + server / TSF / deployer 三端 + 一个 ADR + 一个 `test/TestWeaselIPC/*` 用例，一次性提交）。
既有先例可抄：`include/WeaselIPC.h:261–288` 就是同一 `EatLine` 回带 body 的追加式用法（ADR 0012）。

**选词与上屏**：云端词不在引擎候选序列里，因此**不能**走 `SelectCandidateOnCurrentPage`
（它只作用于引擎页内列表）。云端词选中后应作为**字符串直接提交**，
具体通道（合成串提交 vs 本机既有的剪贴板借用上屏，约 2 s 已知限制）**留 T0.2 核实**，
不在此预先断言。前端不得自己发起查找或联网（ADR 0028 决策 1 薄壳白名单）。

**开关与配置**：`app_options` 是当前"每应用/全局行为开关"的既有落点
（`output/data/weasel.yaml:18–40`，服务端经 `rime_api->set_option` 落地，
见 `RimeWithWeasel.cpp:695–709` 与 `inline_preedit` / `soft_cursor` 同批处理）。
快捷面板是开关入口的候选宿主（`WeaselServer/QuickPanelDialog.cpp`）。
两者读写同一份状态（R7）。

### 方案 B（升级路径，非首版）：真正的异步推送通道

新增一条 server→client 的候选更新通知，云端词到位即推送、当前候选窗立刻刷新。
收益是消除方案 A 的"晚一拍"；代价是新增 IPC 命令号（再次四边同步）+ 前端在输入回调里
处理外部通知的安全性与乱序问题（ADR 0029 已处理过面板显隐的投递与顺序，可读作参照）。
**不采纳为 v1**：先验证产品价值，再付协议的代价。

### 方案 C（明确不采纳）：独立云候选进程

会直接违反 ADR 0030 决策 2 的**常驻进程数 = 1** 发布验收基线与 P7；
进程拓扑也已由 [0031](../../../docs/adr/0031-console-session-single-server-and-self-heal.md) 单独定案
（一个 console 会话只有一个常驻服务、非 console 拒绝启动、缺位自愈），
再加一个进程等于重开这条已经关闭的决策。
若将来确有需求（例如把网络能力隔离到单独可更新进程），路径是：
先提一份新 ADR 显式**修订 ADR 0030 决策 2 的"1 进程"基线**（并同批处理 0031 的拓扑定案），
同一提交内更新性能门禁判据与发布验收条款，再改代码。**098 不开这个口子。**

### 明确不采纳（其余）

- **在 librime 侧挂 Extension/Module**：子模块不许改（P8）；
  实测 `librime/src/rime/module.h:14–36`、`module.cc:13–51`、`setup.cc:25–37`、`gear/gears_module.cc:95`
  只有 `ModuleManager` / `RIME_REGISTER_MODULE` 一层，没有面向外部的候选注入点；
  并且把联网放进引擎进程会把阻塞 I/O 带进候选计算，与 P2 的精神冲突。
- **在 TSF 前端发网络请求**：违反 P2（回调内禁阻塞 I/O）与 ADR 0028 决策 1「前台不得自己去找数据」。
- **复用第三方（搜狗）协议、字段语义或端点**：违反 ADR 0030 决策 4 与 spec「合规红线」；
  `docs/research/sogou-reverse-2026-11/evidence/urls-hosts.txt` 的 185 主机清单
  （`--- SGTool.exe --- hosts(185)`，含 `cloud.pinyin.sogou.com`）**永不连接**，只作黑名单来源。
- **把云端词插入引擎候选序列**：破坏 `_MapVisibleIndexToEngine` 的可见↔引擎映射与选词语义（R-1）。
- **v1 就上 WebSocket 长连接**：REPORT.md §7 路线 B 估 1–2 人月，HTTP 拉取版可先落地。

---

## 门禁与验证策略

| 门禁 | 怎么跑 | 通过线 |
| --- | --- | --- |
| 关闭态零请求（AC-01） | 默认出厂配置下跑固定输入序列，观察出向连接 | 请求数 = 0；`_GetCandidateInfo` 输出与上线前逐字节一致 |
| 三降级（AC-02/03/04） | 仓库内本地假端点脚本：正常返回 / 故意拖延 > 阈值 / 断连；白名单置空各跑一遍 | 本地候选与上屏全程正常；超时静默放弃；空白名单不发包 |
| 白名单与黑名单（AC-03/AC-09） | 白名单只含 1 主机 → 请求其它主机；白名单填入黑名单主机 → 配置被拒 | 非白名单主机不可达；含 `*.sogou.com` 的白名单拒绝生效并给出原因 |
| 选词语义不破坏（AC-06） | 云端词存在与否两种状态下，同一数字键/翻页序列指向完全相同的本地词 | 用例断言一致 |
| 隐私最小化（AC-07） | 抓包字段清单比对 spec FR-008 | 无设备/账号/应用/位置/剪贴板类字段；无遥测 |
| 性能三数字（AC-08，ADR 0030 决策 2） | 目视确认常驻进程数；取 `Get-Process WeaselServer` 的 `PrivateMemorySize64` 汇总；空闲 CPU 静止 ≥ 8 s 后采样（采样法同 REPORT.md §5） | **常驻进程数 1 / Server Private Working Set < 40 MB / 空闲 CPU < 0.1%（单核）**；对照物：第三方 6 进程 / 183.4 MB / 0.78% |
| 协议兼容 | `test/TestWeaselIPC/*` 新增用例：老前端遇新字段安全降级 | 新→老、老→新两个方向都不崩、不串位 |
| 装机实测 | 界面与性能证据只有真机才算：`env.bat` 置 `RELEASE_BUILD=1` 出包（版本号不得取 `git describe --tags`），NSIS silent-install 显式 `/D=<path>` 且先清 `HKLM\Software\Fluxing\Weasel` 与 `HKCU\Software\Fluxing`，装完再测 | 见 tasks.md T4.4 手工步骤 |

R1–R9 与 P1–P8 的逐条复核放在最后一个文档提交里做一次（对照本表与 tasks.md 的 Done 记录）。

---

## 提交拆分

每个提交 = 一个可独立回退的竖切，前缀用模块名。

| # | 提交 | 内容 | 回滚方式 |
| --- | --- | --- | --- |
| C1 | `docs(specs): 098 云候选立项三件套` | 本目录三个 `.md` | 删目录，无代码影响 |
| C2 | `chore(ci): 云候选门禁骨架 + 本地假端点` | 门禁脚本与假端点，先红后绿 | 还原脚本 |
| C3 | `feat(WeaselServer): 进程内云候选请求器（桩注入，默认关）` | 线程 + 单槽丢弃 + 桩数据源；开关关时不创建线程 |  revert（无外部行为） |
| C4 | `feat(RimeWithWeasel): 唯一点合并云端候选，空名单逐字节等价` | `_GetCandidateInfo` 合并 + 降级 | revert（等价于未合并） |
| C5 | `feat(WeaselIPC): CandidateInfo 追加云端候选数组（四边同步）` | `WeaselIPCData.h` + server/TSF/deployer 三端消费 + ADR 0032 + `test/TestWeaselIPC` 用例，**一次性提交** | 不可部分回滚，必须整体 revert |
| C6 | `feat(RimeWithWeasel): 云端词选中直接提交通路` | 不经引擎选词的提交路径 + 索引映射保护用例 | revert |
| C7 | `feat(WeaselServer): 端点白名单 + 超时 + WinHTTP 真请求` | 白名单解析、黑名单拒绝、超时静默放弃、替换桩 | 关开关即等效关闭；代码 revert |
| C8 | `feat(WeaselUI): 快捷面板开关 + 首次开启明示` | 面板入口（简体中文文案）+ 明示一次确认 | revert |
| C9 | `fluxing(perf): 三数字基线回归 + CHANGELOG` | 实测数字、对照表、变更日志 | 文档回退 |
| C10 | `docs(adr): 0032 云候选架构决策` | 若与 C5 分离则违反四边同步 ⇒ 实际以 C5 内联为准，本行仅在设计变更后启用 | — |

**R5 豁免声明（C5 / T1.3）**：四边同步硬规则要求一次提交覆盖
`include/WeaselIPCData.h` + 三端 + ADR + 测试（≥ 6 个文件），与 R5「1–3 文件」冲突；
取仓库更具体的硬规则，并在提交信息与 ADR 中写明该豁免。

**回滚开关**（FR / US7）：**不新增专门机制**，复用既有两条 ——
① 出货配置里的云候选开关默认关；② 端点白名单为空即等效物理断网。
两条任一生效，代码路径回到"不发请求 + 合并点输出逐字节一致"。

---

## 风险

| # | 风险 | 影响 | 缓解 |
| --- | --- | --- | --- |
| R-1 | 云端词与可见↔引擎索引映射冲突，破坏数字键 / Shift 选词 / 翻页 | **最高**，选错词比没词更糟 | 云端词不进引擎序列，独立数组（C5）；AC-06 用例；`_MapVisibleIndexToEngine` 保持原样 |
| R-2 | IPC 四边同步漏掉一端 → 老前端串位或崩 | 高 | 只追加字段 + 双向降级 + `test/TestWeaselIPC`；一次性提交；`.claude/rules/ipc-boundary.md` 走查 |
| R-3 | 性能基线破线（多出的线程被频繁唤醒 / 缓冲区常驻） | 高，属发布验收条款 | 开关关不建线程；条件变量唤醒不轮询；单槽丢弃；C9 实测三数字 |
| R-4 | 有人把网络挪进 TSF 回调 | 高（P2 违规、卡输入法） | 方案上明确禁止；代码走查项；无同步等待 API 暴露给前端 |
| R-5 | 逆向所得被当成实现依赖写进代码/文档 | 合规风险 | spec「合规红线」；文档只引用行为规格与主机清单；评审检查项 |
| R-6 | 日志/崩溃转储把输入内容带出去 | 隐私风险 | 不落明文、无遥测；AC-07 抓包 + 检查日志点 |
| R-7 | 自建服务端不存在 → 无法端到端演示 | 中 | M1 全用仓库内假端点闭环；真协议在 M2；服务端交付另立任务 |
| R-8 | 「延迟一拍」使云端词出现时机不连贯 | 中，体验 | 接受为 v1 限制；方案 B 作为升级路径，需新 ADR |
| R-9 | 云端词是否写用户词库不确定 → 用户以为记住了/以为没记住 | 中 | spec 默认**不写**；若要写，独立开关 + 明示，另立 task |
| R-10 | 工作区已存在他人未提交的改动，落点与本计划重叠（实测 `git diff --stat` 命中 `WeaselIPCServer/WeaselServerImpl.cpp`、`WeaselTSF/WeaselTSF.cpp`、`test/TestWeaselIPC/TestWeaselIPC.cpp`、`docs/adr/README.md`） | 高（冲突/误提交他人改动） | T1.3 与 T5.1 开工前先看这批改动是否已合入；合入后再取 ADR 号与基线；提交时只 `git add` 本任务文件，禁 `git add -A` |

---

## 【待核实】（M0 任务负责清零）

1. **异步回填通道有无**：`WeaselIPCServer/WeaselServerImpl.cpp` 搜 `Notify|Push|broadcast|ForEach`、
   `WeaselIPC/WeaselClientImpl.cpp` 搜 `wait_update|ProcessPendingUpdates` 均**零匹配** ⇒
   推定"无 server→client 候选推送通道"，**未做完整枚举**，T0.1 定论。
2. **云端词上屏通道**：合成串提交还是本机既有剪贴板借用（约 2 s 限制），T0.2 定论。
3. **开关与白名单落地位置**：出货配置 / `app_options` / `HKCU\Software\Fluxing` 三选一，T0.3 定论（R7 只允许一处真相）。
4. **开关生效条件**：改配置或面板切换后是否需重启常驻服务，T0.3 一并定。
5. **下一个可用 ADR 编号**：已确证 —— `docs/adr/` 目录实测最新为
   `0031-console-session-single-server-and-self-heal.md`（README 索引行同批脏改动新增，未见 0032），
   故 098 取 **0032**；T0.4 开工前再枚举一次以防他人抢号。
6. **HTTP 客户端**：WinHTTP 能否在现有链接设置与许可下直接使用（P3），T2.4 前置确证。
