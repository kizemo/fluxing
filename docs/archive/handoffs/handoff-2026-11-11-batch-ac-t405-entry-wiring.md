# 批次 AC 交接 · spec 100 阶段 E 入口接线 T405（代码 + 门禁完成，真机判据待用户）

- 生成：2026-11-11 · 分支 `feat/100-cloud-sync` · 本批基点 HEAD `9d8a7a0`（批次 AB = 阶段 D 全部 DONE）
- 上游：[批次 AB · 阶段 D 客户端运行时](handoff-2026-11-11-batch-aa-stage-d-not-started.md)（文件名沿用 AA，内容已是阶段 D 完成版）
- 规格：`F:/soft/00selfmade/rime_claude/.specify/specs/100-fluxing-cloud-account-sync/tasks.md`（T405 见 §阶段 E）、`plan.md`、`docs/adr/0038-*.md`、`docs/adr/0054-*.md`

---

## 1. 本批做了什么（一句话）

在**已就绪**的云账号页 `fluxing::cloud::account_dialog::Show()` 上接了用户可见入口：托盘右键菜单与快捷设置栏品牌图标右键菜单（两者**共用** `IDR_MENU_POPUP`，ADR 0038）新增一项「账号与同步 (&Z)」，点击后在 UI 线程打开账号页。**零 IPC 变更、零新增第三方依赖、零面板几何改动。**

## 2. 落点与批次 AB 文档预估不同（已回写 tasks.md）

`tasks.md` 原预估 T405 落在 `WeaselServer/QuickPanelDialog.cpp` / `.h`。**实际落点跟随批次 L 先例**（同一套菜单命令的既有做法）：

| 文件 | 改动 |
|---|---|
| `WeaselServer/resource.h` | `L70-71` 新增注释 + `#define ID_WEASELTRAY_CLOUD_ACCOUNT 40024`（现网 ID 上限原为 40023） |
| `WeaselServer/WeaselServer.rc` | `L166`（CHS）/ `L263`（CHT）/ `L360`（ENU）各加 1 行 `MENUITEM`，紧随「快捷键设置 (&B)」、位于「用户词典管理 (&D)」**之前**；总行数 410 → 413 |
| `WeaselServer/WeaselServerApp.cpp` | `L16` 新增 `#include "Cloud/CloudAccountDialog.h"`；`L505` 起在 `SetupMenuHandlers()` 注册 handler（含跨线程弹回） |

合计 **3 文件 / +19 行**（`.rc` 是二进制式 UTF-16LE 资源，`git diff --stat` 显示 `Bin 26210 -> 26652 bytes`，属正常）。

### 两条刻意的设计决定（勿被后来者"优化"掉）

1. **不复用 `ID_WEASELTRAY_SYNC`(40012)**。该 ID 承载 `WeaselDeployer.exe /sync`（**本机**词库同步，见 `WeaselServerApp.cpp:400-402`），菜单文案「用户资料同步 (&N)」与云端「账号与同步」极易混淆。挪用 = 静默剥夺既有本机功能。**新增 ID 是唯一正确解。**
2. **只调 `account_dialog::Show()` 即为完整接线**，不需要额外的服务水化。账号页自身在 `WeaselServer/Cloud/CloudAccountDialog.cpp:105-108` 里 `new CloudAccountService(...)` 并 `RestoreFromStore()`，且跑在控制器线程（`CloudTransport.h:10` 明令禁止 UI 线程调用）；`Show()`（`:556`）只做 `SetForegroundWindow` / `CreateWindowExW` ⇒ **无 constitution P2 阻塞 I/O 风险**（本入口不在 `ITfTextInputProcessor::*` 回调内，且 handler 自身用 `m_uiThreadId` 判断 + `PostMessage` 自投递弹回 UI 线程，沿用既有写法）。
3. 助记符选 `Z`（现网 E/F/M/O/T/V/W/X/Y 已占用），ENU 用 `Account and Sync (&Z)` 而非 `Account & Sync`，避开 `&` 转义歧义。`_APS_NEXT_COMMAND_VALUE 40003` 不动（40018–40023 早已先例性超出该值，VS 生成字段）。

## 3. 门禁三分账（诚实记账）

### ✅ 已验证（有证据路径，可复跑）

| 判据 | 结果 | 证据 |
|---|---|---|
| WeaselServer 构建 | **0 个错误**、5 个既有 warning、耗时 38.93 s | `tmp/T405-weaselserver-build.log`（产物 `output\Win32\WeaselServer.exe` 2641920 B，SHA256 `094E54C18D534F5C630E221BEA5E1C9391C6D2CAEF6E3553A3E5B4C4A21DCD50`） |
| **构建产物里真有该菜单项**（规避"构建链没编那份二进制"老坑） | `LoadLibraryExW(LOAD_LIBRARY_AS_DATAFILE)` + `LoadMenuW(hMod, 105)` + `GetMenuStringW` ⇒ `id=40024 text=[Account and Sync (&Z)]`，**恰好 1 处**，位置在 `40023 Shortcut settings` 之后、`40010 Dictionary manager` 之前 | `tmp/T405-verify-menu.ps1` → `tmp/T405-verify-menu.out.txt` |
| 三语言块均编进二进制 | exe 原始字节内 UTF-16LE 文案「账号与同步」/「帳號與同步」/「Account and Sync」**各命中 1 次** | 同上脚本第二段（`CHS/CHT/ENU occurrences = 1/1/1`） |
| 云单测无回归 | `RESULT: PASS=297 FAIL=0`、`VERDICT: ALL PASS`、`TEST_FLUXINGCLOUD_EXIT=0` —— 与批次 AB 基线**逐字一致** | `tmp\build-fluxing-cloud-tests.cmd` → `tmp/T405-cloud-tests.log` |
| 编码保真 | `.rc` **UTF-16LE + BOM `FF FE` + CRLF** 未破（回读 `head=255,254`，行数 410→413）；`resource.h` / `WeaselServerApp.cpp` UTF-8 无 BOM 纯 CRLF | 本轮实测；补丁脚本 `tmp/T405-apply.ps1`（幂等，可重跑） |
| clang-format（新增行） | 新增区间（`resource.h:70-71`、`WeaselServerApp.cpp:15-16 + 504-521`）与 clang-format 输出**逐行一致 = NONE** | `tmp/T405-app-formatted.cpp`、`tmp/T405-resh-formatted.h` + `Compare-Object` |
| 改动面干净 | `git diff --stat` = 3 文件 / +19 行；`git ls-files --others` 在 `WeaselServer/` 下为空 | 本轮实测 |

