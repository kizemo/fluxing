# 100 · Tasks · 云端账号与多端同步

> 配套 `spec.md`（用户视角）与 `plan.md`（选型 / 协议 / 算法 / 变更面）。
> 本文件是**执行序**：任务编号即推进顺序，同阶段内按编号先后。
> 每条任务带优先级 `[P1]/[P2]/[P3]` 与用户故事标签 `[US#]`（宪法 R3）；
> **单条任务 ≤ 4 小时、触及 ≤ 3 个文件**（宪法 R5）；
> **Done 判据 = 贴测试输出或可复跑证据路径**（宪法 R6）。
> `[P1]` 关键路径 / `[P2]` 重要 / `[P3]` 可延后。

---

## 0. 任务索引

| 阶段 | 主题 | 任务 | 出口判据 |
|---|---|---|---|
| **A** | 取证与工程底座 | T101–T104 | 三份取证结论落盘并回写 `plan.md`；测试工程能编译 |
| **B** | 契约与客户端纯逻辑 | T201–T204 | `test/TestFluxingCloud` 全绿（编解码 / 合并 / HLC） |
| **C** | 服务端（Cloudflare） | T211–T215 | `wrangler dev` 集成测试全绿 |
| **D** | 客户端运行时 | T301–T305 | 对 mock Worker 跑通注册 → 登录 → 续期 |
| **E** | 账号页与文案 | T401–T405 | 账号页可见；`AboutDialog` 文案改写进 CHANGELOG |
| **F** | 同步引擎与调度 | T501–T505 | 单机双用户目录同步收敛（并集 + 一致） |
| **G** | 双机验收与发版 | T601–T604 | AC-01 ~ AC-14 全过；三数字未破线；门禁全绿 |

**硬前置**：T102、T103 未出结论前，**禁止**写词库合并实现与端点硬编码（风险 R1 / R5）。

---

## 阶段 A · 取证与工程底座

### T101 · 邮箱验证码通道选型 [P1] [US1]
- **目标**：定下「验证邮箱归属」的送达通道与成本口径，形成可执行结论。
- **文件**：`docs/research/fluxing-cloud-2026-11/email-verify-channel.md`（新建）。
- **做法**：① 查 Cloudflare 侧事务性邮件能力的当前可用性与免费/付费口径；② 至少对比 1 家外部事务性邮件服务（按量计费）；③ 写明「首版是否强制验证」「未验证账号的降级行为」（`plan.md` §4.4 的 `pending` 规则）。
- **Done 判据**：结论落盘到上述文件 + 给出每千封验证信成本估算 + 明确选型（1 选 1，写进 CHANGELOG 前不得进代码）。
- **依赖**：无（**外部依赖，最早启动**）。

### T102 · 词库 / 短语导出·导入合并语义取证 [P1] [US3] [US4] [US8]
- **目标**：坐实「同一词条在两端的词频如何合并」—— 这是 R1 风险，**不得凭推测写实现**。
- **文件**：`docs/research/fluxing-cloud-2026-11/userdict-merge-semantics.md`（新建）+ 取证脚本落 `tmp/`（不入库）。
- **做法**：① 用 `RimeLeversApi` 导出用户词库，观察导出格式（是否含词频、是否含时间戳）；② 在隔离用户数据目录里导入「同词条、不同词频」的两份词库，记录最终词频；③ 记录导入是否覆盖同名词条、导入期间 leveldb 锁行为；④ 确认短语（用户短语）的落盘格式与是否可按条目读改。
- **Done 判据**：产出 3 条可复跑的结论（导出字段集 / 同名词条覆盖规则 / 导入时锁与并发行为），每条附实测输出片段路径。
- **依赖**：无。**结论未落盘前，T503（合并实现）不得开工**。

### T103 · 云端域名可达性与端点连通性取证 [P1] [US6] [US12]
- **目标**：确认「自有域名 + Cloudflare」在目标用户网络下的可达性与延迟，并确定首版端点策略。
- **文件**：`docs/research/fluxing-cloud-2026-11/endpoint-reachability.md`（新建）。
- **做法**：① 候选自有域名解析与握手耗时（境内多点）；② 与平台默认域名对照；③ 记录「同步失败时用户看到什么」的降级口径来源（写进 T501 的错误文案）；④ 确认是否需要在客户端保留「自定义端点」配置口子（默认不暴露 UI）。
- **Done 判据**：每个候选域名一行结论（可达 / 延迟量级 / 失败现象）+ 拍板首版端点。
- **依赖**：无。

