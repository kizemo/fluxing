# CLAUDE.md — Fluxing 进站指引

> WeaselServer / 小狼毫 的 RIME 前端 fork;**Fluxing / 火流猩输入法** 品牌分支,
> 安装路径 / 用户数据 / CLSID 均替换, 与上游 `rime/weasel` 互不冲突。
>
> 本文件**只承载**"删除后 Claude 会做错" 的导航与规则。其余一律改为链接, 不复制。

---

## 1. 进站阅读 (5 份, in order)

1. [`CONTEXT.md`](CONTEXT.md) — 身份 / 模块 / 词汇 / 不变量
2. [`AGENTS.md`](AGENTS.md) — 操作手册, 首屏 (§1) + 危险区 (§4) + checklist (§5)
3. [`.specify/memory/constitution.md`](.specify/memory/constitution.md) — 法则层, 含 R1–R9 / P1–P8
4. [`.specify/memory/project-knowledge.md`](.specify/memory/project-knowledge.md) §A 与 §11 — 部署 + NSIS 配方
5. [`docs/agents/domain.md`](docs/agents/domain.md) — 工程技能消费契约

按需再读:
- `docs/agents/issue-tracker.md` · `docs/adr/` · `.specify/specs/NNN-*/spec.md`
- [`Rime-设置说明书-重点快捷键.md`](Rime-设置说明书-重点快捷键.md) — 改 `output/data/**/*.yaml` (schema/custom/key_binder/editor/ascii_composer/switcher/punctuator/recognizer/translator/app_options) 时必读;重点章节 §5–§7、§9(本机现状)、§10(常见坑)

---

## 2. 必须强制 (删除就出错)

- **必查图谱**: 探索任何源码前, 用 `codebase-memory-mcp` (`F-soft-00selfmade-rime-claude`,
  12,715 节点 / 38,206 边);顺序 `search_graph → trace_path → get_code_snippet → query_graph → get_architecture`。
  降级到 Grep / Read 的条件在 [`.claude/rules/codebase-graph.md`](.claude/rules/codebase-graph.md)。
- **必须中文**: 用户可见字符串保持简体中文 (constitution P2/P5)。
- **必须 IPC 四边同步**: `include/WeaselIPCData.h` 与协议的任何变更 → server / TSF / deployer 三端 + 一个 ADR + 一个 `test/TestWeaselIPC/*` 测试, 一次性提交。
- **必须不开 TSF 阻塞 I/O** (P2): `ITfTextInputProcessor::*` 回调内禁止 `CreateFile` / 同步注册表 / `BeginWaitForSingleObject`。
- **必须 `RELEASE_BUILD=1` in `env.bat`** for release (L10 §6);禁用 `git describe --tags` 取版本号。
- **必须 `StartMaintenance` / `EndMaintenance` 包裹** `RimeLeversApi::{export,import}_user_dict`,否则 leveldb LOCK 失败。
- **必须 UTF-8 + `EF BB BF` + CRLF** 写 `output/install.nsi` (L09)。
- **必须 `/D=<path>` 显式** silent-install (L13 / L17), 且先清 `HKLM\Software\Fluxing\Weasel` 与 `HKCU\Software\Fluxing` (L54)。
- **必须 Conventional Commits + 模块前缀** (`WeaselTSF | WeaselServer | WeaselDeployer | WeaselSetup | WeaselUI | WeaselIPC | RimeWithWeasel | installer | docs | fluxing | ci`), 否则 commit 不合 P4。
- **必须 `git status -sb` + `git diff --stat`** 在 commit 前 — 阻止 `weasel.props` / `env.bat` / `*.obj` 误提交。
- **必须先读 [`Rime-设置说明书-重点快捷键.md`](Rime-设置说明书-重点快捷键.md)** 当修改 `output/data/**/*.yaml` / `output/data/**/*.custom.yaml` 的 `key_binder/bindings` / `editor/bindings` / `ascii_composer/switch_key` / `switches` / `recognizer/patterns` / `punctuator/*` / `translator/*` / `app_options` / `switcher/hotkeys`;遵循 §3 补靪语法、§6/§7 action 完整列表、§9.3 WeaselTSF `RELEASE_MASK` 坑。

