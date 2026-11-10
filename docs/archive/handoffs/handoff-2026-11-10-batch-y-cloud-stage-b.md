# Handoff · 批次 Y · spec 100 阶段 B（契约与客户端纯逻辑 T201–T204）

> 生成：2026-11-10（同日同 spec 就地改写，不另建副本）
> 分支：`feat/100-cloud-sync`
> 上一批：[批次 X · 取证与工程底座](handoff-2026-11-10-batch-x-cloud-sync-foundation.md)
> 结论标注口径：**verified** = 本轮实测有输出；**inferred** = 由实测证据推理但未直接跑；**not verified** = 未跑，仅按文档/常识判断。

---

## 1. 一段现状

spec 100 **阶段 B 已全部完成（T201 / T202 / T203 / T204 四任务 DONE）**：新建生产头 `include/FluxingCloudEnvelope.h`（34.9 KB，`inline`、无状态、无 I/O），把「信封编解码 + HLC 定序 + 条目合并 + CNG AES-256-GCM」四组纯逻辑落为**生产与单测同源**的单一事实源；`test/TestFluxingCloud/TestFluxingCloud.cpp` 已整份重写并 `#include` 该生产头，整包运行 **`RESULT: PASS=134 FAIL=0` / `VERDICT: ALL PASS` / `TEST_FLUXINGCLOUD_EXIT=0`（verified）**，且已 `grep` 取证「测试文件里没有第二份合并 / HLC 实现」（verified）。

阶段 B 的红线是宪法 **R7（生产与单测同源，禁止复制两份实现）** 与 plan.md §5.3「**不新增 IPC 消息**」——本轮**未触碰** `include/WeaselIPCData.h` 及任何 IPC 协议面（verified：`git status` 中无该文件），因此不触发 IPC 四边同步。同步引擎与未来的账号页同处 `WeaselServer` 进程内，阶段 B 只交付纯逻辑，尚未接线到服务端（服务端在阶段 C，批次 Z）。

**当前可直接开工的下一批**：阶段 C（服务端 Cloudflare Worker / D1 / R2，T211 起）与批次 Y 无冲突面 —— 前者是 `cloud/fluxing-sync/` 下的 TypeScript，后者是 C++ 头文件，两批可并行（inferred：目录与语言完全不相交，无共享文件）。

---

## 2. 必读文档（全绝对路径，按阅读顺序）

| # | 路径 | 读什么 |
|---|---|---|
| 1 | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-10-batch-y-cloud-stage-b.md` | **本文**。批次 Y 状态与本轮工具坑 |
| 2 | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\spec.md` | 需求与验收（US7/US8 是本批的直接来源） |
| 3 | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\plan.md` | §4.1 信封 / §4.2 HLC / §5.3 不新增 IPC / §6 服务端契约 |
| 4 | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\tasks.md` | 阶段 B 第 64 行起（T201 68 / T202 76 / T203 84 / T204 92），阶段 C 第 101 行起 |
| 5 | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\next-phase-plan.md` | 执行序与 D1–D5 决策 |
| 6 | `F:\soft\00selfmade\rime_claude\docs\adr\0039-cloud-account-sync-architecture.md` | 架构决策（账户 / 设备 / 密钥 / 同步边界） |
| 7 | `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` | 法则层：R1–R9 / P1–P8（R7 是本批主约束） |
| 8 | `F:\soft\00selfmade\rime_claude\AGENTS.md` | 首屏 §1 + 危险区 §4 + checklist §5 + handoff §9 |
| 9 | `F:\soft\00selfmade\rime_claude\docs\research\fluxing-cloud-2026-11\userdict-merge-semantics.md` | 词库合并语义（T102 结论，阶段 D 起的硬约束） |
| 10 | `F:\soft\00selfmade\rime_claude\docs\research\fluxing-cloud-2026-11\endpoint-reachability.md` | 端点可达性（T103；`*.workers.dev` 红线来源） |
| 11 | `F:\soft\00selfmade\rime_claude\docs\research\fluxing-cloud-2026-11\email-verify-channel.md` | 邮箱验证码通道（T101） |

> 三份 research 结论**不要重新实测覆盖**，它们是硬约束来源。

---

## 3. 阶段逐任务进度

### 阶段 A · 取证与工程底座（批次 X）— 全部 DONE

| 任务 | 状态 | 一句话 |
|---|---|---|
| T101 | DONE | 邮箱验证码通道结论落盘（`email-verify-channel.md`） |
| T102 | DONE | 词库合并语义结论落盘（`userdict-merge-semantics.md`，墓碑与 `dee` 必须保留） |
| T103 | DONE | 端点可达性结论落盘（`endpoint-reachability.md`，`*.workers.dev` 在境内 DNS 污染） |
| T104 | DONE | 测试工程骨架接线（`test/TestFluxingCloud/` 可构建可运行） |

### 阶段 B · 契约与客户端纯逻辑（批次 Y）— 全部 DONE

| 任务 | 状态 | 交付与证据 |
|---|---|---|
| T201 · 信封编解码单测 | **DONE 2026-11-10** | 单测段 [5]–[10] 共 **71 条**断言；①–⑤ 全覆盖（往返 / AAD 篡改 / nonce 长度 / base64 畸形 / 空与超长载荷）。反例清单见 §4 |
| T202 · 条目合并单测 | **DONE 2026-11-10** | 段 [13]（15）+ [14]（16）= **31 条**；收敛判据显式写成 `Check(fc::MergeEntries(b, a) == m, "⑤ 交换输入顺序结果相同（两端收敛）")`；另证幂等 2 条 / 结合律 2 条 / 空集恒等 3 条 |
| T203 · HLC 定序单测 | **DONE 2026-11-10** | 段 [11]（11 条：18 版本笛卡尔积 / 324 项反对称 / 153 项可区分 / 5832 项传递）+ [12]（14 条，含 counter 打满 `0xFFFF` 与时钟回拨）= **25 条** |
| T204 · 共享信封头落地 | **DONE 2026-11-10** | 新建 `include/FluxingCloudEnvelope.h`；单测改 `#include "../../include/FluxingCloudEnvelope.h"`；`vcxproj` 补 `<ClInclude>`；无第二份实现取证通过 |