### T104 · 测试工程骨架与系统库链接接线 [P1] [US12]
- **目标**：让 `test/TestFluxingCloud` 能编译运行，并把 WinHTTP / CNG / Credential Manager 三个系统库接进构建链。
- **文件**：`test/TestFluxingCloud/TestFluxingCloud.cpp`（新建）、`test/TestFluxingCloud/TestFluxingCloud.vcxproj`（新建）、`xmake.lua`（改，加测试目标）。
- **做法**：x86 Release；先放一条 `assert` 冒烟用例把工程跑通；链接 `winhttp.lib` / `bcrypt.lib` / `advapi32.lib`。
- **Done 判据**：贴出该工程「编译通过 + 用例通过」的输出（与其他 `tmp\build-qpanel-tests.cmd` 同口径）。
- **依赖**：无。

---

## 阶段 B · 契约与客户端纯逻辑

> 本阶段产出的纯逻辑（信封 / HLC / 合并）是**生产与单测同源**的单一事实源（宪法 R7），落在 `include/FluxingCloudEnvelope.h`。

### T201 · 信封编解码单测 [P1] [US7]
- **目标**：用判别性单测钉死条目信封的编解码行为（`plan.md` §4.1）。
- **文件**：`test/TestFluxingCloud/TestFluxingCloud.cpp`（改）。
- **做法**：覆盖 ① 正常往返一致；② **AAD 改动后解密必须失败**（防串位 / 防降级）；③ nonce 长度不符必须报错；④ base64 畸形输入必须报错；⑤ 空载荷与超长载荷（≥ 批次上限）的边界。
- **Done 判据**：贴单测输出（全部通过 + 用例计数），并列出「反例断言」至少 3 条。
- **依赖**：T104。

### T202 · 条目合并单测 [P1] [US8]
- **目标**：钉死「按条目合并」的语义（同键取新 / 异键取并集 / 收敛）。
- **文件**：`test/TestFluxingCloud/TestFluxingCloud.cpp`（改）。
- **做法**：① 同键不同版本取大；② 同键同版本结果确定（不得依赖字典序偶然性）；③ 异键取并集；④ **合并满足幂等**（merge(merge(a,b),c) == merge(a,merge(b,c))）；⑤ 合并后两端收敛（对同一对输入，两个方向合并结果一致）。
- **Done 判据**：贴单测输出 + 明写「收敛判据」的断言表达式。
- **依赖**：T201。

### T203 · HLC 定序单测 [P1] [US8]
- **目标**：钉死混合逻辑时钟的全序性质（`plan.md` §4.2）。
- **文件**：`test/TestFluxingCloud/TestFluxingCloud.cpp`（改）。
- **做法**：① `(ms,counter,device)` 比较满足**严格弱序**（用 std::sort 的等价类检查）；② 本机写入后 `ms >= last_ms`；③ 同毫秒自增 `counter`；④ 收到更远的远端 `ms` 后本机 `ms` 随之抬升；⑤ 两台设备对同一组版本算出**同一顺序**。
- **Done 判据**：贴单测输出（含严格弱序检查通过）。
- **依赖**：T201。

### T204 · 共享信封头落地 [P1] [US7]
- **目标**：把 B 阶段单测所测的纯逻辑抽到生产头文件，供客户端与服务端契约对齐。
- **文件**：`include/FluxingCloudEnvelope.h`（新建）、`test/TestFluxingCloud/TestFluxingCloud.cpp`（改）。
- **做法**：把 T201–T203 验证过的实现迁入头文件（`inline`，无状态、无 I/O）；单测改为**直接 include 生产头**（生产与单测同源，禁止复制两份实现）。
- **Done 判据**：贴单测输出 + `grep` 证明测试文件里没有第二份合并 / HLC 实现。
- **依赖**：T201 / T202 / T203。

---

## 阶段 C · 服务端（Cloudflare）

> 代码位置 `cloud/fluxing-sync/`；接口契约见 `plan.md` §6.1；D1 表见 §6.2；R2 布局见 §6.3。

