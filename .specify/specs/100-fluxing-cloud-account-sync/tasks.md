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
| **E** | 账号页与文案 | T401–T405、T408 | 账号页可见；`AboutDialog` 文案改写进 CHANGELOG；注册 + 邮箱验证码链路可用 |
| **F** | 同步引擎与调度 | T501–T505 | 单机双用户目录同步收敛（并集 + 一致） |
| **G** | 双机验收与发版 | T601–T604 | AC-01 ~ AC-14 全过；三数字未破线；门禁全绿 |

**硬前置**：T102、T103 未出结论前，**禁止**写词库合并实现与端点硬编码（风险 R1 / R5）。　**→ 2026-11-10 已解除：两份结论均已落盘（见 `docs/research/fluxing-cloud-2026-11/`）。**

**文档治理（2026-11-11，用户拍板「按实现回写」）**：阶段 A–C 的偏差 **A1–A3**（本文件字面 vs 实做）与 **D1–D6**（`plan.md` §6.2 的 D1 表）已按实做回写进 `plan.md` §6.1 / §6.2 / §5 / §13 与本文件各阶段的「状态」行；阶段 D 的实现偏差（T301 改自实现密码学、T305 改快照通道、T401 改普通 Win32 窗口、T405 入口改快捷设置栏右键菜单）同样已回写。**本文件与 `plan.md` 自此与代码同源** —— 后续会话只读一套真相。

---

## 阶段 A · 取证与工程底座

### T101 · 邮箱验证码通道选型 [P1] [US1]
- **状态**：**DONE 2026-11-10** · 交付 `docs/research/fluxing-cloud-2026-11/email-verify-channel.md`（9,305 B）；选型 = Cloudflare Email Service（Workers Paid 计划，3,000 封/月）；未实时核价项见 md §6
- **目标**：定下「验证邮箱归属」的送达通道与成本口径，形成可执行结论。
- **文件**：`docs/research/fluxing-cloud-2026-11/email-verify-channel.md`（新建）。
- **做法**：① 查 Cloudflare 侧事务性邮件能力的当前可用性与免费/付费口径；② 至少对比 1 家外部事务性邮件服务（按量计费）；③ 写明「首版是否强制验证」「未验证账号的降级行为」（`plan.md` §4.4 的 `pending` 规则）。
- **Done 判据**：结论落盘到上述文件 + 给出每千封验证信成本估算 + 明确选型（1 选 1，写进 CHANGELOG 前不得进代码）。
- **依赖**：无（**外部依赖，最早启动**）。

### T102 · 词库 / 短语导出·导入合并语义取证 [P1] [US3] [US4] [US8]
- **状态**：**DONE 2026-11-10** · 交付 `docs/research/fluxing-cloud-2026-11/userdict-merge-semantics.md`（23,309 B）+ 探针 `tmp/t102/`（不入库）；**硬前置红线已解除**：T503 基座改用 `backup_user_dict` + `restore_user_dict`（`UserDbMerger`），`export` / `import` 不适合作合并原语
- **目标**：坐实「同一词条在两端的词频如何合并」—— 这是 R1 风险，**不得凭推测写实现**。
- **文件**：`docs/research/fluxing-cloud-2026-11/userdict-merge-semantics.md`（新建）+ 取证脚本落 `tmp/`（不入库）。
- **做法**：① 用 `RimeLeversApi` 导出用户词库，观察导出格式（是否含词频、是否含时间戳）；② 在隔离用户数据目录里导入「同词条、不同词频」的两份词库，记录最终词频；③ 记录导入是否覆盖同名词条、导入期间 leveldb 锁行为；④ 确认短语（用户短语）的落盘格式与是否可按条目读改。
- **Done 判据**：产出 3 条可复跑的结论（导出字段集 / 同名词条覆盖规则 / 导入时锁与并发行为），每条附实测输出片段路径。
- **依赖**：无。**结论未落盘前，T503（合并实现）不得开工**。

### T103 · 云端域名可达性与端点连通性取证 [P1] [US6] [US12]
- **状态**：**DONE 2026-11-10** · 交付 `docs/research/fluxing-cloud-2026-11/endpoint-reachability.md`（13,597 B）+ 实测输出 `tmp/t103/endpoint-probe.txt`；拍板首版端点 `https://api.<自有域名>` + CF Worker，**明确排除 `*.workers.dev`**（DNS 污染指纹）
- **目标**：确认「自有域名 + Cloudflare」在目标用户网络下的可达性与延迟，并确定首版端点策略。
- **文件**：`docs/research/fluxing-cloud-2026-11/endpoint-reachability.md`（新建）。
- **做法**：① 候选自有域名解析与握手耗时（境内多点）；② 与平台默认域名对照；③ 记录「同步失败时用户看到什么」的降级口径来源（写进 T501 的错误文案）；④ 确认是否需要在客户端保留「自定义端点」配置口子（默认不暴露 UI）。
- **Done 判据**：每个候选域名一行结论（可达 / 延迟量级 / 失败现象）+ 拍板首版端点。
- **依赖**：无。

### T104 · 测试工程骨架与系统库链接接线 [P1] [US12]
- **状态**：**DONE 2026-11-10** · `test/TestFluxingCloud`（cpp + vcxproj + xmake.lua）已接进根 `xmake.lua`；x86 Release 编译通过，冒烟 `RESULT: PASS=6 FAIL=0` / `VERDICT: ALL PASS` / `TEST_FLUXINGCLOUD_EXIT=0`（脚本 `tmp/build-fluxing-cloud-tests.cmd`）
- **目标**：让 `test/TestFluxingCloud` 能编译运行，并把 WinHTTP / CNG / Credential Manager 三个系统库接进构建链。
- **文件**：`test/TestFluxingCloud/TestFluxingCloud.cpp`（新建）、`test/TestFluxingCloud/TestFluxingCloud.vcxproj`（新建）、`xmake.lua`（改，加测试目标）。
- **做法**：x86 Release；先放一条 `assert` 冒烟用例把工程跑通；链接 `winhttp.lib` / `bcrypt.lib` / `advapi32.lib`。
- **Done 判据**：贴出该工程「编译通过 + 用例通过」的输出（与其他 `tmp\build-qpanel-tests.cmd` 同口径）。
- **依赖**：无。

---

## 阶段 B · 契约与客户端纯逻辑

> 本阶段产出的纯逻辑（信封 / HLC / 合并）是**生产与单测同源**的单一事实源（宪法 R7），落在 `include/FluxingCloudEnvelope.h`。

### T201 · 信封编解码单测 [P1] [US7]
- **状态**：**DONE 2026-11-10** · 单测段 [5]–[10] 共 71 条断言；整包 `RESULT: PASS=134 FAIL=0` / `VERDICT: ALL PASS` / `TEST_FLUXINGCLOUD_EXIT=0`。①–⑤ 全覆盖；反例断言清单（AAD 4 项 / 密文 2 项 / 错密钥 / nonce 3 项 / base64 2 项 / 线格式 10 项）见 `docs/archive/handoffs/handoff-2026-11-10-batch-y-cloud-stage-b.md` §4
- **目标**：用判别性单测钉死条目信封的编解码行为（`plan.md` §4.1）。
- **文件**：`test/TestFluxingCloud/TestFluxingCloud.cpp`（改）。
- **做法**：覆盖 ① 正常往返一致；② **AAD 改动后解密必须失败**（防串位 / 防降级）；③ nonce 长度不符必须报错；④ base64 畸形输入必须报错；⑤ 空载荷与超长载荷（≥ 批次上限）的边界。
- **Done 判据**：贴单测输出（全部通过 + 用例计数），并列出「反例断言」至少 3 条。
- **依赖**：T104。

### T202 · 条目合并单测 [P1] [US8]
- **状态**：**DONE 2026-11-10** · 单测段 [13]（合并 15 条）+ [14]（条目表序列化 16 条）= 31 条断言。收敛判据显式写成 `Check(fc::MergeEntries(b, a) == m, "⑤ 交换输入顺序结果相同（两端收敛）")`；另证 `merge(x,x) == x` 幂等 2 条、结合律 2 条、空集恒等 3 条
- **目标**：钉死「按条目合并」的语义（同键取新 / 异键取并集 / 收敛）。
- **文件**：`test/TestFluxingCloud/TestFluxingCloud.cpp`（改）。
- **做法**：① 同键不同版本取大；② 同键同版本结果确定（不得依赖字典序偶然性）；③ 异键取并集；④ **合并满足幂等**（merge(merge(a,b),c) == merge(a,merge(b,c))）；⑤ 合并后两端收敛（对同一对输入，两个方向合并结果一致）。
- **Done 判据**：贴单测输出 + 明写「收敛判据」的断言表达式。
- **依赖**：T201。

### T203 · HLC 定序单测 [P1] [US8]
- **状态**：**DONE 2026-11-10** · 单测段 [11]（全序 11 条：18 个版本笛卡尔积 / 324 项反对称 / 153 项可区分 / 5832 项传递）+ [12]（本机时钟 14 条，含 counter 打满 0xFFFF 与时钟回拨）= 25 条断言。严格弱序由「`std::sort(HlcLess)` 结果非降」+「正序与逆序排序互为镜像」双向确认
- **目标**：钉死混合逻辑时钟的全序性质（`plan.md` §4.2）。
- **文件**：`test/TestFluxingCloud/TestFluxingCloud.cpp`（改）。
- **做法**：① `(ms,counter,device)` 比较满足**严格弱序**（用 std::sort 的等价类检查）；② 本机写入后 `ms >= last_ms`；③ 同毫秒自增 `counter`；④ 收到更远的远端 `ms` 后本机 `ms` 随之抬升；⑤ 两台设备对同一组版本算出**同一顺序**。
- **Done 判据**：贴单测输出（含严格弱序检查通过）。
- **依赖**：T201。

