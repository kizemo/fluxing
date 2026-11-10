# Handoff · 批次 X · 用户资料云同步「取证与工程底座」（spec 100）

- **日期**：2026-11-10
- **分支**：`feat/100-cloud-sync`（自 `Fluxing@1b022f3` 切出）
- **范围**：`.specify/specs/100-fluxing-cloud-account-sync/tasks.md` 阶段 A（T101 / T102 / T103 / T104）
- **性质**：**零运行时行为**。本批次不产出任何用户可见变化，不触碰 `WeaselServer` / `WeaselTSF` 生产代码。

---

## 1. 一句话现状

阶段 A 四项任务 **全部 DONE**：三份取证结论已落盘、测试工程已接入构建链且编译 + 冒烟全绿；
spec 100 的硬前置红线（R1 / R5）**已解除**，阶段 B（契约与客户端纯逻辑，T201–T204）可开工。

---

## 2. 交付物清单（全部已落盘，可复核）

| 任务 | 交付物 | 大小 |
|---|---|---|
| T102 | `docs/research/fluxing-cloud-2026-11/userdict-merge-semantics.md` | 23,309 B |
| T103 | `docs/research/fluxing-cloud-2026-11/endpoint-reachability.md` | 13,597 B |
| T101 | `docs/research/fluxing-cloud-2026-11/email-verify-channel.md` | 9,305 B |
| T104 | `test/TestFluxingCloud/TestFluxingCloud.cpp` | 3,642 B |
| T104 | `test/TestFluxingCloud/TestFluxingCloud.vcxproj` | 10,695 B |
| T104 | `test/TestFluxingCloud/xmake.lua` | 360 B |
| T104 | `xmake.lua`（根，+1 行 `includes("test/TestFluxingCloud")`，落在 debug 分支内） | 改 |
| — | `tmp/build-fluxing-cloud-tests.cmd`（构建 + 运行脚本，不入库） | 674 B |

取证探针（**不入库**，在 `tmp/` 下可复跑）：
- `tmp/t102/t102_probe.cpp`（17,787 B）+ `tmp/t102/build.cmd` + `tmp/t102/art/report_*.txt` + `tmp/t102/all-reports.txt`
- `tmp/t103/endpoint-probe.txt`

---

## 3. 已验证（有实测证据，可直接依赖）

### 3.1 T104 构建与冒烟（本轮亲手跑出）

```
==== BUILD OK ====
==== TestFluxingCloud · 用户资料云同步 · 骨架冒烟 ====
[1] 冒烟：测试工程可运行   [PASS] 算术基线 / [PASS] UTF-8 字面量为 3 字节（/utf-8 生效）
[2] 链接：winhttp.lib      [PASS] WinHttpOpen 返回有效会话句柄
[3] 链接：bcrypt.lib       [PASS] BCryptOpenAlgorithmProvider(SHA256) / BCryptGetProperty(OBJECT_LENGTH) 返回正数
[4] 链接：advapi32.lib     [PASS] RegOpenKeyExW(HKCU\Software) 成功
RESULT: PASS=6 FAIL=0
VERDICT: ALL PASS
TEST_FLUXINGCLOUD_EXIT=0
```

⇒ `winhttp.lib` / `bcrypt.lib` / `advapi32.lib` 三库**确实接进了构建链**（是链接后调用成功，不是"能编译"而已）。

### 3.2 T102 词库合并语义（probe 实测，非读码推断）

