# Handoff 2026-11-07（云端账号与多端同步）：spec 100 规划落地

> **状态：规划三件套 + ADR + 取代指针 + 交接双产物全部落盘**；实现尚未开工。
> 本文是**新建**文件（未改写 `handoff-2026-11-07-quickpanel-v7-fwicon-diagonal.md` 等更早文档）。
> 下一会话请按 §五 执行序接手，**勿重做已完成的取证与选型**，**勿改写本文以外的历史 handoff / prompt**。
>
> **分支 / 提交**：分支 `Fluxing`，与 `kizemo/Fluxing` **已同步**（批次 F/G 共 5 个提交已推：`38b4c73` / `1df3fad` / `5b1a46f` / `2fb9718` / `3201562`，远端由 `d4aa63a` 推到 `3201562`）。
> 本批新增文件（spec 100 三件套 + ADR 0039 + README 索引 + 010 指针 + 本文 + 接力 prompt + `CLAUDE.md` §2）**在本文写作时尚未提交**；提交后本文 §四 为完整清单。
> 工作区除 §四 所列文件外仅剩 ` m librime` 子模块假脏（**不得提交**）。

---

## 一、当前状态（一段话）

用户 2026-11-07 的原始指令是「进入用户登录、同步配置的阶段，请进行规划，希望可以采用 Cloudflare 空间，满足用户在云端保存配置、个人词库、用户短语等内容，从而实现多端同步」。
本批**只做规划落地，不写实现代码**：通过 `ask_user` 取得四项前置决策（邮箱+密码客户端输入 / 全量端到端加密 / 官方统一端点 / 冲突按条目合并），
产出 spec `100-fluxing-cloud-account-sync` 的 **spec.md（用户视角）/ plan.md（选型、协议、算法、变更面、风险）/ tasks.md（7 阶段 30 条任务）**，
写入 **ADR 0039**（后端与客户端选型决策），给 **spec 010 加取代指针**（其技术结论作废），补 **ADR README 索引**（顺带补上漏登记的 0038），
并落交接双产物 + `CLAUDE.md` §2 指针。
**最关键的技术事实**：仓库**零 HTTP 客户端**（实测 `WinHttp|WinINet|HttpSendRequest|InternetOpen|curl_easy` 全零命中）⇒ spec 010 设计的 libcurl **并不存在**，
因此客户端网络改用系统自带 **WinHTTP**、加密用 **Windows CNG**、凭据用 **Credential Manager**（三者都是操作系统组件 ⇒ 不违反宪法 P3「不新增第三方依赖」）。
**未做**：任何实现代码；批次 G 遗留的「v7 图标人眼观感确认」与「Bug #4 真机复现」（仍需用户在场）。

---

## 二、必读文档（全绝对路径，按此顺序）