### T204 · 共享信封头落地 [P1] [US7]
- **状态**：**DONE 2026-11-10** · 新建 `include/FluxingCloudEnvelope.h`（34.9 KB，`inline` 无状态无 I/O）；`test/TestFluxingCloud/TestFluxingCloud.cpp` 已改为 `#include "../../include/FluxingCloudEnvelope.h"`，整包 `PASS=134 FAIL=0`。**无第二份实现取证**：测试文件内 `^(struct|class|inline|static)\s.*(Hlc|Envelope|Entry|Merge|Base64|Seal|Open)` 零命中；`CompareHlc` / `HlcTick` / `MergeEntries` 全仓仅 `FluxingCloudEnvelope.h` 各 1 处定义（435 / 470 / 993 行），测试侧全部走 `fc::` 前缀（271 处）
- **目标**：把 B 阶段单测所测的纯逻辑抽到生产头文件，供客户端与服务端契约对齐。
- **文件**：`include/FluxingCloudEnvelope.h`（新建）、`test/TestFluxingCloud/TestFluxingCloud.cpp`（改）。
- **做法**：把 T201–T203 验证过的实现迁入头文件（`inline`，无状态、无 I/O）；单测改为**直接 include 生产头**（生产与单测同源，禁止复制两份实现）。
- **Done 判据**：贴单测输出 + `grep` 证明测试文件里没有第二份合并 / HLC 实现。
- **依赖**：T201 / T202 / T203。

---

## 阶段 C · 服务端（Cloudflare）

> 代码位置 `cloud/fluxing-sync/`；接口契约见 `plan.md` §6.1；D1 表见 §6.2；R2 布局见 §6.3。
>
> **状态：DONE 2026-11-10**（提交 `5495d52`）· **2026-11-11 回写**：T211 的「4 张表 / `wrangler dev` / `/v1/ping`」字面已按实做更正 —— 实做 = **7 表 4 索引**（**D1–D6**，见 `plan.md` §6.2）、**vitest-pool-workers 真 workerd** 20/20 用例（**没有** `wrangler dev` 启动日志）、探活端点 **`GET /v1/health`**（原 `/v1/ping` **不存在**）。**Done 判据换载体**：以 `npm run check` 退出码 0 + 20/20 用例 + 变异验证（改错一个错误码即 1 failed）为准。

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

> 代码位置 `WeaselServer/Cloud/`（**18 个文件** = 8 个模块的 `.h`/`.cpp` 共 16 个 + `CloudRimeUserDictBackend.cpp` + `CloudMaintenanceHost.h`）；单测落 `test/TestFluxingCloud/`（**追加 8 个测试函数**，整包 `PASS=297 FAIL=0`）。
> **2026-11-11 T301–T305 全部 DONE**（提交见 handoff 批次 AB）。

### T301 · 密钥派生与信封加密 [P1] [US7]
- **状态**：**DONE 2026-11-11** · `WeaselServer/Cloud/CloudCrypto.{h,cpp}`；决策见 **ADR 0054**。**实现偏差**（原写「`BCryptDeriveKeyPBKDF2` / CNG」）：PBKDF2-HMAC-SHA256 / HKDF-SHA256 **自实现**（CNG 的 PBKDF2 **默认 PRF=SHA-1**、且**无 HKDF** 语义）；**AES-256-GCM 仍复用** `include/FluxingCloudEnvelope.h` 的 `crypto::AesGcmSeal/Open` 与 **5 字段** `BuildAad`（`item_key|type|hlc_ms|hlc_counter|device`）。**salt 策略**：按邮箱确定性派生（`SHA256("fluxing-enc-v1|" + lower(email))`）—— 服务端**从不用 salt 校验**，客户端登录前拿不到服务端随机 salt。**Done 判据已达成**：标准向量（SHA-256 FIPS 180-4 / HMAC RFC 4231 TC1·TC2·TC3·TC6 / PBKDF2 RFC 7914 §11 / HKDF 公式复算 + .NET 独立实现）+ AAD 改动解密失败的反例，落在 `TestCloudCryptoVectors` / `TestCloudSyncPure`。
- **目标**：实现 `plan.md` §3.1 的密钥派生与信封加密。
- **文件**：`WeaselServer/Cloud/CloudCrypto.h`（新建）、`WeaselServer/Cloud/CloudCrypto.cpp`（新建）。
- **做法**：① 派生 `master`（PBKDF2-HMAC-SHA256，600000 次，32 B）；② HKDF-SHA256 分离 `auth_key` / `enc_key`（info = `fluxing-auth-v1` / `fluxing-enc-v1`）；③ AES-256-GCM 加密，AAD = 5 字段；④ **口令缓冲用后清零**；⑤ 全程不落日志。
- **Done 判据**：贴单测/探针输出（固定向量往返一致 + AAD 改动解密失败）+ 一条「日志中无口令 / 密钥」的 grep 证据。
- **依赖**：T204、T102（口令强度下限规则）。

### T302 · WinHTTP 传输模块 [P1] [US6] [US8]
- **状态**：**DONE 2026-11-11** · `WeaselServer/Cloud/CloudTransport.{h,cpp}`：`ICloudTransport` + `WinHttpTransport`；BaseUrl 可注入（环境变量 `FLUXING_CLOUD_BASE_URL`，默认 `https://api.aiec.fun`）；超时 15 s / 重试 2 次 / 指数退避 500→1000→2000→8000 ms 封顶；代理自动探测；**证书校验不关**（`WINHTTP_OPTION_DISABLE_FEATURE` + `WINHTTP_DISABLE_AUTHENTICATION` 只关**自动认证**，不关证书链校验）。**Done 判据部分达成**：纯函数 16 条断言（`BuildUrl` / `ShouldRetry` / `RetryBackoffMs` / `UrlEncodePathSegment`）；**未达成**：真实请求日志与代理环境降级日志 —— 依赖服务端部署（外部资源）。
- **目标**：把网络层抽象成可 mock 的 `ICloudTransport`。
- **文件**：`WeaselServer/Cloud/CloudTransport.h`（新建）、`WeaselServer/Cloud/CloudTransport.cpp`（新建）。
- **做法**：超时、重试退避、代理自动探测、证书校验不得被关闭；**不得**在 UI 线程调用。
- **Done 判据**：贴一次真实请求日志（端点 / 耗时 / 状态码）+ 一次代理环境失败时的降级日志（中文原因）。
- **依赖**：T104。

### T303 · 凭据存储 [P1] [US2] [US9]
- **状态**：**DONE 2026-11-11** · `WeaselServer/Cloud/CloudCredentialStore.{h,cpp}`：target `Fluxing:CloudAccount`、`CRED_TYPE_GENERIC` + `CRED_PERSIST_LOCAL_MACHINE`（DPAPI 保护）；自带二进制编解码（magic `FLCRED1` + 8 字段长度前缀 + **无尾随字节**校验）。**Done 判据已达成**：真实 Credential Manager 的 Save/Load/Delete 往返 + 删除后读不到（`TestCloudCredentialStoreReal`，独立 target `Fluxing:Test:CloudAccount`）。
- **目标**：refresh token 与 `enc_key` 存 Credential Manager（DPAPI 保护）。
- **文件**：`WeaselServer/Cloud/CloudCredentialStore.h`（新建）、`WeaselServer/Cloud/CloudCredentialStore.cpp`（新建）。
- **做法**：① 存 / 读 / 删三路径；② 读取失败（凭据被清）⇒ 视为未登录，回到登录态而非报错崩溃。
- **Done 判据**：贴「写入后重启进程仍能读出」的证据 + 「删除后应用回到未登录态」的证据。
- **依赖**：T301。

### T304 · 账户客户端 [P1] [US1] [US2] [US9]
- **状态**：**DONE 2026-11-11** · `WeaselServer/Cloud/CloudAccount.{h,cpp}`：状态机 `未登录 → 登录中 → 已登录 → 需重新登录`；`POST /v1/auth/login` 体 = `{email, auth_key, device:{id,name}}`，响应**消费 `email_verified`**；`DELETE /v1/devices/{device_id}`（传 `device_id` 而非库内自增 id）；401 ⇒ 自动 `POST /v1/auth/refresh` 后**重试一次**。**Done 判据部分达成（mock 传输全链路）**：`TestCloudAccountMock` 覆盖 登录成功 / 401 失败 / 空凭据回未登录（不崩）/ 设备列表 / 撤销设备 / 401→刷新→重试（请求序列 3 次）；**未达成**：真实端点调用 —— 依赖服务端部署。
- **目标**：注册 / 登录 / 续期 / 登出 / 设备列表 / 清空云端的客户端门面。
- **文件**：`WeaselServer/Cloud/CloudAccount.h`（新建）、`WeaselServer/Cloud/CloudAccount.cpp`（新建）。
- **做法**：状态机 `未登录 → 登录中 → 已登录 → 需重新登录`；续期失败自动重试一次，仍失败才落「需重新登录」。
- **Done 判据**：贴一次完整「注册 → 登录 → 续期 → 登出」的调用序列日志（含状态迁移）。
- **依赖**：T301、T302、T303、T212。

### T305 · 词库 / 短语导出导入协调 [P1] [US3] [US4]
- **状态**：**DONE 2026-11-11** · `WeaselServer/Cloud/CloudUserDictIo.{h,cpp}` + `CloudRimeUserDictBackend.cpp`（rime 后端，**只进 WeaselServer 工程**）。**实现偏差（原「导出 / 导入」已按 T102 证伪改写）**：导出 = `backup_user_dict`，还原 = `restore_user_dict`（**不用** `export_user_dict` / `import_user_dict`）；行格式 `<code>\t<phrase>\t<c=.. d=.. t=..>`，云端 `item_key = "<词典名>\t<词条>"`、`payload = "<code>\t<c=.. d=.. t=..>"`；多词典快照按 `#@/db_name` 分段串联。**互斥经 `RimeWithWeaselHandler` 的 `StartMaintenance` / `EndMaintenance`**（`CloudMaintenanceHost.h` 在 `WeaselServerApp` 构造期注入；未注册 ⇒ 云同步**明确失败**，不静默降级）。**已知局限**：快照通道**不传播墓碑** ⇒ **跨设备删除不保证生效**（阶段 G 加固项）。**Done 判据部分达成**：解析 / 生成往返、tick 取最大、多词典分段、维护态钩子注册与调用（`TestCloudUserDictIo`）；**未达成**：真机「导入期间输入不卡顿」的时序记录（需用户在场）。
- **目标**：在**进程内**导出与导入用户词库、短语，并与引擎读写互斥。
- **文件**：`WeaselServer/Cloud/CloudUserDictIo.h`（新建）、`WeaselServer/Cloud/CloudUserDictIo.cpp`（新建）。
- **做法**：① 复用既有维护态机制挂起引擎对该用户库的访问；② 导出 / 导入**在后台线程**完成，绝不在输入回调里做；③ 导出的条目化格式按 **T102 的取证结论**定（分隔符 / 字段数）。
- **Done 判据**：贴「导出 → 改文件 → 导入 → 词频符合 T102 结论」的证据 + 一条「导入期间输入不卡顿」的时序记录。
- **依赖**：T102、T303。