### T211 · Worker 骨架与 D1 / R2 初始化 [P1] [US1]
- **目标**：起一个能跑的空 Worker，带 D1 迁移与 R2 桶绑定。
- **文件**：`cloud/fluxing-sync/wrangler.toml`（新建）、`cloud/fluxing-sync/migrations/0001_init.sql`（新建）、`cloud/fluxing-sync/src/index.ts`（新建）。
- **做法**：按 `plan.md` §6.2 建 4 张表（`users` / `devices` / `refresh_tokens` / `sync_items`）；`wrangler dev` 能起、`/v1/ping` 返回 200。
- **Done 判据**：贴 `wrangler dev` 启动日志 + 一次 `/v1/ping` 的请求/响应。
- **依赖**：无（可与阶段 B 并行）。

### T212 · 认证接口 [P1] [US1] [US2]
- **目标**：注册 / 验证 / 登录 / 续期四件事可用，且服务端**不落明文口令**。
- **文件**：`cloud/fluxing-sync/src/auth.ts`（新建）、`cloud/fluxing-sync/src/index.ts`（改）。
- **做法**：① 注册存 `verifier = HMAC-SHA256(server_pepper, auth_key)`（pepper 走 Workers Secret，**不进代码库 / 不进 D1**）；② 登录比对 `verifier`；③ 续期轮换 refresh token，服务端**只存哈希**；④ 邮箱未验证的账号不得写入同步数据（`pending` 态）。
- **Done 判据**：贴 `wrangler dev` 下四条接口的请求/响应日志 + 一条「错误口令返回中文可读错误码」的用例。
- **依赖**：T211、T101（验证策略）。

### T213 · 同步推拉接口 [P1] [US3] [US5] [US7]
- **目标**：push / pull 按 `plan.md` §6.1 契约实现，且服务端只见密文。
- **文件**：`cloud/fluxing-sync/src/sync.ts`（新建）、`cloud/fluxing-sync/src/index.ts`（改）。
- **做法**：① push 写 R2 对象 + D1 索引行；② pull 支持 `since` 增量 + `cursor` 分页；③ 服务端**不解密**、不解析载荷；④ 对象键 = `u/<account_id>/<sha256(item_key)>`。
- **Done 判据**：贴「推 3 条 → 拉回 3 条」的请求/响应日志 + 一次 R2 对象内容抽查（确认是密文）。
- **依赖**：T211、T212。

### T214 · 设备与账号端点 [P1] [US9] [US11]
- **目标**：设备列表 / 移除设备 / 清空云端数据三条管理接口。
- **文件**：`cloud/fluxing-sync/src/devices.ts`（新建）、`cloud/fluxing-sync/src/account.ts`（新建）。
- **做法**：① 移除设备后其 refresh token 立即失效；② 清空 = 批量删 R2 对象 + 删 D1 索引行（**任务清单化 + 重试**，不吞失败）；③ 清空接口返回实际删除计数。
- **Done 判据**：贴「移除设备后续期被拒」与「清空后 R2 前缀为空」的证据各一条。
- **依赖**：T212、T213。

### T215 · 服务端集成测试 [P1] [US1] [US12]
- **目标**：把 C 阶段接口纳入可复跑的测试脚本。
- **文件**：`cloud/fluxing-sync/test/api.test.ts`（新建）。
- **做法**：覆盖注册→登录→续期→推→拉→撤销设备→清空全链路 + 错误分支（错误口令、未验证账号写数据、越权取他人条目）。
- **Done 判据**：贴测试运行输出（通过数 / 失败数）。
- **依赖**：T214。

---

## 阶段 D · 客户端运行时

### T301 · CNG 加密模块 [P1] [US7]
- **目标**：实现 `plan.md` §3.1 的密钥派生与信封加密。
- **文件**：`WeaselServer/Cloud/CloudCrypto.h`（新建）、`WeaselServer/Cloud/CloudCrypto.cpp`（新建）。
- **做法**：① `BCryptDeriveKeyPBKDF2`（600000 次，HMAC-SHA256）派生 `master`；② HKDF-SHA256 分离 `auth_key` / `enc_key`；③ AES-256-GCM 加密，AAD = `item_key|type|hlc`；④ **口令缓冲用后清零**；⑤ 全程不落日志。
- **Done 判据**：贴单测/探针输出（固定向量往返一致 + AAD 改动解密失败）+ 一条「日志中无口令 / 密钥」的 grep 证据。
- **依赖**：T204、T102（口令强度下限规则）。