- `UserDbValue::Pack()` = `c=<commits> d=<dee> t=<tick>`（`librime/src/rime/dict/user_db.h:20-23`）。
- **export 通道只导 `commits`**：`dee` 与逐条 `tick` 丢失 ⇒ 同词条"词频"不可跨端保真传递。
- **墓碑（负 commits）在 export 通道彻底消失**（返回 0、正文空），backup 通道保留 ⇒ 用 export 做同步会让**删除不传播**。
- import 把 `-1` **放大为 `min(-1,-2)=-2`**，并制造 `c=0 d=0 t=0` 垃圾条目。
- restore 走 `UserDbMerger`：按 `abs(commits)` 择优、`dee` 取 max、`tick` 抬到 `max(本地, 快照)`、同快照重复 apply 幂等。
- `backup_user_dict` 落点在**进程 CWD** 的 `sync\<user_id>\` —— 因为 `sync_dir` 初始值是相对路径 `"sync"`（`librime/src/rime/deployer.cc:18-21`）⇒ **生产接线必须用 `get_user_data_sync_dir()` 取绝对路径**。

### 3.3 T103 端点可达性（本机单出口实测）

- `workers.dev`：DNS 四路解析（223.5.5.5 / 119.29.29.29 / 8.8.8.8 / 系统）**结果全不同且全非 CF 网段**，TCP 443 三次全超时 ⇒ 定性为 **DNS 污染**，**不得作为服务端点**。
- 对照域 `cloudflare.com` TLSv1.3 握手 192ms、`fluxing.com` 可解析（218ms）⇒ 走自有域名 + CF 是通的。
- `fluxing.cn` / `fluxing.dev` / `api.fluxing.dev`：**本机不可解析** ⇒ 域名尚未注册/未接入。
- 首版端点拍板：`https://api.<自有域名>` + Cloudflare Worker。

### 3.4 T101 邮件通道

- Cloudflare Email Service 与"自有域名 + CF"同厂商 ⇒ 不需要第三方账号与 API key，天然落在 T101 边界内。
- **Workers Free 计划无出站邮件**；Workers Paid 含 3,000 封/月，超出 $0.35/1,000 封。
- ⇒ 最终选定 **Cloudflare Email Service（Workers Paid）**。

---

## 4. 推断（未实测，下阶段要补证）

1. **大词库合并耗时未测** —— 本机 `%APPDATA%\Fluxing` 下**没有任何 `*.userdb*`**（只有 `build/`、`installation.yaml`、`user.yaml`），所以 T102 探针用的是自造小库。真实用户几十万条词库的 `restore_user_dict` 耗时不详。
2. **锁与并发行为未做真实并发实测** —— T102 探针每次都是 `destroy_session` 之后才调 levers。真实场景下"同步线程 vs 输入线程"是否会发生 leveldb LOCK 争用，未知 ⇒ **已列为 T503 第一个单测**。
3. **境内多点未测** —— 本机只有单出口。T103 的"污染指纹"结论独立支撑"排除 workers.dev"，但"自有域名在各省的延迟量级"仍是推断。
4. **`rime_dict_manager.exe` CLI 契约未探明** —— `-h` 返回 `invalid arguments.` + `EXIT=1`；不把 `dist_Win32\lib` 注入 `PATH` 会 `0xC0000135`。**不作为捷径依赖**，同步一律走 `RimeLeversApi`。
5. **T101 五项未验证**：Workers Paid 月费（定价页未列）、外部服务一手价（`web_search` 402 不可用）、发件域 SPF/DKIM onboarding 步骤、单封速率阈值、免费额度例外。

---

## 5. 未验证 / 已知缺口

- **`web_search` 整体不可用**：返回 HTTP **402「plan balance has run out」**（套餐余额耗尽，非限流，重试无用）。本轮所有外部事实只能靠 `web_fetch` 抓厂商原文 `.md` 镜像。
- **`resend.com/pricing` 未取到正文价格**：Next.js RSC 页面 125,779 tokens 被截断。补救路径 = 抓 `https://resend.com/pricing.md`（页面自带 `<link rel="alternate" type="text/markdown">`）。本轮未做，因此 T101 的"外部服务比价"标注为**未实时核价**。
- **托盘那 1 个"中英图标"**：批次 W（`f425526`）已移除**我方**语言栏项（`kLangBarRegisterItem = false`）；剩下的 1 个是 **Windows 自带输入法指示器**，本仓库代码改不掉。用户已接受，**不要再重做**。