---

## 阶段 E · 账号页与文案

### T401 · 账号页对话框 [P1] [US1] [US9] [US12]
- **状态**：**界面层 DONE 2026-11-11**（`WeaselServer/Cloud/CloudAccountDialog.{h,cpp}` 已落地：普通 Win32 modeless 窗口 + `CloudAccountController` 后台工作线程；分区含 登录 / 登出 / 刷新 / 设备列表 / 撤销设备 / 清空云端数据 / 状态栏）；**入口接线未做**（见 T405）。**实现偏差**（原写「与 `QuickPanelDialog` 同栈（WTL）」）：仓库现状**无 WTL** ⇒ 改用**普通 Win32 modeless 窗口**（与 `AboutDialog` 同栈，不引新依赖）。**Done 判据部分达成**：错误口令的中文状态与「不进入登录态」有单测覆盖（`TestCloudAccountMock`）；**账号页截图（未登录 / 已登录）与真机联调未做**（依赖服务端部署 + 用户在场）。
- **目标**：账号页（分区：登录状态 / 邮箱 / 设备列表 / 同步开关与最近同步时间 / 立即同步 / 登出 / 清空云端）。**入口不再挂面板第 4 槽**（用户 2026-11-11 拍板：挂**快捷设置栏的右键菜单**）。
- **文件**：`WeaselServer/Cloud/CloudAccountDialog.h`（新建）、`WeaselServer/Cloud/CloudAccountDialog.cpp`（新建）。
- **做法**：普通 Win32 modeless 窗口（`AboutDialog` 同栈）；**密码框不回显**；口令用后清零；错误一律「状态 + 中文原因」，不弹打断式错误框；`login` 响应消费 `email_verified`。
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

### T405 · 账号入口接线 [P1] [US1]
- **状态**：**DONE 2026-11-11（批次 AC）**，**但真机判据未闭环**（见下）。入口位置按**用户 2026-11-11 拍板**：**快捷设置栏的右键菜单**，**不是**面板第 4 槽。右键菜单**不是被删除，而是换地方** —— 托盘语言栏项虽在批次 W 下线（ADR 0053），其右键菜单已迁到快捷设置栏品牌图标（ADR 0038），账号入口挂在**同一份菜单路由**上，一次同时覆盖品牌图标右键与托盘右键两个入口。账号页本身已可被程序调用：`fluxing::cloud::account_dialog::Show()`。
- **目标**：在**快捷设置栏的右键菜单**加一项「账号与同步」，点击打开账号页。
- **文件**：**实做与预估有偏差，按实做回写**：`WeaselServer/resource.h`（改，+2）、`WeaselServer/WeaselServer.rc`（改，CHS/CHT/ENU 三块各 +1 行 `MENUITEM`）、`WeaselServer/WeaselServerApp.cpp`（改，+17）。**原预估的 `WeaselServer/QuickPanelDialog.cpp` / `.h` 未改** —— 品牌图标右键与托盘右键共用 `IDR_MENU_POPUP`（ADR 0038），延用批次 L「快捷键设置」的落点先例即可达成同一效果，且零面板几何改动，比直接改 `QuickPanelDialog` 更贴合 spec 099 边界。
- **做法**：复用既有 `IDR_MENU_POPUP` 菜单路由；新增菜单项 ID `ID_WEASELTRAY_CLOUD_ACCOUNT 40024`（**不复用** `ID_WEASELTRAY_SYNC 40012`，后者承载 `WeaselDeployer.exe /sync` 的**本机**词库同步，挪用等于静默剥夺既有功能）；在 `SetupMenuHandlers()` 内注册 handler 调 `fluxing::cloud::account_dialog::Show()`，沿用 `m_uiThreadId` 判断 + `PostMessage(WM_COMMAND, MAKEWPARAM(40024, 0))` 弹回 UI 线程；**不改面板几何与状态真相源**（spec 099 边界）；**不新增任何 IPC 消息**（见 T402）；入口无需额外水化 —— 账号页自身已在控制器线程执行 `RestoreFromStore()`（`CloudAccountDialog.cpp:105-108`，`CloudTransport.h:10` 禁止 UI 线程调用），`Show()` 仅 `SetForegroundWindow` / `CreateWindowExW` ⇒ 不构成 constitution P2 阻塞 I/O。
- **Done 判据**：贴右键菜单出现「账号与同步」并能打开账号页的真机截图。本批已交付**代码 + 构建产物自证**，**真机截图仍待用户装机后人工点一次**（IME 自绘顶层窗跨进程抓不到，见记忆 #43）。
- **Verified（2026-11-11，均为实测）**：`build.bat weasel` ⇒ **0 个错误**（5 个既有 warning，日志 `tmp/T405-weaselserver-build.log`）；**产物二进制自证** `tmp/T405-verify-menu.ps1`（`LoadLibraryExW(LOAD_LIBRARY_AS_DATAFILE)` + `LoadMenuW(hMod, IDR_MENU_POPUP=105)` + `GetMenuStringW`）⇒ `id=40024 [Account and Sync (&Z)]` 在 `output\Win32\WeaselServer.exe`（2641920 B / SHA256 `094E54C1…CD50`）内**恰好 1 处**且顺序紧随 `40023 Shortcut settings (&B)`；CHS/CHT/ENU 三语言文案在 exe 原始字节中**各命中 1 次**；云单测 `tmp\build-fluxing-cloud-tests.cmd` ⇒ `RESULT: PASS=297 FAIL=0` / `ALL PASS`（**与批次 AB 基线逐字一致，无回归**）；新增行 clang-format 干净；`.rc` 保真 **UTF-16LE + BOM + CRLF**（410→413 行）。
- **Still not verified**：① 真机右键菜单截图（需装机 `scripts\install\silent-install.ps1` + 用户在场点一次）；② `Show() → 登录成功 → CloudSyncService 首次 pull` 的后两步 —— **Cloudflare 侧阻塞已于 2026-11-11 全部解除**（见 T407：D1 / R2 / `AUTH_PEPPER` / `wrangler deploy` / `api.aiec.fun` DNS 全绿，`/v1/health` 返回 200），当前唯一阻塞是**客户端侧真机联调**（需两台设备或两套隔离用户数据目录，见 T405 ①）。
- **依赖**：T401。

---

## 阶段 F · 同步引擎与调度

### T501 · 引擎骨架与调度 [P1] [US6] [US12]
- **目标**：后台线程 + 事件唤醒的调度器，实现 `plan.md` §7 的触发矩阵。
- **文件**：`WeaselServer/Cloud/CloudSyncEngine.h`（新建）、`WeaselServer/Cloud/CloudSyncEngine.cpp`（新建）。
- **做法**：启动拉一次（**不阻塞启动流程**，失败静默）；本地变更防抖 5 分钟后推（全局上限 15 分钟/设备）；每 60 分钟拉一次；手动「立即同步」；事件等待 + 定时器唤醒（**不使用轮询**）。
- **Done 判据**：贴调度日志（触发时刻 + 动作）+ 空闲 CPU 基线未破线的采样。
- **依赖**：T304。
- **状态**：**代码 DONE**（批次 AF，2026-11-11）；运行期日志与空闲 CPU 采样仍待真机（见「Still not verified」）。
- **Verified（2026-11-11，实测）**：`WeaselServer/Cloud/CloudSyncEngine.{h,cpp}` 新建并接入 `WeaselServer.vcxproj`（ClCompile `NotUsing` + 文件首行 `NOMINMAX`）；单后台线程 + `std::condition_variable`，`cv_.wait_until(deadline, pred{!running_||wakeup_})` ⇒ **事件唤醒 + 定时唤醒、无轮询**；调度用**单调钟**（steady_clock），墙钟只用于状态显示与 UTC 日配额翻篇；触发优先级 **手动 > 退避重试 > 启动 > 脏推 > 定时**；引擎与凭据解耦（注入 `SyncRunner` 回调，不持 token / enc_key / 端点）⇒ 未登录天然零出向请求（R9）。云单测 `tmp\build-fluxing-cloud-tests.cmd` ⇒ `RESULT: PASS=402 FAIL=0`（基线 324 ⇒ **+78**），含引擎 4 组用例 78 条断言（退避阶梯 / 日配额 / 自然日边界 / 推送截止 / 触发决策 / 运行时生命周期），**只 include 生产头、不复制第二份实现**（宪法 R7）；`build.bat weasel` ⇒ **0 错误 0 警告**；8 个生产文件 clang-format 逐行比对 `FORMAT_BAD_FILES=0`。
- **本轮修掉的 3 个生产缺陷**：① `last_push_ms_ / last_pull_ms_` 初值为 0 ⇒ 定时条件恒成立、`startup_delay_ms` 形同虚设 ⇒ 改用 `SteadyNowMs()` 初始化；② 退避阶梯第 4 档写成 `60 * 1000`（= 60 秒，比第 3 档 15 分钟还短）⇒ 改为 `60 * 60 * 1000`（单测已加**硬断言**钉死该值）；③ `kWait` 分支无条件把 `last_outcome` 覆盖成 `kSkipped` ⇒ 上一轮成功 / 失败结果被吞 ⇒ 改为仅 `kQuotaExceeded` 才改写。
- **本地改动钩子（批次 AG，2026-11-11，已接 —— 偏差消除）**：开工前核实工程事实推翻了原判断 —— **提交事件与同步引擎同在 `WeaselServer.exe`**（`WeaselTSF` 不链接 librime；`RimeWithWeaselHandler` 归 `WeaselServerApp` 所有）⇒ **不新增 IPC 消息**（不触发四边同步红线）。落点 = `_Respond` 的 `get_commit` 成功处（**全仓唯一**提交边界，读后即清 ⇒ 每笔提交恰好一次）；链路 = 提交边界 → `FluxingLocalChangeHook` 槽（实现放在 `RimeWithWeasel` 静态库内，依赖方向 **Cloud → Rime**，否则 `TestWeaselIPC` 会 LNK2019）→ `CloudSyncNotifier`（`shared_ptr` 持引擎 ⇒ 无 UAF；`StopEngine()` 先摘目标再停引擎）→ `CloudSyncEngine::NotifyLocalChange()`（只置标志 + 唤醒；网络 I/O 在引擎后台线程 ⇒ 输入零阻塞，宪法 P2）。另：`WeaselServerApp` 构造期即构造云账号控制器 ⇒ 云同步不再依赖「用户是否打开过账号页」（未登录仍零出向请求）。详见 `docs/adr/0056-local-change-hook-in-process-direct-link.md`。
- **批次 AG 门禁（实测）**：云单测 `RESULT: PASS=414 FAIL=0`（基线 402 ⇒ **+12**，F5 组断言覆盖槽的注册/通知/注销、宿主的挂载/摘除、`last_trigger == kLocalChange`、摘除后不再触发同步）；`build.bat weasel` ⇒ **0 错误 0 警告**（顺带修掉两处被本轮重编译暴露的**既有**警告：`WeaselServer/UserDictionary.h` 的 `class`/`struct` 不一致 C4099、`WeaselServerApp.cpp` 的 `_snwprintf` 弃用 C4996）；clang-format 12 个生产文件 `FORMAT_BAD_FILES=0` + 两个既有大文件的新增行 `FORMAT_NEWLINE_BAD=0`；`TestWeaselIPC.exe` 链接成功 ⇒ 依赖方向「Cloud → Rime」成立。
- **Still not verified（批次 AG 复评）**：① 运行期调度日志（触发时刻 + 动作）需真机（IME 进程内）；② 空闲 CPU 基线采样（T602 范畴）；③ **钩子端到端时序**（打一个字 → 防抖 5 分钟 + 推送全局间隔 15 分钟 → 云端可见）需登录态 + 线上账号 + 用户在场。