### T302 · WinHTTP 传输模块 [P1] [US6] [US8]
- **目标**：把网络层抽象成可 mock 的 `ICloudTransport`。
- **文件**：`WeaselServer/Cloud/CloudTransport.h`（新建）、`WeaselServer/Cloud/CloudTransport.cpp`（新建）。
- **做法**：超时、重试退避、代理自动探测、证书校验不得被关闭；**不得**在 UI 线程调用。
- **Done 判据**：贴一次真实请求日志（端点 / 耗时 / 状态码）+ 一次代理环境失败时的降级日志（中文原因）。
- **依赖**：T104。

### T303 · 凭据存储 [P1] [US2] [US9]
- **目标**：refresh token 与 `enc_key` 存 Credential Manager（DPAPI 保护）。
- **文件**：`WeaselServer/Cloud/CloudCredentialStore.h`（新建）、`WeaselServer/Cloud/CloudCredentialStore.cpp`（新建）。
- **做法**：① 存 / 读 / 删三路径；② 读取失败（凭据被清）⇒ 视为未登录，回到登录态而非报错崩溃。
- **Done 判据**：贴「写入后重启进程仍能读出」的证据 + 「删除后应用回到未登录态」的证据。
- **依赖**：T301。

### T304 · 账户客户端 [P1] [US1] [US2] [US9]
- **目标**：注册 / 登录 / 续期 / 登出 / 设备列表 / 清空云端的客户端门面。
- **文件**：`WeaselServer/Cloud/CloudAccount.h`（新建）、`WeaselServer/Cloud/CloudAccount.cpp`（新建）。
- **做法**：状态机 `未登录 → 登录中 → 已登录 → 需重新登录`；续期失败自动重试一次，仍失败才落「需重新登录」。
- **Done 判据**：贴一次完整「注册 → 登录 → 续期 → 登出」的调用序列日志（含状态迁移）。
- **依赖**：T301、T302、T303、T212。

### T305 · 词库 / 短语导出导入协调 [P1] [US3] [US4]
- **目标**：在**进程内**导出与导入用户词库、短语，并与引擎读写互斥。
- **文件**：`WeaselServer/Cloud/CloudUserDictIo.h`（新建）、`WeaselServer/Cloud/CloudUserDictIo.cpp`（新建）。
- **做法**：① 复用既有维护态机制挂起引擎对该用户库的访问；② 导出 / 导入**在后台线程**完成，绝不在输入回调里做；③ 导出的条目化格式按 **T102 的取证结论**定（分隔符 / 字段数）。
- **Done 判据**：贴「导出 → 改文件 → 导入 → 词频符合 T102 结论」的证据 + 一条「导入期间输入不卡顿」的时序记录。
- **依赖**：T102、T303。

---

## 阶段 E · 账号页与文案

### T401 · 账号页对话框 [P1] [US1] [US9] [US12]
- **目标**：快捷设置栏第 4 槽升级为账号页（分区：登录状态 / 邮箱 / 设备列表 / 同步开关与最近同步时间 / 立即同步 / 登出 / 清空云端）。
- **文件**：`WeaselServer/Cloud/CloudAccountDialog.h`（新建）、`WeaselServer/Cloud/CloudAccountDialog.cpp`（新建）。
- **做法**：与既有 `QuickPanelDialog` 同栈（WTL）；**密码框不回显**；口令用后清零；错误一律「状态 + 中文原因」，不弹打断式错误框。
- **Done 判据**：贴账号页截图（未登录态 / 已登录态各一张）+ 一条「错误口令有中文提示且不进入登录态」的证据。
- **依赖**：T304。

### T402 · 部署器入口 —— 首版明确不做 [P2] [US12]
- **目标**：**记录**「在 `WeaselDeployer` 加账号入口需触发 IPC 四边同步」这一约束，首版不实现。
- **文件**：`docs/adr/0039-cloud-account-sync-architecture.md`（改，补一节「触发条件」）。
- **做法**：写明触发条件：若将来在部署器加账号入口 ⇒ 必须新增 IPC 消息 ⇒ 届时才需要 server / TSF / deployer 三端同步 + ADR + `test/TestWeaselIPC` 用例。首版账号页只挂在快捷面板第 4 槽 ⇒ **不新增任何 IPC 消息**。
- **Done 判据**：ADR 0039 中该节落盘 + `include/WeaselIPCData.h` 在整个阶段 E 保持未改（贴 `git diff --stat` 证据）。
- **依赖**：T401。