### 阶段 C 起 — 全部 PENDING

`tasks.md` 第 101 行起：T211（Worker 骨架 + D1/R2）→ T212（认证）→ … → 阶段 G 验收。本批**未开始**任何阶段 C 工作（verified：`cloud/` 目录不存在）。

---

## 4. 判别性反例断言清单（T201 Done 判据的「至少 3 条」实证，实际远超）

全部为 `verified`（`TestFluxingCloud.exe` 实跑通过）：

**AAD 四项全覆盖**（改任一字段都必须 `kAuthFailed`）
- `hlc_ms + 1` ⇒ `kAuthFailed`
- `hlc_counter + 1` ⇒ `kAuthFailed`
- `type phrases → userdb` ⇒ `kAuthFailed`
- `item_key` 换文件名 ⇒ `kAuthFailed`

**密文与密钥**
- 密文首字节 `^= 0xFF` ⇒ `kAuthFailed`
- tag 末字节 `^= 0x01` ⇒ `kAuthFailed`
- 错误 `enc_key` ⇒ `kAuthFailed`

**nonce 长度**
- 11 字节 nonce ⇒ `SerializeEnvelope` 与 `OpenEnvelope` **双报** `kBadNonce`
- 线格式 nonce `"AAAAAAAAAAA="`（base64 解出 8 字节）⇒ `ParseEnvelope` `kBadNonce`

**base64 畸形**
- `cipher\t%%%%` ⇒ `kBadBase64`
- base64 截断一字符 ⇒ `kBadBase64`
- 解码端严格模式反例：`"abc"` / `"ab$d"` / `"QR=="` / `"Q=Q="` / `"===="` 全拒
- **防假绿正例**：`"QQ=="` / `"QUI="` 必须接受（防止「一律拒绝」也能让反例全绿）

**线格式**
- `ParseEnvelope("")` ⇒ `kEmpty`
- magic `v2` ⇒ `kBadMagic`
- 7 行 / 9 行 / `item_key`↔`type` 行交换 ⇒ `kBadField`
- `type\tbogus` ⇒ `kBadType`
- `hlc_ms\tabc`、`hlc_counter\t70000` ⇒ `kBadNumber`
- `item_key\ta\qyaml` ⇒ `kBadEscape`

**边界**
- 空载荷 ⇒ `kOk` 且 `cipher.size() == 16`（恰好一个 tag）
- `kMaxPayloadBytes + 1` ⇒ `kTooLong`
- 恰好 `kMaxPayloadBytes` ⇒ `kOk`（防把上限判定写成 `>=`）

**条目表**
- `""` ⇒ `kEmpty`；`fluxing-entries-v2` ⇒ `kBadMagic`
- 字段不足 5 / 空行 ⇒ `kBadField`
- payload `!!!!` ⇒ `kBadBase64`；device `d\q` ⇒ `kBadEscape`；ms 23 位 ⇒ `kBadNumber`

**非对称性（防「实现通了但语义反了」）**
- 同 key 同 HLC 但 payload 不同 ⇒ payload 字典序大者胜（确定性兜底）
- 合并输出**恒按 key 升序** —— 这是幂等 / 交换 / 结合三性质成立的全部原因

---

## 5. 证据路径