### T502 · 推送与拉取 [P1] [US3] [US5]
- **目标**：把变更条目推上去、把远端增量拉回来。
- **文件**：`WeaselServer/Cloud/CloudSyncEngine.cpp`（改）、`WeaselServer/Cloud/CloudUserDictIo.cpp`（改）。
- **做法**：推只带**变化条目**（一次推合并多条）；拉走 `since` 增量；单次同步数据量有上限，超限分批并给进度。
- **Done 判据**：贴一次「本地改配置 → 推 → 拉回一致」的日志。
- **依赖**：T501、T305、T213。
- **状态**：**代码 DONE**（批次 AF）；端到端「推 → 拉回一致」日志仍待真机双机。
- **Verified（2026-11-11，实测）**：落点在 `WeaselServer/Cloud/CloudSyncService.{h,cpp}`（`CloudSyncCoordinator::SyncOnce`），**不是** `CloudSyncEngine.cpp` / `CloudUserDictIo.cpp`（引擎只管「何时同步、同步哪一头」，真正的推拉在协调器里）—— 与任务书的文件清单有出入，以实做回写；分批常量 **推 200 条/请求、拉 100 条/页 × 最多 10 页/轮**，一轮内游标不前进即停手；同步顺序改为 **拉 → 合并 → 回写 → 推 → 侧写**；推送集合为空时**不发推送请求**（零写放大）；维护态已用 `StartMaintenance` / `EndMaintenance` 正确包裹（否则 leveldb LOCK 失败）。
- **本轮修掉的 1 个生产缺陷**：sidecar 语义错判 —— 原实现只看 `locally_modified` 就写 `synced=true`，导致**只拉不推的启动轮会把本地改动误标成「已同步」**，改动永远推不上去 ⇒ 改为 `push_indices`（本地下标）+ `std::vector<bool> pushed[]`，**只有本轮真的推成功的条目**才标 `synced=true`。
- **Still not verified**：「本地改配置 → 推 → 拉回一致」的端到端日志（需线上服务 + 真机账号态）。

### T503 · 合并与回传 [P1] [US8]
- **目标**：客户端解密远端密文 → 与本地按条目合并 → 加密回传，使两端收敛。
- **文件**：`WeaselServer/Cloud/CloudSyncEngine.cpp`（改）、`include/FluxingCloudEnvelope.h`（改）。
- **做法**：`config` 整份取 HLC 较大者；`phrases` / `userdb` 按条目键合并后**重新加密并回传**（新 HLC）。**条目的键与词频合并规则必须照 T102 的结论实现**，不得凭推测。
- **Done 判据**：贴「两端同时改不同条目 → 同步后两端为并集且一致」的日志（AC-08 / AC-09）。
- **依赖**：**T102 结论落盘**、T502。
- **状态**：**代码 DONE**（批次 AF）；AC-08 / AC-09 的「两端为并集且一致」仍需**真机双机**才能判定。
- **Verified（2026-11-11，实测）**：合并规则**直接复用** `include/FluxingCloudEnvelope.h` 的 `MergeEntries`（批次 X 的 T102 结论落点），**未在客户端另写一份**；HLC 比较用同一文件的 `CompareHlc`（ms → counter → device 字典序）。**服务端侧新增收敛守卫**（本批 worker 交付）：`cloud/fluxing-sync/src/sync.ts` push 逐条比对 —— 存储中的 HLC ≥ 入参 ⇒ **跳过**（不写 R2 / D1、不推进 seq），响应新增 `skipped` 字段；`src/wire.ts` 新增 `compareHlc` + `compareUtf8Bytes`（按 **UTF-8 字节序**比 device，与客户端一致）。服务端门禁：`npm run typecheck` 退出码 **0**；`npx vitest run` ⇒ **30/30 PASS**（基线 20 ⇒ +10）。
- **形状裁定（需用户知悉）**：服务端 `skipped` 实现为**字符串数组**（与既有 `accepted` 同构），**未**改成对象数组 —— 改 `accepted` 形状会破坏客户端既有契约。
- **Still not verified**：① AC-08 / AC-09 真机双机并集一致性；② **服务端加固已部署**（批次 AG，2026-11-11：`npx wrangler deploy` ⇒ 成功，`api.aiec.fun` custom domain，Current Version ID `0aaf933c-f832-4c64-a726-3570f32ed848`，绑定 D1/R2 `fluxing-sync`）⇒ 线上 `/v1/auth/resend` 已生效（未知邮箱 ⇒ HTTP 404 `email_not_found`）；**✅ 批次 AH 已用真实账号闭环实测通过**（2026-11-11，探针 `scripts/cloud/live-e2e-probe.mjs`，19/19 断言全 PASS）—— `push` 响应 `skipped`（同版本再推 ⇒ `skipped=["userdb\t…"]`）与 `pull` 顶层 `server_seq`（= 2）均线上验证通过，详见本文末「批次 AH 收尾记录」；③ **遗留风险（非本批引入）**：`restore_user_dict` 是「合并」语义（abs(commits) 择优、dee 取 max、tick 抬升），合并快照未必覆盖本地胜出的冲突条目。

### T504 · 降级与退避 [P1] [US6]
- **目标**：断网 / 失败时静默降级，不阻断输入、不产生重试风暴。
- **文件**：`WeaselServer/Cloud/CloudSyncEngine.cpp`（改）、`WeaselServer/Cloud/CloudTransport.cpp`（改）。
- **做法**：指数退避 1 / 5 / 15 / 60 分钟，最多 4 次后转「等待下一次定时」；**不弹窗、不阻塞输入**；断网期间不排队累积，恢复后按防抖窗口补推；每日推次数上限 100 / 拉 50。
- **Done 判据**：贴断网期间的重试序列日志（确认退避阶梯与无风暴）+ 恢复后自动补推的一次成功记录。
- **依赖**：T501。
- **状态**：**代码 DONE**（批次 AF）；断网重试序列日志与恢复补推仍待真机。
- **Verified（2026-11-11，实测）**：退避阶梯 **1 / 5 / 15 / 60 分钟**，最多 **4 次**，用尽 ⇒ `kBackoffExhausted` ⇒ 交回定时触发（不无限重试）；每日配额 **推 100 / 拉 50**，用尽 ⇒ **跳过（`kSkipped`，不是错误）**，手动同步**不受配额限制**；断网期间**不排队累积**（只记脏标记，恢复后按防抖窗口补推）；全程**不弹窗、不阻塞输入**（网络 I/O 只在引擎后台线程；**无 MessageBox**）。单测逐条覆盖：退避 attempt→档位映射、超阶梯取最后一档、attempt=0 下溢、空阶梯、推 / 拉配额边界与上限为 0、跨 UTC 自然日配额归零、手动压过退避且不受配额限、退避窗口内 `kWait` / 到期 `kRunBoth` / 用尽 `kBackoffExhausted`、推用尽但拉有余额只拉（`kRunPull`）。
- **Still not verified**：真机断网期间的退避序列日志 + 恢复后自动补推的成功记录（需用户在场断网）。
- **未改文件**：`CloudTransport.cpp` 本批**未改**（失败与降级逻辑落在引擎层，传输层保持「要么成功要么返回中文错误」，不引入重试语义）。