### T403 · `AboutDialog` 文案改写 [P1] [US7] [US12]
- **目标**：改掉与新功能自相矛盾的旧文案。
- **文件**：`WeaselServer/AboutDialog.cpp`（改）、`CHANGELOG.md`（改）。
- **做法**：旧文案「本输入法不含云端账号。词库、配置与用户数据全部保存在本机，不上传、不同步。」**必须改写**为「默认不上传、不同步；登录后仅同步你自己造的资产，且离开本机前已加密」。新增两条免责：「口令遗忘后云端数据无法找回」「同步内容在离开本机前已加密」。
- **Done 判据**：贴改写后的文案全文 + `CHANGELOG.md`「主要更新」条目。
- **依赖**：T401。

### T404 · CHANGELOG 条目 [P1] [US1]
- **目标**：用户可见改动全部进 CHANGELOG（宪法 P5）。
- **文件**：`CHANGELOG.md`（改）。
- **做法**：条目覆盖账号页上线、登录 / 同步 / 设备管理的用户可见行为、以及「不上传输入历史」的边界声明。
- **Done 判据**：贴 CHANGELOG 新条目全文。
- **依赖**：T401、T403。

### T405 · 快捷面板第 4 槽接线 [P1] [US1]
- **目标**：第 4 槽从「关于说明」升级为「账号页」入口，保留「关于」分区。
- **文件**：`WeaselServer/QuickPanelDialog.cpp`（改）、`WeaselServer/QuickPanelDialog.h`（改）。
- **做法**：**不改面板几何与状态真相源**（spec 099 边界）；「关于」分区保留原内容。
- **Done 判据**：贴点击第 4 槽进入账号页的真机截图。
- **依赖**：T401。

---

## 阶段 F · 同步引擎与调度

### T501 · 引擎骨架与调度 [P1] [US6] [US12]
- **目标**：后台线程 + 事件唤醒的调度器，实现 `plan.md` §7 的触发矩阵。
- **文件**：`WeaselServer/Cloud/CloudSyncEngine.h`（新建）、`WeaselServer/Cloud/CloudSyncEngine.cpp`（新建）。
- **做法**：启动拉一次（**不阻塞启动流程**，失败静默）；本地变更防抖 5 分钟后推（全局上限 15 分钟/设备）；每 60 分钟拉一次；手动「立即同步」；事件等待 + 定时器唤醒（**不使用轮询**）。
- **Done 判据**：贴调度日志（触发时刻 + 动作）+ 空闲 CPU 基线未破线的采样。
- **依赖**：T304。

### T502 · 推送与拉取 [P1] [US3] [US5]
- **目标**：把变更条目推上去、把远端增量拉回来。
- **文件**：`WeaselServer/Cloud/CloudSyncEngine.cpp`（改）、`WeaselServer/Cloud/CloudUserDictIo.cpp`（改）。
- **做法**：推只带**变化条目**（一次推合并多条）；拉走 `since` 增量；单次同步数据量有上限，超限分批并给进度。
- **Done 判据**：贴一次「本地改配置 → 推 → 拉回一致」的日志。
- **依赖**：T501、T305、T213。

### T503 · 合并与回传 [P1] [US8]
- **目标**：客户端解密远端密文 → 与本地按条目合并 → 加密回传，使两端收敛。
- **文件**：`WeaselServer/Cloud/CloudSyncEngine.cpp`（改）、`include/FluxingCloudEnvelope.h`（改）。
- **做法**：`config` 整份取 HLC 较大者；`phrases` / `userdb` 按条目键合并后**重新加密并回传**（新 HLC）。**条目的键与词频合并规则必须照 T102 的结论实现**，不得凭推测。
- **Done 判据**：贴「两端同时改不同条目 → 同步后两端为并集且一致」的日志（AC-08 / AC-09）。
- **依赖**：**T102 结论落盘**、T502。