| 证据 | 位置 / 复现方式 |
|---|---|
| 单测运行结果 | `F:\soft\00selfmade\rime_claude\Release\TestFluxingCloud.exe`；结尾打印 `RESULT: PASS=134 FAIL=0` + `VERDICT: ALL PASS` |
| 构建 + 运行一键脚本 | `F:\soft\00selfmade\rime_claude\tmp\build-fluxing-cloud-tests.cmd`（674 B，**不入库**；x86/Release，显式传 `SolutionDir`） |
| 无第二份实现（①） | 测试文件内正则 `^(struct\|class\|inline\|static)\s.*(Hlc\|Envelope\|Entry\|Merge\|Base64\|Seal\|Open)` ⇒ **零命中**（verified） |
| 无第二份实现（②） | 全仓 `Weasel*\*.cpp/.h` / `RimeWithWeasel\*.cpp` / `include\*.h` / `test\*\*.cpp` 中 `CompareHlc` / `HlcTick` / `MergeEntries` **各仅 1 处定义**：`include/FluxingCloudEnvelope.h` 行 435 / 470 / 993（verified） |
| 无第二份实现（③） | 测试侧全部走 `fc::` 前缀，共 **271 处**；`TestMergeEntries()` 是测试函数名（767 行定义 / 946 行调用），不是合并实现 |
| 断言数对账 | 段 [1]=3 · [2]=1 · [3]=2 · [4]=1 · [5]=10 · [6]=8 · [7]=14 · [8]=7 · [9]=22 · [10]=10 · [11]=11 · [12]=14 · [13]=15 · [14]=16 ⇒ **合计 134**（与 `PASS=134` 逐项相符） |

---

## 6. 环境事实与不可从仓库推导的工具坑

### 6.1 构建与运行口径（本轮实测成功，可原样复跑）

`TestFluxingCloud` **不进主解决方案的默认构建**，必须单工程 msbuild 且**显式传 `SolutionDir`**（vcxproj 用 `$(SolutionDir)\include` 找生产头，而该工程不在 `Weasel.sln` 里 ⇒ 不传就找不到头）。脚本见 §5；PowerShell 调用与判读：

```powershell
& cmd.exe /c "F:\soft\00selfmade\rime_claude\tmp\build-fluxing-cloud-tests.cmd" 2>&1 |
  Select-String -Pattern 'BUILD|RESULT:|VERDICT:|\[FAIL\]|TEST_FLUXINGCLOUD_EXIT' |
  ForEach-Object { $_.Line }
```

脚本自身 `exit /b 0`，**真实判据看 `TEST_FLUXINGCLOUD_EXIT`**（不要看脚本退出码）。

### 6.2 本轮烧掉构建轮次的三个坑（下次直接避）

1. **`BCRYPT_AES_GCM_ALGORITHM` 不存在**（`error C2065`）。CNG 没有独立 AES-GCM 算法标识：必须 `BCryptOpenAlgorithmProvider(alg, BCRYPT_AES_ALGORITHM, nullptr, 0)`，再用 `BCRYPT_CHAINING_MODE = BCRYPT_CHAIN_MODE_GCM` 切模式（verified）。`BCRYPT_CHAIN_MODE_GCM` 是 `L"ChainingModeGCM"` 常量，长度用 `sizeof(BCRYPT_CHAIN_MODE_GCM)`（34 字节含 null）并需 `const_cast<wchar_t*>`。
2. **`STATUS_AUTH_TAG_MISMATCH` 只在 `<ntstatus.h>` 里**，而 `<ntstatus.h>` 与 `<windows.h>` 同包含会撞状态码定义。解法：自定字面常量 `constexpr NTSTATUS kStatusAuthTagMismatch = static_cast<NTSTATUS>(0xC000A002L);`，**不要 include `<ntstatus.h>`**（会被包含顺序坑死）。
3. **header-only 无前置声明 ⇒ 定义顺序即编译顺序**：`crypto::SecureZero` 被 `AesGcmSeal`/`AesGcmOpen` 调用，必须定义在它们**之前**。

### 6.3 全项目通用工具坑（本轮再次踩到）