### T505 · 同步状态呈现 [P1] [US9] [US12]
- **目标**：账号页显示最近一次同步时间与结果、失败的中文原因。
- **文件**：`WeaselServer/Cloud/CloudAccountDialog.cpp`（改）、`WeaselServer/Cloud/CloudSyncEngine.h`（改）。
- **做法**：引擎暴露只读状态快照（最近同步时间 / 结果 / 失败原因文案），UI 轮询式刷新但**不做阻塞等待**。
- **Done 判据**：贴账号页上「最近同步：xx:xx，成功 / 失败（中文原因）」的截图。
- **依赖**：T501、T401。
- **状态**：**代码 DONE**（批次 AF）；账号页截图仍待用户装机（IME 自绘窗跨进程抓不到，见记忆 #43）。
- **Verified（2026-11-11，实测）**：`CloudSyncEngine` 暴露只读状态快照 `CloudSyncStatus{ever_synced, last_sync_ms, last_outcome, last_trigger, last_message, push_count_today, pull_count_today, backoff_step, next_attempt_ms, running, enabled}`（`status()` 加锁 ⇒ UI 线程可安全读）；账号页新增**「最近同步」静态行**（ControlId `kIdSyncStatus=1017`）与**「立即同步」按钮**（`kIdSyncNow=1016`），文案形如「最近同步：2026-11-11 20:15:31 · 成功（推送 3 条，拉取 7 条）」/「· 失败（中文原因）」；UI 刷新走 `PostMessageW(WM_APP_REFRESH)`，**不做阻塞等待**（宪法 P2）；时间格式化用 **Win32 FILETIME 链**（`FileTimeToLocalFileTime` → `FileTimeToSystemTime`），**刻意不用 CRT `localtime`**（进程级共享静态缓冲区，并发会互相踩）；`build.bat weasel` ⇒ **0 错误 0 警告**；clang-format 两文件干净。
- **本轮我方修正**：worker 初版「立即同步」直调 `RunSyncRound`、**绕过引擎** ⇒ 手动同步不进引擎状态机（`last_trigger` 不记 `kManual`）⇒ 改为**优先走引擎** `engine->RequestManualSync()`（引擎线程执行，网络 I/O 不在 UI 线程），引擎未启动时（未登录 / 恢复态失败）才退化为直接跑一轮并给出中文原因。锁序铁律：**先 `engine_mutex` / `sync_mutex`，后 `impl->mutex`**；析构顺序：**先停引擎（join 引擎线程）再停工作线程**，且 join 时**不持 `sync_mutex`**（否则与引擎线程互锁）。
- **偏差**：plan §8 的「同步开关（用户可关掉同步）」**未做** —— T505 任务书未要求，且需要新增持久化设置项；本批只做「状态呈现 + 立即同步」。
- **Still not verified**：账号页真实观感（480×472 布局、三个新控件是否遮挡）需装机截图。

---

## 阶段 G · 双机验收与发版

### T601 · 验收用例全跑 [P1] [US1] [US2] [US3] [US4] [US5] [US6] [US8] [US9] [US11] [US12]
- **目标**：逐条跑 AC-01 ~ AC-14，产出证据索引。
- **文件**：`docs/research/fluxing-cloud-2026-11/acceptance-matrix.md`（新建）、`tmp/`（证据，不入库）。
- **做法**：AC-02 / AC-03 / AC-04 / AC-05 / AC-08 / AC-09 / AC-13 用**两台设备或两套隔离用户数据目录**；AC-07 做抓包与存储抽查；AC-10 验「登出后本机数据仍在 / 清空后云端无可恢复内容」。
- **Done 判据**：14 条 AC 逐条写「通过 / 不通过 + 证据路径」，无「待定」。
- **依赖**：阶段 E、F 全绿。
- **状态**：**文档 DONE**（批次 AN，2026-11-11）—— `docs/research/fluxing-cloud-2026-11/acceptance-matrix.md` 已新建，14 条 AC 逐条给出「通过 / 不通过 + 证据路径 + 缺口」，**无「待定」**。
- **Verified（2026-11-11，实测）**：**通过 1 条（AC-12）**、不通过 13 条。结构性阻塞：**7 条**需第二台设备（AC-03/04/05/08/09/13/14）、**4 条**需登录态 + 用户在场（AC-01/02/06/10）⇒ 11 条在「单机 + 无人值守」下**无法**收敛到全绿；纯机端可推进的只剩 **AC-07**（D1 `sync_items.item_key` 明文存词条）与 **AC-11**（私有内存 138.5 MB ≫ 40 MB 基线），两条均需独立一批。
- **Still not verified**：AC-01 的真机最后一公里（用户亲手用原口令登录 `kizemo@163.com`；探针侧 verify→login 需 6 位码）；AC-02 需重启系统与服务后验自动续期；AC-06 需真机断网（按 exe 路径防火墙出站阻断）；AC-10 需真机点「清空云端数据」后回查 D1/R2。