- **必须阶段收尾双产物**: 完成 `.specify/specs/NNN-*/tasks.md` 任一阶段任务时, 必须同步更新 `docs/archive/handoffs/handoff-YYYY-MM-DD-<slug>.md` 并就地改写 `docs/archive/prompts/prompt-next-session-YYYY-MM-DD-<slug>.md` (接力 prompt 只给全路径指针, 不复述 handoff 内容); 规则全文见 [`AGENTS.md` §9](AGENTS.md#9-handoff--relay-prompt-mandatory)。

- **本批交接指针（AI）**: [批次 AI · 快捷设置栏右键菜单「变小」根因 + DPI 对齐修复](docs/archive/handoffs/handoff-2026-11-11-batch-ai-quickpanel-menu-dpi.md) —— 用户报「右键菜单非常小、文字缩小、可视度降低」⇒ 根因**不是字号配置**（全仓 `OWNERDRAW`/`WM_MEASUREITEM`/`WM_DRAWITEM` 在生产菜单路径零命中 = **系统资源菜单，仓库无字号可调项**），而是 `TrackPopupMenu` 的**属主窗 + 线程 DPI context 冻结在 96**（面板窗 `GetDpiForWindow=144`，同进程三个隐藏/Popup 属主窗 `dpiWin=96`）⇒ 行高 20.35 px/项 = 96 逻辑值；修法 = `WeaselServer/SystemTraySDK.cpp::ShowPopupMenuAt` **弹出前**把 target 窗 + 当前线程 DPI context 对齐到弹出点所在显示器、`TrackPopupMenu` 后**反向还原**（先 thread 后 window ⇒ 长期状态零残留），DPI API 走 `GetModuleHandleW(L"user32.dll")`+`GetProcAddress` **运行时解析**（同 `QuickPanelDialog.cpp:75` 范式 ⇒ 不引头类型/不新增导入库/老 Windows 自动退化）；**未新增 IPC 消息**、未改菜单项、未做自绘、未动 manifest；一处修覆盖「品牌图标右键」+「托盘右键」两条入口（ADR 0038 共用回调）；门禁：修后真机 **334×650 / 20 项 / 32.50 px·项⁻¹**（修前 238×423 / 20.35），高度比 **1.537 ≈ 系统缩放 150%**、`PROBE_EXIT=0`、截屏目视字号正常 + `build.bat weasel` **0 警告 0 错误** + clang-format 新增行 `RECHECK_TOTAL_HUNKS=0`（LF 保真无 BOM）+ 装机 SHA 双证 `094E54C18D534F5C → 1B0EE23CD2531A7E` **MATCH=True**（新 pid 214300，回滚件 `WeaselServer.exe.old-261111-101226`）；**未闭环**：本批**未提交**、`lessons-learned.md` 未开 `L##`、用户目视复验（托盘那条入口未单独复测）、备选 `PerMonitorV2` manifest 未启用；**关键工具坑** = `FluxingQuickPanel_v3` 只在 IME 激活瞬间存在 ⇒ 「切输入法 → `Alt+,` → 枚举 → 点右键 → 读菜单」**必须同进程内一次做完**（`tmp\AI-activate-panel.ps1`）
- **上一批交接指针（AH）**: [批次 AH · spec 100「线上真实账号闭环 + item_key TAB 契约」](docs/archive/handoffs/handoff-2026-11-11-batch-ah-live-e2e-probe-and-item-key-tab-fix.md) —— **线上判据首次闭环**：新增可复跑探针 `scripts/cloud/live-e2e-probe.mjs`（Node 标准 crypto 独立复刻 KDF + GCM ⇒ 三方一致性可证）⇒ 线上 **register → verify → login → push → pull 全链路 19/19 断言通过**（`push` 首轮 `accepted` / 同版本再推 `skipped` / `pull` 顶层与条目 `server_seq` / 密文可解密且明文一致 / D1 无明文 / R2 对象与 pull 密文逐字节一致）；顺带**修掉一个真机生产缺陷** —— 客户端 `item_key` 恒含 TAB（`<词典名>\t<词条>`）被服务端「禁 C0 控制字符」400 拒绝 ⇒ 服务端放行 TAB（ADR 0057）+ 2 条回归用例 + 重新部署（Version ID `c18cc111-50c4-4b84-bb52-bcfd6c150807`）；门禁：云单测 `PASS=418 FAIL=0`（+4 跨实现固定向量）+ `build.bat weasel` 0 错误 0 警告 + 服务端 vitest 30/30；**未闭环**：真实 Windows 客户端用同一 `auth_key` 登录线上账号的端到端、双机 AC-08/AC-09、真机截图；ADR [0057](docs/adr/0057-item-key-tab-separator-contract.md)
- **上一批交接指针（AG）**: [批次 AG · spec 100 阶段 F 收尾「服务端加固部署 + 本地改动钩子接通」](docs/archive/handoffs/handoff-2026-11-11-batch-ag-local-change-hook-and-server-deploy.md) —— **服务端加固已部署上线**（`npx wrangler deploy` ⇒ `api.aiec.fun`，Current Version ID `0aaf933c-f832-4c64-a726-3570f32ed848`；`GET /v1/health` ⇒ **200**；`POST /v1/auth/resend`（未知邮箱）⇒ **404 `email_not_found`** ⇒ resend 端点线上已生效；`MAIL_MODE="outbox"` 保持 $0 路线）+ **本地改动钩子接通（ADR 0056，不新增 IPC 消息）** —— 开工前逐项核实**推翻了 AF 的前提**（`WeaselTSF` 不链接 librime、`RimeWithWeaselHandler` 归 `WeaselServerApp` 所有 ⇒ 提交事件与同步引擎同在 `WeaselServer.exe`）⇒ 改走**进程内函数指针槽**（`include/FluxingLocalChangeHook.h` + `RimeWithWeasel/FluxingLocalChangeHook.cpp`；依赖方向 **Cloud → Rime**，否则 `TestWeaselIPC` LNK2019）⇒ 落点 = `_Respond` 的 `get_commit` 成功处（**全仓唯一**提交边界，读后即清 ⇒ 每笔提交恰好一次）⇒ `CloudSyncNotifier`（`shared_ptr` 持引擎 ⇒ 无 UAF；`StopEngine()` 先摘目标再停引擎）⇒ `CloudSyncEngine::NotifyLocalChange()`（只置标志 + 唤醒；网络 I/O 在引擎后台线程 ⇒ 输入零阻塞，P2）；另 `WeaselServerApp` 构造期即构造云账号控制器 ⇒ 云同步不再依赖「用户是否打开过账号页」（未登录仍零出向请求）；门禁：云单测 **`PASS=414 FAIL=0`**（基线 402 ⇒ **+12**）+ `build.bat weasel` **0 错误 0 警告**（顺带修掉两处既有警告 C4099 / C4996）+ clang-format `FORMAT_BAD_FILES=0` / `FORMAT_NEWLINE_BAD=0` + 服务端 `typecheck=0` / vitest **30/30**；**未闭环**：`skipped` / `server_seq` 需真实账号（`MAIL_MODE="outbox"` ⇒ 验证码明文不可直读）+ 双机验收（`CloudCredentialStore` 为 Windows 用户级全局单例）+ 真机截图 / 托盘 `OUR_HIT=0` / 断网退避 / 输入体感 / 钩子端到端时序（均需用户在场）；ADR [0056](docs/adr/0056-local-change-hook-in-process-direct-link.md)
- **上一批交接指针（AF）**: [批次 AF · spec 100 阶段 F「同步引擎与调度」T501–T505 + 阶段 G 加固项（resend 端点）](docs/archive/handoffs/handoff-2026-11-11-batch-af-sync-engine-scheduling.md) —— **新建 `WeaselServer/Cloud/CloudSyncEngine.{h,cpp}`**（单后台线程 + `cv_.wait_until(deadline, pred)` ⇒ 事件唤醒 + 定时唤醒、无轮询；单调钟调度、墙钟只做显示；引擎与凭据解耦 ⇒ 未登录零出向请求）+ **改 `CloudSyncService`**（拉→合并→回写→推→侧写；推 200/请求、拉 100/页×10 页；顶层 `server_seq` 缺失时保留调用方值）+ **账号页 T505**（`kIdSyncStatus=1017`「最近同步」行 + `kIdSyncNow=1016`「立即同步」+ `kIdResend=1015`「重新发送验证码」；客户区 410→472；时间格式化用 Win32 FILETIME 链，**禁 CRT `localtime`**）+ **服务端加固**（`wire.ts` `compareHlc`/`compareUtf8Bytes` + `sync.ts` push HLC 守卫（响应加 `skipped`）+ pull 顶层 `server_seq` + `auth.ts`/`index.ts` 新增 `POST /v1/auth/resend`）；**修掉 4 个生产缺陷**（定时初值 0 ⇒ `startup_delay_ms` 形同虚设 / 退避第 4 档 60 秒 ⇒ 改 60 分钟 / `kWait` 覆盖 `last_outcome` / sidecar 把未推送的本地改动误标已同步）；门禁：云单测 **`PASS=402 FAIL=0`**（基线 324 ⇒ **+78**）+ `build.bat weasel` **0 错误 0 警告** + 8 生产文件 clang-format `FORMAT_BAD_FILES=0` + 服务端 `typecheck=0` / vitest **30/30**（基线 20 ⇒ +10）；**未闭环**：服务端加固**未部署**（`wrangler deploy` 待授权 ⇒ 线上 `skipped`/`server_seq`/resend 全未生效）+ `NotifyLocalChange()` **无调用方**（TSF 侧 IPC 四边同步红线 ⇒ 本批偏差，由 60 分钟定时轮兜底）+ 双机验收（`CloudCredentialStore` 全局单例 ⇒ 同用户两套目录**不够**）+ 真机截图 / 托盘 `OUR_HIT=0` / 收码端到端（`MAIL_MODE="outbox"` 不发真信）；ADR [0055](../../docs/adr/0055-cloud-sync-engine-scheduling-and-convergence.md)
- **上一批交接指针（AE）**: [批次 AE · spec 100 阶段 E「注册 + 邮箱验证码」客户端链路补齐](docs/archive/handoffs/handoff-2026-11-11-batch-ae-cloud-register-verify-chain.md) —— **补上客户端完全缺失的注册 + 邮箱验证码链路**（服务层 `CloudAccountService::Register`/`VerifyEmail` + UI 层 `SubmitRegister`/`SubmitVerifyEmail` + 验证码/注册/验证三控件，客户区 `460×320 → 480×410`；**5 文件**）；**设计定案 = 注册 ≠ 登录**（`Register` 成功不落凭据、不改状态机，必须 `VerifyEmail` + `Login` 才拿令牌；单测用 `state()==kLoggedOut` + `store.Load` 失败双重坐实）；门禁：云单测 `PASS=324 FAIL=0`（基线 297 ⇒ **+27**）+ `build.bat weasel` **`WEASEL_EXIT=0` 0 错误** + 4 生产文件 clang-format `diffLines=0` + 产物新鲜度 `FRESH=True`；**未闭环**：账号页真机注册→收码→验证→登录端到端（`MAIL_MODE="outbox"` ⇒ 验证码只进 D1 `mail_outbox` / `email_verifications` 存 `code_hash`，明文不可直读）+ 真机截图（IME 自绘窗跨进程抓不到）；**阶段 G 加固项**：服务端无 resend 端点 ⇒ 验证码过期/用尽后用户被锁死，补它需 `wrangler deploy`，与「不重做 Cloudflare」冲突故顺延
- **上一批交接指针（AD）**: [批次 AD · 托盘「中/英」图标根因坐实（第四轮）+ spec 100 线上判据阻塞点收口](docs/archive/handoffs/handoff-2026-11-11-batch-ad-tray-langbar-residue.md) —— **代码修复已对且已上机**（`kLangBarRegisterItem = false`；注册表双视图 → 当前构建产物 sha16 `AB623A41EE447C5D`）；复现根因**第四次定位** = **仍持有修复前镜像的宿主进程**（Windows 已加载映像不随磁盘文件替换而更新；`install.nsi` 的 locked 分支只写 versioned shim ⇒ 裸名旧文件留盘）；**收口轮已完成三件**：(a) 持旧镜像宿主 **6 → 1**（已结束 msedge / Weixin / DingTalk / Claude Code Haha / YXCalendar，剩 `MiniMax Code`(163924) = 本会话宿主，会话结束后自然解决；证据 `tmp/AD-tray-check-5.txt`）(b) **T405 判据已闭合**（探针 `tmp/AD-t405-menu-probe.ps1` 退出码 0：菜单 20 项、pos=4 id=`40024` text=`账号与同步 (&Z)`；截图 `tmp/AD-T405-tray-menu.png`）(c) Cloudflare **自动化授权失败**（`wrangler login --browser=false` 回调窗口仅 ~2.5 min，UIA 点 Allow 120s 未命中，凭据三路径全 MISS）；**未闭环**：(a) `OUR_HIT=0` 需用户重启 MiniMax Code / 注销重登后复跑 `scripts/install/check-tray-input-indicator.ps1`；(b) Cloudflare 需人工点 Allow（或注入 `CLOUDFLARE_API_TOKEN`）⇒ D1/R2/AUTH_PEPPER/deploy/DNS 全未执行 ⇒ **线上判据 not verified**
- **上一批交接指针（AC）**: [批次 AC · spec 100 阶段 E 入口接线 **T405 代码 DONE，真机判据未闭环**（快捷设置栏品牌图标右键菜单 + 托盘右键菜单新增「账号与同步 (&Z)」= `ID_WEASELTRAY_CLOUD_ACCOUNT 40024`，点击调 `fluxing::cloud::account_dialog::Show()`；**3 文件 / +19 行** —— `WeaselServer/resource.h`、`WeaselServer.rc`（CHS/CHT/ENU 各 +1 MENUITEM，UTF-16LE+BOM+CRLF 保真）、`WeaselServerApp.cpp::SetupMenuHandlers()`；**未新增任何 IPC 消息**、**未改面板几何**、**未复用** `ID_WEASELTRAY_SYNC 40012`；落点按实做回写了 tasks.md（原预估 `QuickPanelDialog.cpp/.h` 未改，因 ADR 0038 共用菜单）；门禁：`build.bat weasel` 0 错误 + 产物 `LoadMenuW(105)` 自证 40024 恰好 1 处且顺序正确 + 云单测 `PASS=297 FAIL=0`（与 AB 基线逐字一致）+ clang-format 新增行干净；**未闭环**：真机截图需用户装机点一次右键菜单（IME 自绘窗跨进程抓不到）+ Cloudflare 人工动作仍待执行（wrangler login / D1 / R2 / AUTH_PEPPER / deploy / api.aiec.fun DNS ⇒ 线上判据 not verified）](docs/archive/handoffs/handoff-2026-11-11-batch-ac-t405-entry-wiring.md) · 接力 prompt: 见当轮回答正文；按用户 2026-11-08 指示，接力 prompt 直接给在当轮回答正文、不另落文件
- **上一批交接指针（AB）**: [批次 AB · spec 100 阶段 D「客户端运行时」**完成**（T301–T305 全 DONE：新建 `WeaselServer/Cloud/` **18 个文件** —— `CloudJson`/`CloudCrypto`/`CloudTransport`/`CloudCredentialStore`/`CloudAccount`/`CloudSyncService`/`CloudUserDictIo`/`CloudAccountDialog` 各 `.h`+`.cpp` 共 16 + `CloudRimeUserDictBackend.cpp` + `CloudMaintenanceHost.h`；PBKDF2-HMAC-SHA256 60 万次 + HKDF 自实现、AES-256-GCM 复用 `FluxingCloudEnvelope.h` 的 CNG 实现、salt 按邮箱确定性派生（**ADR 0054**）、WinHTTP 传输（**禁 libcurl**）、凭据进 Windows Credential Manager、HLC 只发嵌套 `{ms,counter,device}`、词库走 **T102 快照通道** `backup_user_dict`/`restore_user_dict`、账号页 `fluxing::cloud::account_dialog::Show()`；单测 `PASS=297 FAIL=0`（基线 134）、`build.bat weasel` 0 错误、clang-format 18.1.8 全干净；文档按用户授权**回写** `plan.md` 9 处 + `tasks.md` 5 处 + ADR 0054 + CHANGELOG + ops 全文 + `wrangler.toml` 自定义域 `api.aiec.fun`；**未验证**：Cloudflare 授权/D1/R2/secret/deploy/DNS 全部未执行 ⇒ 线上判据 not verified，阶段 E 入口（T405）未做）](docs/archive/handoffs/handoff-2026-11-11-batch-aa-stage-d-not-started.md) · 接力 prompt: 见当轮回答正文；按用户 2026-11-08 指示，接力 prompt 直接给在当轮回答正文、不另落文件
- **上一批交接指针（Z）**: [批次 Z · spec 100 阶段 C「Cloudflare 服务端」（T211–T215 全 DONE：`cloud/fluxing-sync/` 落地 Workers + D1 + R2，9 条契约端点 + `/v1/health`，23 文件 / +6611 提交 `5495d52`；`npm run check` 退出码 0、真 workerd 内 20/20 用例 PASS、`typecheck` 通过、变异验证证明用例有牙；**未验证**：`wrangler deploy` / 自定义域路由 / `MAIL_MODE=cloudflare` 真实发信 —— 自有域名与 Workers Paid 均未就绪；下批阶段 D（T301–T305）不依赖这两项即可开工）](docs/archive/handoffs/handoff-2026-11-10-batch-z-cloud-stage-c.md) · 接力 prompt: 见当轮回答正文；按用户 2026-11-08 指示，接力 prompt 直接给在当轮回答正文、不另落文件
- **上一批交接指针（Y）**: [批次 Y · spec 100 阶段 B「契约与客户端纯逻辑」（T201–T204 全 DONE：新建 `include/FluxingCloudEnvelope.h` 承载信封 8 行线格式 + HLC 全序 + 条目合并 + CNG AES-GCM，`test/TestFluxingCloud` 改为直接 include 生产头、`PASS=134 FAIL=0`，并已 `grep` 取证「测试文件里无第二份合并 / HLC 实现」；阶段 C 的服务端 TS 与批次 Z 可并行，冲突面为零）](docs/archive/handoffs/handoff-2026-11-10-batch-y-cloud-stage-b.md) · 接力 prompt: 见当轮回答正文；按用户 2026-11-08 指示，接力 prompt 直接给在当轮回答正文、不另落文件
- **上一批交接指针（X）**: [批次 X · 用户资料云同步「取证与工程底座」（T101 邮箱验证码通道 / T102 词库合并语义 / T103 端点可达性 三份结论落盘 + T104 测试工程骨架接线；spec 100 硬前置红线已解除，批次 Y 的 T201–T204 可开工）](docs/archive/handoffs/handoff-2026-11-10-batch-x-cloud-sync-foundation.md) · 接力 prompt: 见当轮回答正文；按用户 2026-11-08 指示，接力 prompt 直接给在当轮回答正文、不另落文件
- **上一批交接指针（W）**: [批次 W · 托盘「中英」语言栏项图标去除（ADR 0044 / 0053-v1 双双证伪 ⇒ 改为「根本不注册该语言栏项」，`kLangBarRegisterItem = false`；真机判据 `INPUT_INDICATOR_COUNT` 2→1、`OUR_TOOLTIP_HIT=0`、`VERDICT=PASS_VALID_PRECONDITION`，宿主五元组自证加载构建产物；单测 TestFluxingLangBar 8/8、装机 ALL HARD CHECKS PASSED；**已知代价**：语言栏项承载的左键面板入口与右键菜单（含「快捷键设置」「用户登录」）一并没有入口，快捷设置栏仍可 `Alt+,` 打开；**回退杠杆** = `kLangBarRegisterItem` 置 true）](docs/archive/handoffs/handoff-2026-11-10-batch-w-langbar-item-removed.md) · 接力 prompt: 见该 handoff 文档；按用户 2026-11-08 指示，接力 prompt 直接给在当轮回答正文、不另落文件
- **上一批交接指针**: [批次 V · 快捷面板位置改「工作区比例锚点」（分辨率增大不再向左上漂移，ADR 0051）+ 方向 A 主题重绘（32-bit BGRA DIB 自建 src-over 合成 + `UpdateLayeredWindow`；半透明面板体 / 1px 发丝描边 / 分层软投影 / hover 只描边不铺底，ADR 0052）；单测 TestQuickPanelDialog 331/331、装机 WeaselServer.exe SHA256 与构建产物一致（已上机）；**待用户人工复验**：真实改一次分辨率看位置是否还漂、方向 A 视觉是否满意](docs/archive/handoffs/handoff-2026-11-10-batch-v-panel-anchor-and-theme.md) · 接力 prompt: 见该 handoff 文档；按用户 2026-11-08 指示，接力 prompt 直接给在当轮回答正文、不另落文件
- **上一批交接指针**: [批次 U · 快捷面板 6 槽→4 槽（摘掉「快捷键设置」「用户登录(关于·账号说明)」两个键帽，托盘入口保留）+ 键帽符号/笔宽严格同比缩放（新增 `include/FluxingPanelIconGeom.h` 单一真相源；笔宽改为向下取整、逗号目标字号直接按系数给 48→72→96，ADR 0050）；单测 TestQuickPanelDialog 269/269、TestQuickPanelRefactor 41/41、L0 15/15、装机 EXIT=0 且装机 WeaselServer.exe SHA256 与构建产物一致（已上机）；**待用户人工复验**：真实改一次分辨率看符号是否同比、线条是否还偏粗，以及 4 键帽顺序与托盘「快捷键设置」入口](docs/archive/handoffs/handoff-2026-11-09-batch-u-panel-4slots-dpi-symbols.md) · 接力 prompt: 见该 handoff 文档；按用户 2026-11-08 指示，接力 prompt 直接给在当轮回答正文、不另落文件
- **上一批交接指针（T）**: [批次 T · 搜狗双拼失效根因修复（显式选方案=全局广播+新会话继承，ADR 0048）+ 快捷面板 DPI 适配（logo 位图随尺寸重建+图标几何统一缩放，ADR 0049）+ §7.7 验收判据 C3/C3b 失灵修复（shim 名改用 3–5 段区间正则、C3b 由打印升为硬检查，消除「掩盖真缺件」）；单测 249/258 全绿、L0 15/15、装机验收 EXIT=0（C3 列出 26 个 shim、C3b 命中版本化 shim）、正则正反例 8/8；真机验收：双拼 ✅ / DPI logo ✅ 但键帽符号未同比缩放、线宽偏粗（§7.8 记明 DPI 只闭环 logo 一路）；下阶段 P0 键帽符号+线宽 DPI 收尾（§8.1）、P1 去除「快捷键设置」「用户登录(关于·账号说明)」两个键帽（§8.2，用户指定下一会话执行）](docs/archive/handoffs/handoff-2026-11-09-batch-t-sogou-dpi.md) · 接力 prompt: 见该 handoff 文档；按用户 2026-11-08 指示，接力 prompt 直接给在当轮回答正文、不另落文件
- **上一批交接指针（S）**: [批次 S · spec 045 圆形 ✕ 脏态确认 P0 修复（确认框被面板失焦自毁吞掉导致修改静默丢失）+ T201 方案下拉候选源评估（单测 PASS=249、L0 15/15、装机真机复验全绿）](docs/archive/handoffs/handoff-2026-11-09-batch-s-close-x-confirm.md) · 接力 prompt: 见该 handoff 文档；按用户 2026-11-08 指示，接力 prompt 直接给在当轮回答正文、不另落文件
- **上一批交接指针（R）**: [批次 R · spec 045 T301–T304 可测入口（抓出并修掉写读不一致 P0）+ T201/T202/T203 界面补全 + Fix A 分层窗口不合成根因（单测 PASS=246、WeaselServer 构建通过）](docs/archive/handoffs/handoff-2026-11-09-batch-r-t201-t304.md)
- **上一批交接指针（Q）**: [批次 Q · spec 045 T104+T105「+ 自定义」子 modal 与验证指引文案（ADR 0046）](docs/archive/handoffs/handoff-2026-11-09-batch-q-t105-add-modal.md)
- **上一批交接指针（P）**: [批次 P · spec 045 T111 真机验收闭环（run11 = 14/14 全绿，已提交）](docs/archive/handoffs/handoff-2026-11-09-batch-p-t111-acceptance.md) · 接力 prompt: 见该 handoff 文档；按用户 2026-11-08 指示，接力 prompt 直接给在当轮回答正文、不另落文件
- **上一批交接指针**: [批次 O · spec 045 T111 范围定案（方案 A：应用级两行只读、永不写托管块）](docs/archive/handoffs/handoff-2026-11-09-batch-o-t111-scope.md) · 接力 prompt: 见该 handoff 文档；按用户 2026-11-08 指示，接力 prompt 直接给在当轮回答正文、不另落文件
- **更早批次交接指针（N）**: [批次 N · spec 045 T107 正文级真合并（已实现，构建/单测/门禁全绿；缺提交与真机验收）](docs/archive/handoffs/handoff-2026-11-08-batch-n-t107-merge-design.md) · 接力 prompt: 见该 handoff 文档；按用户 2026-11-08 指示，接力 prompt 直接给在当轮回答正文、不另落文件
- **上一批交接指针**: [批次 M · spec 045 快捷键设置 P1 收口（T101/T102/T103/T106）](docs/archive/handoffs/handoff-2026-11-08-batch-m-shortcut-settings-p1.md) · 接力 prompt: 见该 handoff 文档；按用户 2026-11-08 指示，接力 prompt 直接给在当轮回答正文、不另落文件
- **更早批次交接指针（L）**: [批次 L · 快捷设置右键菜单「快捷键设置」+ 托盘「中英文」摘除上机 + 快捷键设置界面 P0 修复](docs/archive/handoffs/handoff-2026-11-08-batch-l-shortcut-settings-menu.md) · 接力 prompt: 见该 handoff 文档；按用户 2026-11-08 指示，接力 prompt 直接给在当轮回答正文、不另落文件
- **更早批次交接指针（K）**: [批次 K · 快捷设置栏图标 v13 英文逗号左缘裁切](docs/archive/handoffs/handoff-2026-11-08-batch-k-v13-punct-comma-cut.md) · 接力 prompt: [prompt-next-session-2026-11-08-batch-k-v13-punct-comma-cut.md](docs/archive/prompts/prompt-next-session-2026-11-08-batch-k-v13-punct-comma-cut.md)
- **更早批次交接指针（J）**: [批次 J · 快捷设置栏图标 v12 英文逗号 + 语言栏退出常驻托盘](docs/archive/handoffs/handoff-2026-11-08-batch-j-v12-punct-comma-tray-icon.md) · 接力 prompt: [prompt-next-session-2026-11-08-batch-j-v12-punct-comma-tray-icon.md](docs/archive/prompts/prompt-next-session-2026-11-08-batch-j-v12-punct-comma-tray-icon.md)
- **更早批次交接指针（I）**: [批次 I · 图标 v11 + 标点跟随状态 + 会话门禁 + 提交边界标点](docs/archive/handoffs/handoff-2026-11-08-batch-i-v11-punct-session-gate.md) · 接力 prompt: [prompt-next-session-2026-11-08-batch-i-v11-punct-session-gate.md](docs/archive/prompts/prompt-next-session-2026-11-08-batch-i-v11-punct-session-gate.md)
- **更早批次交接指针（H2）**: [批次 H2 · 快捷面板图标 v10 字符逗号 + 全角=中文标点 + 点击不失焦](docs/archive/handoffs/handoff-2026-11-08-quickpanel-v10-punct-semantics.md) · 接力 prompt: [prompt-next-session-2026-11-08-quickpanel-v10-punct-semantics.md](docs/archive/prompts/prompt-next-session-2026-11-08-quickpanel-v10-punct-semantics.md)
- **更早批次交接指针（H）**: [批次 H · 快捷面板全半角图标 v9 + 半角=英文标点 + 中英联动标点](docs/archive/handoffs/handoff-2026-11-07-quickpanel-v9-fwicon-batch-h.md) · 接力 prompt: [prompt-next-session-2026-11-07-quickpanel-v9-fwicon-batch-h.md](docs/archive/prompts/prompt-next-session-2026-11-07-quickpanel-v9-fwicon-batch-h.md)
- **并行线交接指针（spec 100 · 云端账号与多端同步）**: [云同步「下一阶段执行序」产出（2026-11-10）](docs/archive/handoffs/handoff-2026-11-10-cloud-sync-next-phase-plan.md) —— 「制定计划」已交付：在 spec 100 三件套（11-07 已落盘）之上补出计划漂移 D1–D4、批次 X 精确范围、X–AD 门禁表与不做清单；**批次 X（T101–T104 取证与底座）可立刻开工、不需用户决策**；阶段 E（账号页）被 D1 阻塞（推荐 A = 快捷面板第 4 槽重新成为「账号」入口）；接力 prompt: 见该 handoff 文档；按用户 2026-11-08 指示，接力 prompt 直接给在当轮回答正文、不另落文件 · 上一轮: [规划落地（2026-11-07）](docs/archive/handoffs/handoff-2026-11-07-fluxing-cloud-sync-plan.md)
- **更早批次交接指针（G）**: [批次 G · 全半角按钮图标 v7 对角矢量布局](docs/archive/handoffs/handoff-2026-11-07-quickpanel-v7-fwicon-diagonal.md) · 接力 prompt: [prompt-next-session-2026-11-07-quickpanel-v7-fwicon-diagonal.md](docs/archive/prompts/prompt-next-session-2026-11-07-quickpanel-v7-fwicon-diagonal.md)
完整的 "为什么" 与事故溯源在 `lessons-learned.md`;Const. 链接 + Verification 见各专项 `.claude/rules/`。

---

## 3. 必须分流

| 决策类型 | 写到 |
|---|---|
| 架构决策 (改 PPL? / 改 IPC? / 换构建?) | `docs/adr/NNNN-<kebab>.md` (MADR 格式, `docs/adr/README.md`) |
| 事故 / bugfix (L09 / L13 / L17 / `WeaselServer.exe` 不退出) | `.specify/memory/lessons-learned.md` L##, 引用 `CONTEXT.md §5` |

不要把决策埋在 commit message / PR body — 会被 commit history 覆盖。

---

## 4. 专项规则 (前 7 项按 frontmatter `paths:` 自动加载)

| 文件 | 触发 glob |
|---|---|
| [`.claude/rules/codebase-graph.md`](.claude/rules/codebase-graph.md) | `Weasel*` / `RimeWithWeasel` / `include` / `test` |
| [`.claude/rules/ipc-boundary.md`](.claude/rules/ipc-boundary.md) | `WeaselIPC*` / `WeaselIPCData.h` / `PipeChannel*` |
| [`.claude/rules/build-toolchain.md`](.claude/rules/build-toolchain.md) | `*.bat` / `*.sln` / `*.xmake.lua` / `*.props` / `env.bat` |
| [`.claude/rules/commit-checklist.md`](.claude/rules/commit-checklist.md) | `test/` / `.specify/specs/` / `CHANGELOG.md` |
| [`.claude/rules/hotkey-binding.md`](.claude/rules/hotkey-binding.md) | `Hotkey*` / `Shift*` / `Test*Hotkey*` / `Test*Binding*` |
| [`.claude/rules/rime-settings.md`](.claude/rules/rime-settings.md) | `output/data/**/*.yaml` / `output/data/**/*.custom.yaml` / `weasel.yaml` |

---

## 5. 自我约束

> CLAUDE.md 不重复 AGENTS.md / CONTEXT.md / constitution.md;被新文件覆盖过的章节直接删, 不要演化成拷贝。