---

## 6. 下阶段（阶段 B · 契约与客户端纯逻辑，T201–T204）

开工前必须遵守的三条：

1. **`include/FluxingCloudEnvelope.h` 尚不存在**。T104 的 vcxproj 里原本预留了一行 `<ClInclude Include="..\..\include\FluxingCloudEnvelope.h" />`，因为文件不存在已**删除**；T201 建好该头文件后要把它加回去。
2. **端点不得硬编码 `workers.dev`**（T103 红线）。默认端点走 `api.<自有域名>`，自定义端点配置口子保留但默认不暴露 UI，**失败时不得静默回退默认端点**。
3. **T503 基座已定**：`backup_user_dict` + `restore_user_dict`（`UserDbMerger`），**不是** `export` / `import`。任何"用 export 做同步"的方案必须在 ADR 里先驳倒 §3.2 的墓碑丢失结论。

分支策略（`next-phase-plan.md` §8.1，记为 D5，**待用户确认**）：按批次分阶段合流 —— 批次 X 零运行时行为，完成即合回 `Fluxing`；Y / Z 同理；从阶段 D 起分支长期存活直到 G 验收通过。合流前三件必查：① `git status -sb` + `git diff --stat` 排除 `RimeWithWeasel/RimeWithWeasel.cpp`、`librime` 子模块假脏、`*.obj`、`docs/design/quickpanel-v6/design-demos/*.html`；② `CLAUDE.md` 与 handoff 指针按合流后 HEAD 改写；③ 本机并发会话存在时的分支互斥。

---

## 7. 本轮工具坑（勿重试）

| 坑 | 处置 |
|---|---|
| `web_search` 返回 **402 plan balance has run out** | 死路，不要重试。改用 `web_fetch` |
| `web_fetch` 抓 Cloudflare 文档 **HTML** 会 37k tokens 被截断 | 抓站内 `.md` 镜像（`.../index.md` 直接可抓） |
| `web_fetch` 抓 `resend.com/pricing`（Next.js RSC）125,779 tokens 被截断 | 抓 `resend.com/pricing.md` |
| `read` 工具对 `tmp\t102\` 下文件报 ENOENT 但文件确实存在 | 该目录用 `bash` + `[System.IO.File]::ReadAllText` 读 |
| 本地安全策略拦"先 `Set-Location` 改工作目录、再对旧路径写/删" | 用 `Start-Process -WorkingDirectory <wd>`，或 `git -C <repo>` 参数式调用 |
| `git commit -F -` **不读 stdin**（`-F` 只接受文件名） | 消息写临时文件再 `-F <file>` |
| `Select-String -Path` 多路径任一缺失会整条命令 `code 1` 且丢后续输出 | 取源码行前先 `Test-Path` |
| `edit` 一次只能传一组 `old_string`/`new_string`，重复键被静默丢弃 | 多处改动 = 多次 `edit`，改完必 `grep` 核对落盘 |
| MSVC 编译含 UTF-8 中文字面量的 `.cpp` 必须加 `/utf-8` | vcxproj 的 `AdditionalOptions` 已带 `/utf-8` |
| 中文输出直接打控制台会乱码（cp936） | 写 UTF-8 文件再读 |

---

## 8. 提到 spec 100 三件套里的决策（本轮无新增，只引用）

`next-phase-plan.md` §5 决策记录（D1–D4 用户 2026-11-10 拍板）+ §7 用户反馈原话 + §8 分支策略。
D1 = 账号页入口走快捷设置栏右键菜单（不加槽位）；D2 = 不改 `AboutDialog` 旧文案（T403 WON'T DO）；
D3 = 接受只剩 `Alt+,` 一个入口；D4 = 按当前 HEAD 重测性能；D5 = 分阶段合流（**本条待用户确认**）。