### T602 · 性能三数字与输入延迟 [P1] [US10]
- **目标**：NFR-001 ~ NFR-003 未破线。
- **文件**：`docs/research/fluxing-cloud-2026-11/perf-baseline.md`（新建）。
- **做法**：按 ADR 0030 的命令测三数字（进程数 = 1 / 私有内存 < 40 MB / 空闲 CPU < 0.1%）；同步开启与关闭两组对照测输入延迟。
- **Done 判据**：贴两组实测数字 + 结论「未破线」。
- **依赖**：T601。
- **状态**：**文档 DONE（数字已贴）· 判据未达成**（批次 AN，2026-11-11）—— `docs/research/fluxing-cloud-2026-11/perf-baseline.md` 已新建，贴出 ADR 0030 口径下的三数字与结论。
- **Verified（2026-11-11，实测，装机版 `WeaselServer.exe` PID 257348，启动 15:45:17，未登录）**：进程数 **1** ✅；私有内存 **136.0 MB**（5 s 窗口）/**138.5 MB**（10 s 窗口）vs 基线 **< 40 MB** ⇒ ❌ **破线 3.4×**；空闲 CPU **0.039 %**（逻辑核折算，✅）/ **0.7804 %**（单核口径，⚠️）。
- **口径歧义（需 ADR 修订裁定）**：ADR 0030 决策 2 的判据栏写「占单核」，做法栏写「逻辑核折算」，两口径在 20 逻辑核机上差 20 倍；本机 20 核 ⇒ 折算口径未破线、单核口径破线。
- **Still not verified**：① **内存归因**未做 —— 装机版已含 spec 100 云模块，**无 spec 100 前对照基线**，故**不能断言**内存增长由云同步造成；需 VMMap/堆快照 + 不含云模块的对照构建；② 输入延迟两组对照未测（IME 自绘窗跨进程抓不到，且「同步开启」组需登录态）—— 仅有结构性证据（引擎独立后台线程、`NotifyLocalChange` 只置标志、推拉不在 TSF 回调内发起）。

### T603 · 合规核查 [P1] [US7] [US12]
- **目标**：未登录零出向请求；服务端存储抽查为密文。
- **文件**：`docs/research/fluxing-cloud-2026-11/compliance-check.md`（新建）。
- **做法**：未登录态全流程抓包计数 = 0（AC-12 / R9）；服务端 R2 对象抽查确认无明文词条 / 短语 / 配置。
- **Done 判据**：贴「未登录出向请求数 = 0」的抓包计数 + 一条 R2 对象内容抽查证据。
- **依赖**：T601。
- **状态**：**AC-12 通过 · AC-07 不通过 · 文档 DONE**（批次 AN，2026-11-11）—— `docs/research/fluxing-cloud-2026-11/compliance-check.md` 已新建。
- **Verified（2026-11-11，实测）**：未登录态 `Get-NetTCPConnection -State Established` 对 WeaselServer PID 257348 = **0 条**，全局到 Cloudflare 网段 = **0 条** ⇒ **AC-12 通过**。R2 侧：服务端只 `BUCKET.put(cipherBytes)`、键为 `u/<userId>/<itemKeyHashHex>` ⇒ **R2 里是密文**（代码级确认）。
- **❗ 本轮新发现（AC-07 的失败点）**：D1 `sync_items.item_key` **明文存放 `<词典名>\t<词条>`**（客户端上报原文，服务端未再加密）⇒ AC-07 字面判据「服务端存储无明文词条」在 **D1 侧不满足** ⇒ **AC-07 不通过**。协议里已有哈希形式（`itemKeyHashHex` 用于 R2 键）⇒ 存在低成本哈希化改法，但会动表结构 + 推拉/合并三处逻辑，属独立一批。
- **Still not verified**：R2 对象内容抽查**本轮无线上对象可抽查**（线上 `sync_items` 为空 ⇒ `ROWS=0`，因为探针闭环未完成 ⇒ 无推送 ⇒ 无对象）；抽查脚本 `tmp/AN-t603-r2-check.ps1` 已就绪（判据：长度 == `cipher_len`、CJK 命中 0、可打印 ASCII 占比 ≈ 0、十六进制头部随机），闭环后复跑即可出结论；传输侧抓包项未做。

### T604 · 门禁与发版 [P2] [US12]
- **目标**：门禁全绿后按发版流程出包。
- **文件**：`CHANGELOG.md`（改，如需补发版条目）、`docs/archive/handoffs/`（新建收尾 handoff）、`docs/archive/prompts/`（新建接力 prompt）、`CLAUDE.md`（改 §2 指针）。
- **做法**：跑 `scripts/gate/run-all.ps1 -Level L0`；改动 ≥3 文件加 L4；发版前 L5。注意：`WeaselServer.exe` 是 PPL ⇒ 装机前先注销 / 重启（R4）。
- **Done 判据**：贴门禁输出（全绿）+ 收尾 handoff 与接力 prompt 的路径。
- **依赖**：T601、T602、T603。
- **状态**：**门禁 + 收尾文档 DONE · 出包未做**（批次 AN，2026-11-11）。**命令更正**：本仓门禁总入口的开关是 `-SkipL0/-SkipL1/-SkipL3/-SkipCases/-SkipSandboxGate`，**没有 `-Level` 参数**（任务书原文 `-Level L0` 会报「找不到与参数名称 'Level' 匹配的参数」）⇒ 直接不带参数即跑 L0/L1/L3/L4/L5 全量。
- **Still not verified**：**未出包**（发版流程未启动；`WeaselServer.exe` 是 PPL ⇒ 装机前先注销 / 重启，R4）；接力 prompt 按用户 2026-11-08 指示**直接写在当轮回答正文**、不另落文件（`docs/archive/prompts/` 不新建）。

### T605 · 账号链路加固 · resend 端点 [P1] [US1]（原阶段 G 加固项，提前到批次 AF 实现）
- **目标**：验证码过期 / 用尽后用户不被锁死 —— 服务端补 `POST /v1/auth/resend`，客户端补「重新发送验证码」入口。
- **文件**：服务端 `cloud/fluxing-sync/src/{wire.ts,sync.ts,auth.ts,index.ts}` + `test/api.test.ts`（改）；客户端 `WeaselServer/Cloud/CloudAccount.{h,cpp}`、`WeaselServer/Cloud/CloudAccountDialog.{h,cpp}`（改）。
- **做法**：服务端按邮箱重发验证码（已验证邮箱 ⇒ 409 `email_already_verified`；不存在 ⇒ 404 `email_not_found`；过于频繁 ⇒ 429 `verify_too_frequent`）；客户端在账号页加「重新发送验证码」按钮（`kIdResend=1015`），**不清空验证码输入框**、失败时仍可改后重提。
- **Done 判据**：服务端用例覆盖 + 客户端按钮可用 + 线上 `POST /v1/auth/resend` 返回 200。
- **依赖**：T405、T406。
- **状态**：**代码 DONE**（批次 AF，2026-11-11）+ **线上已生效**（批次 AG，2026-11-11 已 `wrangler deploy`，见下方实测）。
- **Verified（2026-11-11，实测）**：服务端 `src/auth.ts` 新增 `handleResend`、`src/index.ts` 新增路由 `case '/v1/auth/resend'`；`npm run typecheck` 退出码 **0**、`npx vitest run` ⇒ **30/30 PASS**（基线 20 ⇒ **+10**，其中「已验证邮箱 ⇒ 409」「不存在 ⇒ 404」「过于频繁 ⇒ 429」齐备）；客户端 `CloudAccountService::ResendVerification(email, err*)`（**不改状态机**）已实现，错误走 `ExtractServerError` 给中文原因；账号页新增「重新发送验证码」按钮（`kIdResend=1015`，仅未登录态可用），口令 / 验证码仍走 `ReadControlText` + `SecureZeroWide`，**全程无 MessageBox**；`build.bat weasel` ⇒ 0 错误 0 警告。
- **批次 AG 线上实测（2026-11-11）**：`npx wrangler deploy` ⇒ 成功（`api.aiec.fun`，Current Version ID `0aaf933c-f832-4c64-a726-3570f32ed848`）；`GET /v1/health` ⇒ **HTTP 200** `{"ok":true,...}`；`POST /v1/auth/resend`（未知邮箱）⇒ **HTTP 404** `{"error":{"code":"email_not_found","message":"该邮箱尚未注册，请先注册。"}}` ⇒ **端点线上已生效**（探针须用 `Invoke-WebRequest -SkipHttpErrorCheck`，PS7 下 `Exception.Response.GetResponseStream()` 不存在）。
- **Still not verified**：① **真实邮箱的收码**（`MAIL_MODE="outbox"` 不发真信 ⇒ 用户真实邮箱收不到信；**但闭环本身已通** —— `mail_outbox.body` 存的是**明文验证码正文**，探针据此自动取码，线上 `register → verify → login` 实测通过，见本文末「批次 AH 收尾记录」）；② ~~D1 `email_verifications` 落库复核~~ ⇒ **✅ 已复核**（探针内核对落库）；③ ~~`push` 响应的 `skipped` 与 `pull` 顶层 `server_seq` 需合法 `auth_key` 的真实账号才能验~~ ⇒ **✅ 已验证**（批次 AH，线上 19/19 断言全 PASS）。

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

---

## 批次 AD 收尾记录（2026-11-11 · 托盘「中/英」图标 + 阶段 E 入口）

> 按实现回写（宪法 P3 文档治理）。批次 AD **零 C++ 改动**，只补诊断脚本 + 部署闭环。

### T405 · 状态确认（已 DONE，不重做）
- **代码**：HEAD `233dacb`「feat(WeaselServer): 右键菜单新增「账号与同步」入口 (spec 100 T405)」—— 3 文件 / +19 行（`WeaselServer/resource.h`：`ID_WEASELTRAY_CLOUD_ACCOUNT 40024`；`WeaselServer/WeaselServer.rc`：CHS/CHT/ENU 各 +1 MENUITEM；`WeaselServer/WeaselServerApp.cpp::SetupMenuHandlers()`）→ `fluxing::cloud::account_dialog::Show()`。
- **门禁**：`build.bat weasel` 0 错误；产物 `LoadMenuW(105)` 自证 40024 恰好 1 处且顺序正确；云单测 `PASS=297 FAIL=0`；clang-format 干净。
- **未闭环**：**真机截图**（IME 自绘窗跨进程抓不到）—— 需用户在场点一次托盘 / 快捷设置栏品牌图标的右键菜单，确认「账号与同步 (&Z)」出现并能打开账号页。

### T406 · 托盘「中/英」图标去除 —— 根因第四次定位 + 部署闭环
- **目标**：批次 J / W 两轮修法先后被证伪后，把通知区域的方框「中」/「英」彻底去掉。
- **根因（最终）**：`include/FluxingLangBarStyle.h` 的 `kLangBarRegisterItem = false` 修复**正确且已上机**；复现是因为 **Windows 已加载映像不随磁盘文件替换而更新** —— `install.nsi` 在 DLL 被在跑进程占用时走 locked 分支，**只写 versioned shim、裸名文件原样留盘（仍是旧字节）**，而注册表早已指向修复版 shim ⇒ 旧字节被仍在跑的宿主加载 ⇒ 旧注册逻辑复现。
- **证据**：注册表双视图 → 当前构建产物（`weaselx64.1.2.1.3.1.dll` sha16 `AB623A41EE447C5D` / `weasel.1.2.1.3.0.dll` sha16 `103802069CA84E7C`）；复现源 = 前台宿主 `msedge`(212880) 映射的裸名 `weaselx64.dll`（sha16 `2024059039231556`，mtime 11-10 12:24:30，**早于**修复编译 13:05:08 ⇒ 物理上不可能含修复）；活动 IME 切到搜狗拼音 / 美式键盘时 `OUR_HIT=0`、切回火流猩 ⇒ 1；新启动的 `notepad`（映射 fresh shim）成为前台**不能**清掉旧项 ⇒ CTF 不随宿主切换注销语言栏项。
- **做法**：① 重建 x86 + x64 两条 TSF 链（批次 W 只重建 x64，是部署缺口）；② NSIS 打包；③ 静默装机 `ALL HARD CHECKS PASSED`；④ 把裸名 `weaselx64.dll` / `weasel.dll` 覆盖为当前构建产物（`match=True`，备份 `*.adv-20261110-204207.bak`）；⑤ 新增 `scripts/install/check-tray-input-indicator.ps1`（五段诊断 + 退出码 0/1/2）。
- **Done 判据**：`check-tray-input-indicator.ps1` 输出 `OUR_HIT=0` 且 `VERDICT=PASS_OUR_ITEM_ABSENT`。
- **未闭环（需用户）**：6 个进程仍持有修复前镜像（msedge / Weixin / DingTalk / Claude Code Haha / **MiniMax Code（本会话宿主）** / YXCalendar）⇒ 需结束它们或注销重登后再复测。**代码侧已无可做之事。**

### T407 · 阶段 E 线上判据 —— **DONE 2026-11-11（Cloudflare 侧全绿）**
- **目标**：补齐 spec 100 线上判据（D1 / R2 / AUTH_PEPPER / deploy / DNS）。
- **Done 判据（逐条实测命中）**：
  ① `GET https://api.aiec.fun/v1/health` ⇒ **HTTP 200** `{"ok":true,"service":"fluxing-sync","environment":"dev","time":1794319132}`；
  ② `d1 migrations apply fluxing-sync --remote` ⇒ 13 条命令全绿，`sqlite_master` **9 张表**（`_cf_KV` / `d1_migrations` / `users` / `devices` / `refresh_tokens` / `sync_items` / `email_verifications` / `mail_outbox` / `sqlite_sequence`）；
  ③ R2 桶 `fluxing-sync` 存在（`r2 bucket create fluxing-sync` ⇒ `Created bucket 'fluxing-sync'`，绑定 `env.BUCKET`）；
  ④ `AUTH_PEPPER` 存为 **Workers Secret**（`secret list` ⇒ `type: "secret_text"`，非明文 var）。
- **附加功能判别（不只信元数据自证）**：
  · 只读判别：`POST /v1/auth/login` 打一个不存在的用户 ⇒ **401 `invalid_credentials`**（若 pepper 缺失，此处必为 500 `server_misconfigured`）⇒ pepper 真实加载；
  · 写入判别：`POST /v1/auth/register` 打一个冒烟用户 ⇒ **200** 返回 `account_id` / `salt` / `kdf{PBKDF2-SHA256, 600000}` / `need_email_verify:true` ⇒ D1 落库 `users+1`、`email_verifications+1`、`mail_outbox+1`（$0 路线 outbox 只入库不发信）；冒烟数据随后按 `smoke-%@aiec.fun` 清空，复核 `users=0 / verifications=0 / outbox=0`。
- **执行留痕**：`wrangler deploy` ⇒ `Current Version ID c22a483d-0460-4d17-b46b-ac3347c2c138`、`api.aiec.fun (custom domain)`、Total Upload 42.65 KiB / gzip 11.45 KiB；`workers_dev=false` + `preview_urls=false` ⇒ **无 workers.dev 公网入口**；`npm run check`（端点策略 + vitest 20/20）与 `npm run typecheck` 在部署前已全绿。
- **仍未做**：`MAIL_MODE="outbox"` 是用户 2026-11-11 拍板的 $0 路线结果，真实发信（`MAIL_MODE="cloudflare"` + `[[send_email]]`）**未启用**；`ENVIRONMENT="dev"` 未切 `production`；`AUTH_PEPPER` 值只存在于 `tmp/AD-auth-pepper.txt`（gitignored）与 Workers Secret **两处**，未入库、未进仓。
- **历史阻塞（已解除）**：曾因 `wrangler whoami` ⇒ `You are not authenticated` + `wrangler.toml` 的 `database_id` 为占位全零 UUID ⇒ 线上判据 not verified；2026-11-11 实测 **OAuth 早已成功**（`kizemo@163.com`，account `016c8e736b685af88860034ea6eb1daa`，scope 含 `d1/workers/workers_scripts/workers_routes/zone:read` 等），此前「授权失败」结论系**过期观察**，已就地更正。

### T408 · 客户端「注册 + 邮箱验证码」链路 —— **DONE 2026-11-11（批次 AE）**

- **背景缺口（坐实）**：阶段 D 只实现了 `Login` / `Refresh` / `Logout`。客户端**完全没有**注册与邮箱验证码链路 —— `CloudAccountService` 无 `Register` / `VerifyEmail`，`CloudAccountController` 无对应 `Submit*`，全仓 `grep -E "register|verify"` 零命中（排除 `RegisterClassExW` 等无关项）。服务端 `need_email_verify:true` 因此没有任何客户端路径能走完。
- **服务端契约（只消费，未改）**：`POST /v1/auth/register` body `{email, auth_key, device:{id,name}}` → `{account_id, salt, kdf:{algo,iters}, need_email_verify:true}`；已注册 ⇒ `409 email_taken`「该邮箱已注册，请直接登录。」。`POST /v1/auth/verify` body `{email, code}` → `{ok:true}`；`code` 必须 `/^\d{6}$/`，错误码 `verify_no_code|verify_code_used|verify_code_expired|verify_code_locked|verify_code_mismatch`。**服务端无 resend 端点**（`route()` 仅 9 条 + `/v1/health`）。
- **改动（5 文件）**：
  · `WeaselServer/Cloud/CloudAccount.h`（+17）：新增 `Register(email, password, out_account_id, err)` 与 `VerifyEmail(email, code, err)` 声明。
  · `WeaselServer/Cloud/CloudAccount.cpp`（+134）：`Register` @308 —— 复用 `NormalizeEmail` → 本地校验（空 / 缺 `@` / 缺 `.` / 空口令）→ `DeriveKeys` → `SecureZeroString` 清口令副本 → 读 `info_.device_id`（空则 `GenerateDeviceId` 并写回，保证「注册→验证→登录」服务端看到同一设备）→ `SendJson("POST","/v1/auth/register", …, need_auth=false)` → 解析 `account_id`；失败路径清零 `auth_key`/`enc_key`；**全程不写 store、不动 `state_`**。`VerifyEmail` @401 —— 本地校验 6 位纯数字，否则写「验证码格式不正确（应为 6 位数字）。」且**不发请求**。
  · `WeaselServer/Cloud/CloudAccountDialog.h`（+6）：`SubmitRegister` / `SubmitVerifyEmail`，注释声明 PBKDF2 与网络都在工作线程（宪法 P2：TSF 回调禁阻塞 I/O）。
  · `WeaselServer/Cloud/CloudAccountDialog.cpp`（+138/−51）：`ControlId` 追加 `kIdVerifyCode=1012` / `kIdRegister=1013` / `kIdVerify=1014`；客户区 `460×320 → 480×410`，控件表整体重排（邮箱 / 密码 `ES_PASSWORD` / 验证码 `ES_NUMBER` / 状态框 / 列表 / 底部按钮行）；新增 `ReadControlText`（读后 `SecureZeroMemory`）与 `SecureZeroWide`；`RefreshUi` 追加「未登录才可注册/验证」；`WM_COMMAND` 取值后清口令副本并清空密码框，验证码失败后**保留输入框内容**便于改后重提；注册成功提示「注册成功：请在验证码框输入 6 位验证码完成邮箱验证。」且**不置 `logged_in`**。
  · `test/TestFluxingCloud/TestFluxingCloud.cpp`（+84）：`TestCloudAccountMock` 内新增块 ⑦⑧。
- **设计定案**：**注册 ≠ 登录** —— `Register` 成功不落凭据、不改状态机，必须 `VerifyEmail` + `Login` 才拿令牌，与服务端 `need_email_verify` 语义一致，避免半登录脏态（单测用 `state()==kLoggedOut` + `store.Load` 失败双重坐实）。非法邮箱 / 非法验证码一律**本地拒掉不发请求**，与服务端正则对齐。
- **Done 判据（逐条实测）**：① 云单测 `RESULT: PASS=324 FAIL=0`（基线 297 ⇒ **+27**）、`VERDICT: ALL PASS`、`TEST_EXIT=0`；② `build.bat weasel` ⇒ `WEASEL_EXIT=0`、0 错误、产物 `WeaselServer.exe` sha16 `6CA1628D290D3DC8`；③ 4 个生产文件 clang-format 18.1.8 `diffLines=0`；④ 产物新鲜度 `CloudAccount.obj` / `CloudAccountDialog.obj` mtime 均晚于源文件（`FRESH=True`）。
- **未闭环（需用户在场）**：账号页真机注册 → 收码 → 验证 → 登录的端到端 —— `MAIL_MODE="outbox"` 是 $0 路线，验证码只进 D1 `mail_outbox` / `email_verifications`（后者存 `code_hash`），**明文码无法直读**；真机截图亦需人工（IME 自绘顶层窗跨进程抓不到，见记忆 #43）。
- **阶段 G 加固项（原「本批不做」，已于批次 AF 补齐）**：服务端缺 **resend** 端点 ⇒ 验证码过期 / 次数用尽后无恢复路径（再 `register` 会 409 `email_taken`）⇒ 用户被锁死。**批次 AF 已实现**：服务端 `POST /v1/auth/resend`（`src/auth.ts` + `src/index.ts`，+10 用例，服务端 30/30 PASS）+ 客户端 `CloudAccountService::ResendVerification` + 账号页「重新发送验证码」按钮 ⇒ 见上文 **T605**。**剩余缺口 = 真实账号端到端验证**（部署已于批次 AG 完成：`api.aiec.fun` / Version ID `0aaf933c-f832-4c64-a726-3570f32ed848`；`MAIL_MODE="outbox"` 下验证码明文不可直读，收码闭环需人工取码）。
  **【批次 AL 更新 2026-11-11】** 服务端已实现 `MAIL_MODE="http"`（Resend 兼容的第三方 HTTPS 邮件 API）+ 12 条单测 + 4 处变异有牙 + ADR 0058 + 用户侧落地指南 `docs/ops/fluxing-cloud-resend-setup.md` + 一键上线脚本 `scripts/cloud/resend-go-live.ps1`。**生产 `MAIL_MODE` 仍为 `outbox`**（线上行为与部署前一致；新代码已部署 Version ID `2e6db488-da38-4a3a-8ec7-7f302b167035`，health 200） ⇒ 待用户交付 Resend API Key + 在 Cloudflare 手动加域名 DNS 记录（现有凭据无 DNS 写权限）后切换。⚠️ 切 http 后 **http 模式不写 `mail_outbox`** ⇒ 取码来源须从 D1 改为**用户邮箱**，`scripts/cloud/live-e2e-probe.mjs` 的取码步骤同步失效待改。
- **交接文档**：`docs/archive/handoffs/handoff-2026-11-11-batch-ae-cloud-register-verify-chain.md`。

---

- **批次 AM 更新（2026-11-11 20:45）**：**发信通道已真正上线** —— DNS 由用户在 Cloudflare 手动加 3 条（CNAME `rsend`/`send` → `*.forge.rmta.net` **DNS only 灰云** + TXT `_dmarc`；DKIM 先前已在位）并交付 API Key ⇒ `secret put MAIL_HTTP_API_KEY` → `MAIL_MODE` **outbox→http** → 部署 **Version `29d4e572-49d0-43c2-b091-dda3029edf00`** → health 200 → 直调 Resend 自检信 **200**（id `01a1ca9f-…`）；**服务端 E2E = register 200 且返回 account_id**（证明验证码确经 Resend 发出），「mail_outbox 取码」FAIL 即模式判别器；探针新增 `FLUXING_PROBE_EMAIL` / `FLUXING_PROBE_CODE`；`.key/` 已补进 `cloud/fluxing-sync/.gitignore`。**未闭环**：完整「验证→登录→推送→拉取」闭环缺一个真实信箱 6 位码 + 客户端端到端注册（需未注册过的邮箱）。
## 批次 AH 收尾记录（2026-11-11 · 线上真实账号闭环 + item_key TAB 契约）

> 本批**不改客户端运行时**；改的是**服务端一行字符集校验** + **新增线上闭环探针** + **补跨实现固定向量**。线上判据首次闭环。

### AH-1 · 线上闭环探针（新增 `scripts/cloud/live-e2e-probe.mjs`）
- **背景**：阶段 F / T605 一直记「`skipped` 与顶层 `server_seq` **需真实账号**」⇒ 因为 `MAIL_MODE="outbox"` 下 `email_verifications` 只存 `code_hash`、**明文码不可直读**。**本批推翻该前提**：`mail_outbox.body` 存的是**明文验证码正文**（形如「您的火流猩输入法账号验证码是：123456」）⇒ 闭环**可全自动**，不需用户在场取码。
- **做法**：Node 标准 crypto（OpenSSL）**独立复刻**客户端的 KDF + GCM 布局（不引第三方依赖，宪法 P3）⇒「客户端自实现 == 标准实现 == 线上服务端」三方一致性可证；流程 = 健康检查 → D1 可读 → 派生 → `register` → 从 `mail_outbox` 取明文码 → `verify` → `login` → `push` 首轮（断言 `accepted`）→ 同版本再推（断言 `skipped`）→ `pull`（断言顶层 `server_seq`、条目 `server_seq`、密文可解密且明文一致、密文 ≠ 明文）→ D1 `sync_items` 无明文 → R2 对象逐字节一致且无明文 → 自动清理测试账号（`--keep` 可保留）。
- **Verified（2026-11-11，实测）**：**19/19 断言全 PASS、退出码 0**；`register` 200 返回 `account_id`；`verify` 200；`login` 200 拿到 `access_token`（`email_verified=true`）；`push` 首轮 `accepted=["userdb\t…"]`、`server_seq=2`；同版本再推 `skipped=["userdb\t…"]`；`pull` 顶层 `server_seq=2`、条目 `server_seq=2`；R2 对象 43B 与 pull 密文**逐字节一致**且不含明文；测试账号已自动清理。
- **工具坑（已踩）**：`wrangler r2 object get` **默认读本地存储** ⇒ 必须 `--remote`；写 stdout 用 `--pipe`（`--file -` 会建一个名为 `-` 的文件）；Windows 下 `execFileSync('npx', …)` ENOENT ⇒ 用 `process.execPath` + `node_modules/wrangler/bin/wrangler.js`。

### AH-2 · 真机生产缺陷：`item_key` 带 TAB 被 400 拒绝（ADR 0057）
- **根因**：客户端 `item_key` 恒为 `"<词典名>\t<词条>"`（`WeaselServer/Cloud/CloudUserDictIo.cpp` 的 `MakeDictItemKey`，客户端既有契约，sidecar 另有 TAB 转义），而服务端 `cloud/fluxing-sync/src/wire.ts` 的 `assertHeaderString` 用 `/[\u0000-\u001f\u007f]/` **一律拒 C0 控制字符** ⇒ **真机每一次 userdb / phrases 推送都会被 400 拒绝**（用户侧表现为「云同步一直失败但没有明确原因」）。
- **修法（最窄）**：服务端正则 → `/[\u0000-\u0008\u000a-\u001f\u007f]/`（**放行 TAB 0x09**，其余控制字符含 `\r` `\n` 仍拒）；**不动客户端、不迁数据、不改 `accepted` / `skipped` 形状**；服务端回归用例 +2（TAB 放行 + 换行仍拒）。
- **Verified（2026-11-11，实测）**：服务端 `npm run check` 退出码 **0**、vitest **30/30 PASS**；`npm run deploy` ⇒ 成功，`api.aiec.fun`，**Current Version ID `c18cc111-50c4-4b84-bb52-bcfd6c150807`**；同一探针修复前后实测 **400 → 200**。
- **详见** `docs/adr/0057-item-key-tab-separator-contract.md`。

### AH-3 · 跨实现固定向量（客户端单测 +4）
- **动机**：线上 register → login **证明不了「C++ 派生 == Node 派生」** —— 服务端只比 HMAC(pepper, auth_key)，**从不校验口令**。
- **做法**：用 Node / OpenSSL 独立算出 salt / auth_key / enc_key 后钉进 `test/TestFluxingCloud/TestFluxingCloud.cpp`（email `vector@example.com` / pw `Vector-Pw-1` / 生产迭代 600000）。
- **Verified（2026-11-11，实测）**：云单测 `RESULT: PASS=418 FAIL=0`（基线 414 ⇒ **+4**），4 条跨实现向量断言全 PASS；`build.bat weasel` ⇒ **0 警告 0 错误**；clang-format 新增行区间 `1126..1147` ⇒ `NEW_LINE_VIOLATIONS=NONE`（该文件另有 160+ 处**既有**历史违规，按批次口径只管新增行）。
- **Still not verified**：**没有任何真实 Windows 客户端用同一 `auth_key` 登录线上账号的端到端证据**（探针用的是独立复刻的 Node 实现，不是 C++ 客户端）—— 需装机 + 用户在场。

### AH-4 · 本批未闭环（汇总）
- ① 真实 Windows 客户端端到端登录线上账号；② 双机 / 双 Windows 账号验收（`CloudCredentialStore` 是 Windows 用户级全局单例 ⇒ 同用户两套目录**不够**）与 AC-08 / AC-09 两端并集一致性；③ 账号页真机截图（IME 自绘窗跨进程抓不到）；④ 托盘 `OUR_HIT=0` 复测（T406 遗留，需结束持有旧映像的进程或注销重登）；⑤ 客户端 `Push` **不消费 `skipped`**（不在本批验收口径，记为遗留风险项）。
- **生产库副作用**：探针会在生产 D1（`ENVIRONMENT=dev`）注册真实账号行；脚本末尾自动清理 6 张表，三次运行均已清理成功；`--keep` 可保留供排查。

### AH-5 · 交接文档
- `docs/archive/handoffs/handoff-2026-11-11-batch-ah-live-e2e-probe-and-item-key-tab-fix.md`（接力 prompt 按 §9「one file per spec + day, updated in place」就地改写 `docs/archive/prompts/prompt-next-session-2026-11-11-batch-ad-tray-langbar-residue.md`，未新建重复文件）。

---

## 批次 AI 收尾记录（2026-11-11 · 快捷设置栏右键菜单「变小」根因 + DPI 对齐修复）

> 用户报告：「快捷设置栏的右键菜单，变得非常小，文字也比之前缩小很多，可视度降低。请恢复之前的右键菜单大小设置。」
> 本批**不改云同步链路**、**不新增 IPC 消息**、**不改菜单项**、**不做自绘菜单**、**不动 manifest**；
> 唯一生产改动 = `WeaselServer/SystemTraySDK.cpp` 一个文件。批次 AH 成果一律引用、未重做。

### AI-1 · 根因（verified，判据独立成立）
- 弹出链 = `QuickPanelDialog.cpp:1752-1767` → `WeaselServerApp.cpp:319-325`（**ADR 0038 共用回调** ⇒ 一处修覆盖「品牌图标右键」+「托盘右键」两条入口）→ `SystemTraySDK.cpp::ShowPopupMenuAt`（`LoadMenu` → `GetSubMenu(0)` → `SetMenuDefaultItem` → `CustomizeMenu` → `SetForegroundWindow` → `TrackPopupMenu(hSubMenu, ..., hTargetWnd, NULL)` → `PostMessage(WM_NULL)` → `DestroyMenu`）。
- **全仓 `OWNERDRAW` / `WM_MEASUREITEM` / `WM_DRAWITEM` 在生产菜单路径零命中** ⇒ 这是**系统资源菜单**，仓库里**不存在「菜单字号」可调项**；「改配置恢复字号」是死路。
- 实测：面板窗 `GetDpiForWindow=144`（150%），但**同进程内三个隐藏/Popup 属主窗 `dpiWin=96`**（`tmp\AI-dpi-probe.txt`）；菜单 **20 项 / 内框 222×407 ⇒ 20.35 px·项⁻¹ = 96 逻辑行高** ⇒ 行高与字号按 96 渲染，在 150% 屏上「整体缩小 2/3」。
- 已排除：owner 近期未改（`git log --since=2026-11-06 -S 'SetTargetWnd' -- WeaselServer/` **空**）；manifest 近期未改；AppCompat `Layers` 无条目；装机 exe 内嵌清单 `dpiAware=True` / `dpiAwareness=False`（**PMv1**）；进程启动晚于文件替换 ⇒ 排除「跑旧映像」。

### AI-2 · 修法（唯一生产改动）
- `ShowPopupMenuAt` **弹出前**把 **target 窗 + 当前线程** 的 DPI context 对齐到「弹出点所在显示器」，`TrackPopupMenu` 返回后**反向还原**（先 thread 后 window）⇒ **长期 DPI 状态零残留**。
- DPI API 经 `GetModuleHandleW(L"user32.dll")` + `GetProcAddress` **运行时解析**（沿用 `QuickPanelDialog.cpp:75` / `WeaselPanel.cpp:24` 既有范式）⇒ 不引头类型、不新增导入库，**老 Windows 自动退化为原行为**。

### AI-3 · Done 判据（逐条实测）
- 修前 `238×423 / 20 项 / 内框 20.35 px·项⁻¹` → 修后 **`334×650 / 20 项 / 外框 32.50 px·项⁻¹`（内框 ≈31.70）**，高度比 **1.537 ≈ 系统缩放 150%**；`PROBE_EXIT=0`；截屏目视文字恢复正常字号 ⇒ **`VERDICT=PASS_MENU_DPI_RESTORED`**。
- `build.bat weasel` 连编两次 **0 警告 0 错误**（`BUILD_EXIT=0`）；clang-format 18.1.8 首轮 `TOTAL_HUNKS=3` 且 hunk 行号 **773/777/789 全落在新增区** ⇒ 局部 `--lines=770:860` 后复验 **`RECHECK_TOTAL_HUNKS=0`**，字节校验 `LF=1156 CR=0` ⇒ **LF 保真、无 BOM**。
- 装机 = 备份改名 + `File::Copy` + SHA 双证 + 重启（**不跑 NSIS 安装器**，本仓 install.nsi 冒烟验收已作废）：`sha16 094E54C18D534F5C → 1B0EE23CD2531A7E`、**`MATCH=True`**、旧件留盘 `WeaselServer.exe.old-261111-101226`、新 pid **214300**；只替换 `WeaselServer.exe`、**不动 TSF DLL** ⇒ 无需重启宿主应用。

### AI-4 · 本批收尾与剩余未闭环项
- ① 未提交 / 未 push —— **已闭合**：提交 `110887f`（`fix(WeaselServer): 快捷设置栏/托盘右键菜单恢复 DPI 缩放尺寸（批次 AI）`，6 files / **+271 −7**）；**仍未 push**（未获授权）。
- ② **已闭合**：事故条目 **`L211`**（`F:/soft/00selfmade/rime_claude/.specify/memory/lessons-learned.md:14631`）+ `CHANGELOG.md:3` 首位插入批次 AI 条目；本批属 bugfix 而非架构决策 ⇒ 按 §3 分流应入 lessons-learned，**不做 ADR**；云单测回归 `PASS=418 FAIL=0` 与批次 AH 基线逐字一致。
- ③ **用户目视复验**：`Alt+,` 面板品牌图标右键（几何判据来自 `GetMenuItemRect`，`PrintWindow` 抓 Popup 只画出上半 ⇒ 截图仅作字号佐证）；**托盘图标右键未单独复测**。
- ④ **备选方案未启用**：若复验仍有偏差 ⇒ `PerMonitorHighDPIAware.manifest` 升 **PerMonitorV2**（影响全进程所有窗口，需重编重装重验，风险更大）。

### AI-5 · 交接文档
- `docs/archive/handoffs/handoff-2026-11-11-batch-ai-quickpanel-menu-dpi.md`（含根因/证据路径/环境事实/工具坑/需用户在场的项）；接力 prompt 按用户 2026-11-08 指示**直接给在当轮回答正文**，不另落文件。