### T504 · 降级与退避 [P1] [US6]
- **目标**：断网 / 失败时静默降级，不阻断输入、不产生重试风暴。
- **文件**：`WeaselServer/Cloud/CloudSyncEngine.cpp`（改）、`WeaselServer/Cloud/CloudTransport.cpp`（改）。
- **做法**：指数退避 1 / 5 / 15 / 60 分钟，最多 4 次后转「等待下一次定时」；**不弹窗、不阻塞输入**；断网期间不排队累积，恢复后按防抖窗口补推；每日推次数上限 100 / 拉 50。
- **Done 判据**：贴断网期间的重试序列日志（确认退避阶梯与无风暴）+ 恢复后自动补推的一次成功记录。
- **依赖**：T501。

### T505 · 同步状态呈现 [P1] [US9] [US12]
- **目标**：账号页显示最近一次同步时间与结果、失败的中文原因。
- **文件**：`WeaselServer/Cloud/CloudAccountDialog.cpp`（改）、`WeaselServer/Cloud/CloudSyncEngine.h`（改）。
- **做法**：引擎暴露只读状态快照（最近同步时间 / 结果 / 失败原因文案），UI 轮询式刷新但**不做阻塞等待**。
- **Done 判据**：贴账号页上「最近同步：xx:xx，成功 / 失败（中文原因）」的截图。
- **依赖**：T501、T401。

---

## 阶段 G · 双机验收与发版

### T601 · 验收用例全跑 [P1] [US1] [US2] [US3] [US4] [US5] [US6] [US8] [US9] [US11] [US12]
- **目标**：逐条跑 AC-01 ~ AC-14，产出证据索引。
- **文件**：`docs/research/fluxing-cloud-2026-11/acceptance-matrix.md`（新建）、`tmp/`（证据，不入库）。
- **做法**：AC-02 / AC-03 / AC-04 / AC-05 / AC-08 / AC-09 / AC-13 用**两台设备或两套隔离用户数据目录**；AC-07 做抓包与存储抽查；AC-10 验「登出后本机数据仍在 / 清空后云端无可恢复内容」。
- **Done 判据**：14 条 AC 逐条写「通过 / 不通过 + 证据路径」，无「待定」。
- **依赖**：阶段 E、F 全绿。

### T602 · 性能三数字与输入延迟 [P1] [US10]
- **目标**：NFR-001 ~ NFR-003 未破线。
- **文件**：`docs/research/fluxing-cloud-2026-11/perf-baseline.md`（新建）。
- **做法**：按 ADR 0030 的命令测三数字（进程数 = 1 / 私有内存 < 40 MB / 空闲 CPU < 0.1%）；同步开启与关闭两组对照测输入延迟。
- **Done 判据**：贴两组实测数字 + 结论「未破线」。
- **依赖**：T601。

### T603 · 合规核查 [P1] [US7] [US12]
- **目标**：未登录零出向请求；服务端存储抽查为密文。
- **文件**：`docs/research/fluxing-cloud-2026-11/compliance-check.md`（新建）。
- **做法**：未登录态全流程抓包计数 = 0（AC-12 / R9）；服务端 R2 对象抽查确认无明文词条 / 短语 / 配置。
- **Done 判据**：贴「未登录出向请求数 = 0」的抓包计数 + 一条 R2 对象内容抽查证据。
- **依赖**：T601。

### T604 · 门禁与发版 [P2] [US12]
- **目标**：门禁全绿后按发版流程出包。
- **文件**：`CHANGELOG.md`（改，如需补发版条目）、`docs/archive/handoffs/`（新建收尾 handoff）、`docs/archive/prompts/`（新建接力 prompt）、`CLAUDE.md`（改 §2 指针）。
- **做法**：跑 `scripts/gate/run-all.ps1 -Level L0`；改动 ≥3 文件加 L4；发版前 L5。注意：`WeaselServer.exe` 是 PPL ⇒ 装机前先注销 / 重启（R4）。
- **Done 判据**：贴门禁输出（全绿）+ 收尾 handoff 与接力 prompt 的路径。
- **依赖**：T601、T602、T603。

---

## 附：不在本批范围

| 项 | 说明 |
|---|---|
| 找回密码 / 修改密码 | 首版不做（`spec.md` §3.3）；改密会牵动全量重加密 |
| 网页端管理云端数据 | 首版只有桌面端 |
| 团队 / 家庭组织账号 | 首版不做 |
| 输入历史跨设备回看 | 隐私红线，永不离开本机 |
| `WeaselDeployer` 账号入口 | 首版不做；若做则触发 IPC 四边同步（见 T402） |
| 服务端看明文 | 用户已拍板全量端到端加密，不做明文通道 |