| 坑 | 事实 | 规避 |
|---|---|---|
| `git commit -F -` | **不读 stdin**（`-F` 只接受文件名）；喂 here-string 报 `pathspec ... did not match any file(s)` | 消息写临时文件（放 gitignore 目录）再 `git commit -F <file>` |
| `edit` 多组替换 | 一次调用只接受**一组** `old_string`/`new_string`，JSON 重复键被静默丢弃，返回仍是 `1 block(s)` | 多处改动分多次调用，改完 `grep` 逐处核对落盘 |
| `web_search` | HTTP **402**「plan balance has run out」，死路 | 只能 `web_fetch` 抓厂商 `.md` 镜像；勿重试 |
| `Select-String -Path` 多路径 | 任一缺失 ⇒ 整条命令 code 1 且丢后续输出 | 先 `Test-Path`，或 `-LiteralPath` 单路径逐条查 |
| 本地安全策略 | 拦「先 `Set-Location` 改目录、再对旧路径写/删」，报 `write/delete targets cannot be verified safely after a command working directory change` | 一律参数式调用（`git -C <repo>`、全路径） |
| 文件换行判定 | `read` 工具显示无法区分 CRLF/LF | 必须读字节：`[System.IO.File]::ReadAllBytes` 后数 `` `r`n `` 与裸 `` `n ``。本仓 `TestFluxingCloud.vcxproj` = **纯 LF 无 BOM（208 个 LF）**；`output/install.nsi` 按 L09 必须 **UTF-8+BOM+CRLF** |
| PowerShell 按 `\r\n` 分割 LF 文件 | 整个文件当一行，表现为「行号全空」 | 先确认分隔符再分割 |
| 中文直打控制台 | cp936 乱码 | 写 UTF-8 文件再读，或 `SetConsoleOutputCP(CP_UTF8)`（测试 `main` 已这么做） |

### 6.4 本批新增的架构事实（改代码前必读）

- **信封线格式 = 文本，不是二进制**：首行 magic `v1`，其后严格 7 行 `<字段名>\t<值>`，**行序与行名都严格**。理由 = 排障可 dump、畸形输入可逐字段构造。**加字段必须升 magic 版本**，旧解析器遇未知行**显式失败**（不忽略）。
- **`payload` 在条目表里一律 base64**（phrases 文本与 userdb 二进制走同一路径），避免 TAB/换行破坏行结构；`item_key` / `device` 用 `\\` `\t` `\n` `\r` 转义保留人类可读性。
- **`item_key` 与 `device` 不得含 `|`**（AAD 用 `|` 分隔）。前提成立：item_key 是 Windows 相对路径（Windows 本身禁 `|`），device 是 `[0-9a-f]` 十六进制。
- **`kMaxPayloadBytes = 8 MiB` 是实现自定的防御性值**（plan 未规定字节数），依据 = 最大真实载荷（config 整份，数百 KB）留 20 倍余量。
- **中文错误串是错误分类的隐式契约**：`AesGcmSeal` 用 `err->find("kMaxPayloadBytes")` 反推 `kTooLong`，`OpenEnvelope` 用 `err->find("认证失败")` 反推 `kAuthFailed`。**改这两处文案会静默改动错误码**，必须同步改映射。
- **`HlcTick` 在 plan 之外加了两处防御性补充**（已验证）：counter 打满 `0xFFFF` ⇒ 推进 ms 一毫秒并归零；`now_ms < last_ms`（时钟回拨）⇒ 沿用 `last_ms` 并自增 counter。`HlcObserve` 额外在 `remote.ms == last_ms && remote.counter > c->counter` 时抬升 counter，且永不下调。
- **base64 解码为严格模式**：只允许末尾 `=`（最多 2 个）、总长必须 4 的倍数、填充位必须为 0。目的 = 让「密文被截断」能被抓出来。

---

## 7. 需要用户物理在场的项

| 项 | 原因 | 当前状态 |
|---|---|---|
| — | **本批无**：阶段 B 是纯逻辑 + 单测，无 UI、无装机、无真机交互 | — |

**未来会需要用户在场的**（提前记，免得到时忘）：
- 阶段 D/E 的账号页 UI 与真实邮箱验证码收取（需用户提供邮箱并查收）—— not verified。
- 阶段 G 验收的真机多端同步（需两台设备 or 一台设备 + 手工清库模拟）。

---

## 8. 本轮明确未做（避免下一会话误以为已做）

- **未接线到 `WeaselServer`**：`WeaselServer.exe` 里没有任何代码 `#include "FluxingCloudEnvelope.h"`（verified：全仓 grep 只在测试与头文件自身命中）。接线在阶段 D 之后。
- **未新增 IPC 消息**：`include/WeaselIPCData.h` 未改（verified）。
- **未创建 `cloud/` 目录**：阶段 C 服务端一字未写（verified）。
- **未跑主解决方案全量构建**：只构建了 `TestFluxingCloud.vcxproj`。主构建受不受影响 ⇒ inferred 不受影响（新增的是**未被任何生产 TU include 的头文件**，且 vcxproj 的 `<ClInclude>` 不参与编译），但**未实测**（not verified）。下一批若要先验，跑一次 `xbuild.bat` 全量即可。
- **既有脏文件未提交**（刻意）：`RimeWithWeasel/RimeWithWeasel.cpp`（假脏）、`librime` 子模块（假脏）、`docs/design/quickpanel-v6/design-demos/*.html`（未跟踪的三份设计稿）。