| # | 文档 | 为什么必读 |
|---|---|---|
| 1 | `F:\soft\00selfmade\rime_claude\CLAUDE.md` | 进站导航：强制项、分流规则、专项规则表 |
| 2 | `F:\soft\00selfmade\rime_claude\AGENTS.md` | 操作手册（§1 首屏 / §4 危险区 / §5 checklist / §9 交接双产物） |
| 3 | `F:\soft\00selfmade\rime_claude\CONTEXT.md` | 身份 / 模块 / 词汇 / 不变量 |
| 4 | `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` | 法则层 R1–R9 / P1–P8（P2 禁 TSF 阻塞 I/O、P3 禁第三方依赖、P5 中文化、P7 唯一常驻进程） |
| 5 | `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md` | §A 部署、§11 NSIS 配方 |
| 6 | `F:\soft\00selfmade\rime_claude\docs\agents\domain.md` | 工程技能消费契约 |
| 7 | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\spec.md` | **本批主产物**：用户视角（12 US / 20 FR / 9 NFR / 14 AC / 红线 / 待核实） |
| 8 | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\plan.md` | **本批主产物**：选型表、双机架构、KDF/HKDF/AES-GCM、威胁模型、HLC、合并规则、Worker API 契约、D1/R2 布局、调度、风险 R1–R9、Constitution Check |
| 9 | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\tasks.md` | **本批主产物**：7 阶段 30 条任务（T101–T604），每条带 `[P1/P2/P3]` + `[US#]` + Done 判据 |
| 10 | `F:\soft\00selfmade\rime_claude\docs\adr\0039-cloud-account-sync-architecture.md` | 本批决策记录（决策与后果） |
| 11 | `F:\soft\00selfmade\rime_claude\docs\adr\README.md` | ADR 索引（本批补了 0038 / 0039 两行） |
| 12 | `F:\soft\00selfmade\rime_claude\.specify\specs\010-cloud-sync\spec.md` | **仅作历史依据**，首页已加取代指针，技术结论不得引用 |
| 13 | `F:\soft\00selfmade\rime_claude\.specify\specs\098-cloud-candidates\spec.md` | 与本批方向相反的「云」规格，边界见 `plan.md` §14 |
| 14 | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-07-quickpanel-v7-fwicon-diagonal.md` | 上一批完成态（本文不重复其内容）；批次 G 遗留项见 §八 |
| 15 | `F:\soft\00selfmade\rime_claude\.specify\memory\lessons-learned.md` | 事故溯源（尤其涉及构建/装机/探针的条目） |

---

## 三、本批进度（逐项，含验证等级）

| # | 用户指令 | 状态 | 结论与落点 | 验证等级 |
|---|---|---|---|---|
| 1 | 进入「用户登录 + 同步配置」阶段 | ✅ 完成（规划） | spec 100 三件套落盘：`.specify/specs/100-fluxing-cloud-account-sync/{spec.md,plan.md,tasks.md}` | **verified**（文件已写入并回读；spec.md 11192 B / plan.md 23324 B / tasks.md 22645 B） |
| 2 | 采用 Cloudflare 承载云端 | ✅ 完成 | 后端 = **Cloudflare Workers + R2（密文本体）+ D1（索引元数据）**；**排除 KV**（免费写额度仅 1000 次/天，会被同步热路径打穿） | **verified**（Cloudflare 官方免费额度已核实：Workers 10 万请求/天、10ms CPU/请求；R2 10 GB + 零出口费；D1 500 万行读/天 / 10 万行写/天） |
| 3 | 云端保存配置 / 个人词库 / 用户短语，实现多端同步 | ✅ 完成（范围定义） | 同步清单定死：个人配置（文件级，整份 LWW）+ 用户短语（条目级合并）+ 个人词库（条目级合并）；**输入历史 / 上屏内容永不上云**；方案本体不同步 | **verified**（spec.md §3.1 范围表 + §7 合规红线 6 条） |
| 4 | 「用户登录」怎么做 | ✅ 完成（决策已拍板） | 邮箱 + 密码，**客户端输入**；客户端派生 `master = PBKDF2-HMAC-SHA256(pw, salt, 600000)`，HKDF 分离 `auth_key` / `enc_key`；`auth_key` 上传、服务端只存 `HMAC(pepper, auth_key)`；`enc_key` **永不上传** | **verified**（用户 `ask_user` 四项拍板 + plan.md §3.1–§3.3） |
| 5 | 「同步配置」怎么做 | ✅ 完成（协议已定） | 条目信封 `item_key/type/hlc/nonce/cipher`，AES-256-GCM，AAD = `item_key|type|hlc`；定序用 **HLC**（ms + counter + device）不用裸时间戳；合并在**客户端**做（服务端只见密文，物理上做不到），`phrases`/`userdb` 合并后**重新加密回传**使两端收敛 | **verified**（plan.md §4.1–§4.3） |
| 6 | 是否新增 IPC / 是否新增进程 | ✅ 完成（结论：不新增） | 账号页与同步引擎**同在 `WeaselServer` 进程内** ⇒ **不新增任何 IPC 消息** ⇒ 不触发 server/TSF/deployer 四边同步 + ADR + `test/TestWeaselIPC`；**不新增常驻进程**（仍为 1，ADR 0030 三数字基线不破） | **verified**（plan.md §5.1–§5.3；T402 把「若在 `WeaselDeployer` 加入口 ⇒ 才触发四边同步」记为触发条件） |
| 7 | 第三方依赖怎么办 | ✅ 完成 | **零第三方依赖**：网络 WinHTTP、加密 Windows CNG（`BCryptDeriveKeyPBKDF2` + AES-GCM）、凭据 Credential Manager；服务端依赖只在 `cloud/` 子目录，不进 C++ 链接 | **verified**（仓库 grep 实测零 HTTP 客户端 ⇒ 010 的 libcurl 不存在；宪法 P3 / NFR-004 满足） |
| 8 | 架构决策留档 | ✅ 完成 | `docs/adr/0039-cloud-account-sync-architecture.md`（MADR 四段式，含 Consequences 与「可部分回退」说明）；`docs/adr/README.md` 索引补 0038（**此前漏登记**）与 0039 | **verified**（文件写入并回读；README 表格已更新） |
| 9 | 旧规格冲突消解 | ✅ 完成 | spec `010-cloud-sync` 首页加**取代指针**（后端 / libcurl / Passkey / LWW 四项结论作废），保留仅作历史 | **verified**（已回读 010 首页） |
| 10 | 交接双产物 | ✅ 完成 | 本文 + `docs/archive/prompts/prompt-next-session-2026-11-07-fluxing-cloud-sync-plan.md`（单段落 + 全路径指针，不复述本文内容）+ `CLAUDE.md` §2 指针 | **verified**（写入并回读） |
| 11 | 批次 F/G 提交推送（上一批遗留） | ✅ 完成 | 5 个提交推到 `kizemo/Fluxing`（`d4aa63a` → `3201562`）；推送后 `git status -sb` 仅剩 ` m librime` | **verified**（推送输出 + `git status -sb`） |
| 12 | 实现代码 | ⬜ 未做 | 见 §五 执行序（阶段 A 起） | — |
| 13 | 批次 G 的「v7 图标人眼观感」与「Bug #4 复现」 | ⬜ 未做（需用户在场） | 见 §八 | — |

---

## 四、改动文件清单（提交前对照）

| 文件 | 动作 | 说明 |
|---|---|---|
| `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\spec.md` | **新增** | 用户视角规格：12 条 US / 20 条 FR / 9 条 NFR / 14 条 AC / 6 条合规红线 / 5 条待核实 / §8.1 与 010 的关系。LF / 无 BOM |
| `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\plan.md` | **新增** | 技术方案：选型表、双机架构图、KDF/HKDF/AES-GCM 与威胁模型、HLC 定序、条目合并规则、客户端 7 组模块、Worker API 契约、D1/R2 布局、调度与配额、变更面、R1–R9、Constitution Check、备选方案与否决理由、验证策略、098/099/010 边界。LF / 无 BOM |
| `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\tasks.md` | **新增** | 7 阶段 30 条任务（T101–T104 / T201–T204 / T211–T215 / T301–T305 / T401–T405 / T501–T505 / T601–T604），每条含 `[P1/P2/P3]` + `[US#]` + 文件数（≤3）+ Done 判据；末尾「不在本批范围」表。LF / 无 BOM |
| `F:\soft\00selfmade\rime_claude\docs\adr\0039-cloud-account-sync-architecture.md` | **新增** | ADR 0039（MADR 四段式）：Context / Decision（7 条）/ Consequences（含可回退性）。LF / 无 BOM |
| `F:\soft\00selfmade\rime_claude\docs\adr\README.md` | 改 | 索引表补 0038（**漏登记**）与 0039 两行 |
| `F:\soft\00selfmade\rime_claude\.specify\specs\010-cloud-sync\spec.md` | 改 | 首页加取代指针（四项技术结论作废 + 指向 spec 100 与 ADR 0039） |
| `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-07-fluxing-cloud-sync-plan.md` | **新增** | 本文 |
| `F:\soft\00selfmade\rime_claude\docs\archive\prompts\prompt-next-session-2026-11-07-fluxing-cloud-sync-plan.md` | **新增** | 接力 prompt（单段落 + 全路径指针） |
| `F:\soft\00selfmade\rime_claude\CLAUDE.md` | 改 | §2「本批交接指针」指向本文 + 接力 prompt；保留「上一批交接指针」指向批次 G |

> **不得提交**：` m librime`（子模块假脏）、`weasel.props`、`env.bat`、`*.obj`、`*.log`。

---

## 五、下一阶段执行序（按此顺序，理由随附）

1. **阶段 A 三条取证并行开工**（T101 邮件验证码通道 / **T102 词库导出·导入合并语义** / T103 云端域名可达性）—— 理由：T101 与 T103 都含**外部依赖与采购决策**，等待周期最长，必须最早启动；T102 是风险 R1 的闸门，**结论未落盘前禁止写词库合并实现**（T503）。
2. **阶段 B 纯逻辑底座**（T204 共享信封头 + T201–T203 判别性单测）—— 理由：信封编解码 / 条目合并 / HLC 是**生产与单测同源**的单一事实源（宪法 R7），必须先钉死；服务端与客户端两端都按这份契约实现，先做它可避免两端跑偏。
3. **阶段 C 服务端**（T211 → T215）—— 理由：契约稳定后服务端最省事；`wrangler dev` + 本地 D1/R2 集成测试可在不装客户端的情况下把接口全部打通。
4. **阶段 D 客户端运行时**（T301–T305）—— 理由：CNG → WinHTTP → 凭据 → 账户门面 → 词库协调，依赖链单向，先做被依赖者。
5. **阶段 E 账号页与文案**（T401–T405）—— 理由：依赖 D 的账户门面；**T403 `AboutDialog` 文案改写与 T404 CHANGELOG 是宪法 P5 的硬要求**（旧文案「本输入法不含云端账号……不上传、不同步。」与新功能自相矛盾）。
6. **阶段 F 同步引擎与调度**（T501–T505）—— 理由：合并与回传依赖 B 的纯逻辑与 D 的词库协调（T503 依赖 T102 结论）。
7. **阶段 G 双机验收与发版**（T601–T604）—— 理由：AC-01 ~ AC-14 逐条跑；三数字（1 进程 / < 40 MB / 空闲 < 0.1%）按 ADR 0030 复测；**装机前先注销或重启**（`WeaselServer.exe` 是 PPL，新二进制不重启不加载，风险 R4）。

---

## 六、证据

| 证据 | 路径 |
|---|---|
| spec 100 三件套 | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\{spec.md,plan.md,tasks.md}` |
| ADR 0039 | `F:\soft\00selfmade\rime_claude\docs\adr\0039-cloud-account-sync-architecture.md` |
| ADR 索引（补 0038 / 0039） | `F:\soft\00selfmade\rime_claude\docs\adr\README.md` |
| 010 取代指针 | `F:\soft\00selfmade\rime_claude\.specify\specs\010-cloud-sync\spec.md`（首页） |
| 接力 prompt | `F:\soft\00selfmade\rime_claude\docs\archive\prompts\prompt-next-session-2026-11-07-fluxing-cloud-sync-plan.md` |
| 批次 F/G 推送 | 提交 `38b4c73` / `1df3fad` / `5b1a46f` / `2fb9718` / `3201562`，远端 `kizemo/Fluxing` = `3201562` |
| 仓库零 HTTP 客户端（实测） | `grep` `WinHttp|WinINet|HttpSendRequest|InternetOpen|winhttp|wininet|libcurl|curl_easy` 全零命中 |
| Cloudflare 免费额度（已核实） | Workers 10 万请求/天 + 10ms CPU/请求；KV 读 10 万/天但**写仅 1000/天**；R2 10 GB + 100 万 Class A 写/月 + 零出口费；D1 500 万行读/天 / 10 万行写/天 / 5 GB |
| 上一批真机证据（批次 G） | `F:\soft\00selfmade\rime_claude\tmp\batchi.log`（VERDICT=PASS，7/7 全 True）、`tmp\batchg-slot2-ascii.txt`、`tmp\crop-batchg-slot2-*.png` |

---

## 七、决策（本批拍板 / 结论）

| # | 决策 | 依据 |
|---|---|---|
| **D1** | 认证 = 邮箱 + 密码，**在客户端输入** | 用户拍板（`ask_user`） |
| **D2** | 云端数据**全量端到端加密** | 用户拍板 |
| **D3** | 后端 = **官方统一端点**（项目方运营，自有域名） | 用户拍板；配 R5 可达性风险与 T103 取证 |
| **D4** | 冲突**按条目合并** | 用户拍板；⇒ 合并必须落在客户端（服务端只见密文） |
| **D5** | 后端 = Workers + **R2 + D1**，**排除 KV** | KV 免费写仅 1000/天，热路径必被打穿 |
| **D6** | 客户端 = **WinHTTP + CNG + Credential Manager**，零第三方依赖 | 仓库零 HTTP 客户端 + 宪法 P3 / NFR-004 |
| **D7** | 密钥分离：`auth_key` 上传 / `enc_key` 永不上传；服务端只存 `HMAC(pepper, auth_key)` | 让「服务端全陷」仍读不到内容明文 |
| **D8** | 定序用 **HLC**；`config` 整份 LWW，`phrases`/`userdb` 按条目合并并**回传收敛** | 多机时钟不可信；服务端物理上无法按条目合并 |
| **D9** | **不新增 IPC 消息、不新增常驻进程** | 账号 UI 与引擎同在 `WeaselServer`；P2 / P7 / ADR 0030 |
| **D10** | spec **010 的技术结论作废**，仅作历史依据 | 010 的 libcurl 不存在、后端与策略均被取代 |
| **D11** | 首版**不做**找回密码 / 改密 / 网页端 / 组织账号 / 输入历史上云 | 改密牵动全量重加密；输入历史是隐私红线 |
| **D12** | 词库合并语义**先取证后实现**（T102 闸门） | 风险 R1：做错会丢用户词库 |

---

## 八、需用户在场的事项（沿用上一批，未做）

1. **v7 全半角图标人眼观感确认** —— 放大裁片 `F:\soft\00selfmade\rime_claude\tmp\crop-batchg-slot2-A.png` 与 `-B.png`。数值上墨迹 25×25 / 21×23 raw px、双轴居中偏差 ≤1px 已达标，但「够不够大够不够清楚」只能由人判断。
2. **Bug #4（切换输入法后一小段时间只能打英文）真机复现** —— 需用户配合「切换后立即打字」；探针开工前先 `Get-Process LogonUI` 判锁屏（锁屏时 Windows 拒绝输入注入与屏幕捕获，会假失败）。
3. **云端账号的对外承诺口径** —— 若首版要公开宣传「多端同步」，需用户确认对外措辞（尤其「口令遗忘 = 数据不可恢复」这一条的写法）。

---

## 九、工具坑（沿用 + 本轮新增）

**沿用（务必先读）**

1. `git push origin Fluxing` ❌ —— 本 fork 远端叫 **`kizemo`**（`origin` = rime/weasel，只读）。命令：`git -C F:/soft/00selfmade/rime_claude push kizemo Fluxing`。
2. `bash` 里先 `cd` 再执行会被安全策略拦 —— 一律用 `git -C` 或绝对路径。
3. `git add` 必须显式按路径，**禁止 `git add .`**。
4. `edit` 工具的 `old_string` / `new_string` 里写 `\uFF0C`、`\n` 这类转义会被解析成真字符导致匹配失败 —— 改用脚本按边界正则替换。
5. `edit` 工具偶发把空 `new_string` 报成 “must be string”。
6. `GetStockObject(NULL_BRUSH/NULL_PEN)` **不可** `DeleteObject`。
7. 小尺寸实心圆用 `Ellipse` 会被 GDI 光栅化成四角星 —— 用 `RoundRect` + 半径 = 半边长。
8. `GetDIBits` 必须传真实 bitmap 句柄；BMP 高度为负（top-down）会让逐像素循环跑 0 次、假报 diff = 0。
9. PowerShell `for` 初始化式里写比较表达式会**静默走错**。
10. `WeaselServer.exe` 是 PPL ⇒ 新二进制**必须注销或重启**才加载（装机后立刻跑桌面自动化探针会假失败，见 `tmp\chain-batch-h.log`）。
11. PowerShell `Start-Process -FilePath x.exe -Wait` 调 **GUI 子系统**程序会挂死 —— NSIS / 安装器类必须由 `.bat` 经 `cmd /c` 启动；`/D=` 路径值不能带引号且必须是最后一个参数。

**本轮新增**

12. **文件字节约定（已实测）**：handoff / `CLAUDE.md` / ADR / spec / tasks 一律 **LF、无 BOM**；`CHANGELOG.md` 保持 **CRLF、无 BOM**；`output/install.nsi` 必须 **UTF-8 BOM + CRLF**。
13. **中间产物落文件、对话只留指针 + 结论**；单轮工具调用控制在 10 次以内，超过 20 次立刻停止试探、改为一次取齐。
14. **交接卡（`activeDataDir\maintenance\task-state\LATEST.md`）可能属于别的会话** —— 本轮读到的是另一条会话（practilog 设计）的状态，与本任务无关，**按当前项目上下文判断，不要照它执行**。

---

## 十、阶段收尾要求（本批已完成）

- 交接双产物：本文 + `docs/archive/prompts/prompt-next-session-2026-11-07-fluxing-cloud-sync-plan.md`（接力 prompt 只给**全路径**指针，**不复述本文内容**）。
- `CLAUDE.md` §2 已加本文指针与「上一批交接指针」。
- ADR 索引已补（0038 / 0039）。
- 提交与推送：见 §四 清单，**显式按路径 `git add`**，commit 用 Conventional Commits + 单一 scope（建议 `docs(spec): 云端账号与多端同步 spec 100 规划落地` + `docs(adr): ADR 0039 云端账号与多端同步架构决策`）。