### 🔶 推断（有间接证据，未直接观测）

- 点击该菜单项能打开账号页：链路是 `SystemTraySDK.cpp:852` 的 `PostMessage(hTargetWnd, WM_COMMAND, uItem, 0)` → `SetupMenuHandlers` 注册的 handler → `Show()`。ID/文案/顺序已在二进制取证，handler 抄自同文件既有可用先例 ⇒ **逻辑上成立，但运行时未观测**（原因见下）。
- `clang-format 18.1.8`（本机 `C:\Python314\Scripts\clang-format.exe`）对 `WeaselServerApp.cpp` / `resource.h` 报的 24 / 93 处差异**全部落在既有代码**：这两个遗留文件从未 clang-format 干净过，批次 AB 的格式门禁只覆盖 `WeaselServer/Cloud/*` 18 个文件。本批**不扩大格式清理范围**。

### ❌ 未验证（缺条件，下批必须补）

1. **T405 Done 判据的"真机截图"部分未闭环**：需要装机（`scripts\install\silent-install.ps1 'D:\Program Files\fluxing'`）+ 用户在场点一次右键菜单。IME 自绘顶层窗与 IME 激活态跨进程抓不到（记忆 #43），最后一公里必须人工。
2. **未提交**（本批改动在工作树，收口时按显式路径 `git add`）。
3. **账号页真实登录/同步全链路**依旧 `not verified` —— 被 Cloudflare 侧人工动作卡死（见 §4），`api.aiec.fun` 上没有可用服务端。

## 4. 唯一硬阻塞仍是 Cloudflare 人工动作（与批次 AB 相同，未推进）

用户须本人在 `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync` 下完成 `npx wrangler login`（OAuth 只能人做），其后由 agent 代跑：D1 `fluxing-sync` 建库 → R2 桶 → `d1 migrations apply --remote` → `wrangler secret put AUTH_PEPPER`（≥32 B 随机，**绝不进仓库**）→ `wrangler deploy` → `api.aiec.fun` DNS 记录。`wrangler.toml` 的 `database_id` 仍是占位全零 UUID。
⇒ 阶段 D/E 一切**线上**判据保持 `not verified`，T405 的 `Verified: 菜单点击 → Show() → 登录成功 → CloudSyncService 首次 pull` 四步链路里，后两步在本批**不可能**验证。

## 5. 下阶段建议顺序

1. 装机 + 真机截图收口 T405 判据（agent 可先打包 exe，用户点一次即可）。
2. 提交本批（模块前缀 `WeaselServer`）。
3. Cloudflare 授权后跑通线上链路（tasks.md 阶段 E 其余任务）。
4. 阶段 E 全部收口后，再考虑 T404 之类的发布/开关任务。

## 6. 本轮工具坑（写下来，别再踩）

- **`.rc` 必须用 UTF-16LE 解码再取行号**。上一轮摘要里的 `313/502/691` 是把 UTF-16LE 当 UTF-8 读的**假行号**；真实锚点 CHS `LANGUAGE L24 / IDR_MENU_POPUP L157`、CHT `L205/L253`、ENU `L301/L349`。
- **中文 here-string 锚点在 `.cpp` 里命中 0 次**（注释实际文案与记忆不符）⇒ 补丁一律用**纯 ASCII 锚点**，写盘前断言命中次数，写盘后回读 BOM/CRLF/行数。
- **`bash` 工具里先 `cd` 再写/删会被本机硬策略拦**（`write/delete targets cannot be verified safely after a command working directory change`）⇒ 一律绝对路径、不切目录。
- **`Add-Type` C# 里 `id != -1U` 是非法比较**（警告被视为错误）⇒ 用 `id != uint.MaxValue`。探针编译失败会打印 `MENU_ITEM_40024_PRESENT = NO` —— 这是**假阴性**，与记忆 #41 同源：**任何 0 命中在坐实"判据能看见目标"之前都不是证据**。
- `rc` 里词典管理项宏名是 **`ID_WEASELTRAY_DICT_MANAGEMENT`(40010)**，不是 `ID_WEASELTRAY_MANAGE_USER_DICT`。
- `ID_WEASELTRAY_SHORTCUT_SETTINGS` 在 `.rc` 里恰好出现 3 次（每语言块 1 次），可作按块序 CHS→CHT→ENU 定位的锚点。

## 7. 工作树里**不属于本批**的改动（本批未动、未提交，留给原会话）

- `RimeWithWeasel/RimeWithWeasel.cpp`（+118/−3，批次 T 的显式选方案持久化）
- `librime` 子模块 dirty
- `docs/design/quickpanel-v6/design-demos/*.html` ×3（未跟踪）
⇒ 提交必须按**显式路径** `git add`，禁止 `git add .`。
