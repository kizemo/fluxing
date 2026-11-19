# 批次 AU · 「清空云端」后 sidecar 未重置（本地词典再也传不上去）+ 出包 1.3.3 + 重置口令恢复指引（交接文档）

> 批次性质 = **代码实现批次**（缺陷修复 + 文案 + 出包）。未改 `librime/`、未改 IPC 协议、未新增第三方依赖 ⇒ 不触发「IPC 四边同步」红线。
> 上一批（AT）交接指针：`docs/archive/handoffs/handoff-2026-11-19-batch-at-account-page-ui-refactor.md`（末尾「附：批次 AT-2 追加」与「A9」两段）。

---

## 1. 本批目标（承接 AT-2 的未闭环项）

1. **出包 1.3.3** —— AT-1 / AT-2 的客户端代码从未装机，1.3.2 包不含本批代码；真机验收的硬前置。
2. **修好「清空云端数据后本地词典再也传不上去」** —— AT-2 遗留的、与决策无关的真缺陷。
3. **给「重置口令后云端旧密文不可解」补上可执行的非破坏性恢复指引** —— 不代用户拍板 A/B/C，但把**不依赖该决策**的部分落好。

---

## 2. 缺陷取证链（AU-1 · 已闭环）

### 2.1 现象

用户在「设备与云数据」窗点「清空云端数据」⇒ 云端清空成功 ⇒ 但**本地词典再也传不上去**（云端一直是空的）。

### 2.2 根因（源码级坐实）

| 环节 | 事实 | 证据 |
|---|---|---|
| UI 路径 | `CloudAccountController::SubmitDeleteCloudData` 只调 `impl->service->DeleteCloudData(&err)` | `WeaselServer/Cloud/CloudAccountDialog.cpp:708` |
| sidecar 契约 | sidecar 记录「哪些词条已同步」⇒ 未重置 ⇒ 本地词条被判为「已同步」⇒ **不作为新增推送** | `WeaselServer/Cloud/CloudSyncService.h`（`CloudSyncCoordinator` 契约） |
| 修复函数 | `CloudSyncCoordinator::ResetLocalSyncState` **零调用者、零测试**（`WeaselServer/**` + `test/**` 全树只有定义 `CloudSyncService.cpp:1255` 与声明 `CloudSyncService.h:290`） | 全树 grep |
| 协调器生命周期 | 协调器**每轮同步现建现用**（`CloudAccountDialog.cpp:341` 构造）⇒ UI 侧**没有长期实例**可调 ⇒ 抽成自由函数 | `CloudAccountDialog.cpp` |

### 2.3 为什么此前没人发现

清空云端 + 本地重推这条路径**从未被端到端走通**；侧车落点真相源 = `ResolveFluxingUserDataPath()`（读注册表 `RimeUserDir`，回退 `%AppData%\fluxing`），本机实际落点 `D:\Program Files\fluxing\user1\fluxing\fluxing-cloud-sync.state`。

---

## 3. 落码清单（4 个文件）

### 3.1 修改

| 文件 | 改动 |
|---|---|
| `WeaselServer/Cloud/CloudSyncService.h` | 新增自由函数声明 `bool ResetCloudSyncSidecarFile(const std::string& sidecar_path, std::string* err);`（置于 `class CloudSyncCoordinator` 之前，含「为何是自由函数」的注释） |
| `WeaselServer/Cloud/CloudSyncService.cpp` | 新增 `ResetCloudSyncSidecarFile` 实现（覆盖 sidecar 为 `#fluxing-cloud-sync-v1` + `seq\t0`）；`CloudSyncCoordinator::ResetLocalSyncState` 改为**委托**该函数（同一份实现，UI 与协调器不各写一遍） |
| `WeaselServer/Cloud/CloudAccountDialog.cpp` | ① 匿名命名空间 helper `CloudSyncSidecarPath()`（`ToUtf8(ResolveFluxingUserDataPath() / L"fluxing-cloud-sync.state")`）；② `RunSyncRound` 里原先 3 行 sidecar 路径解析改用它；③ `SubmitDeleteCloudData` 重写：清空成功 ⇒ 重置 sidecar ⇒ 成功则 `impl->engine->NotifyLocalChange()`（`engine_mutex` 作用域独立，不与 `impl->mutex` 嵌套）⇒ 三分支状态文案（成功 / sidecar 重置失败 / 清空失败）；④ `SubmitResetPassword` 成功文案改为可执行恢复指引 |
| `test/TestFluxingCloud/TestFluxingCloud.cpp` | 新增 `TestCloudSidecarReset()`（用 `<cstdio>` 的 `fopen/fputs/fread/remove`，**未新增 include**），6 组断言；已在 `main()` 中 `TestCloudMaintenanceGate()` 之后注册调用 |

### 3.2 版本真相源（4 处）

`version.txt` / `env.bat` / `weasel.props` / `CHANGELOG.md` 顶部：1.3.2 → **1.3.3**。

- `env.bat`：`FLUXING_VERSION=1.3.3`、`PRODUCT_VERSION=1.3.3.0`、`FILE_VERSION=1.3.3.0`、`VERSION_PATCH=3`。
- `weasel.props`：`<VERSION_PATCH>3`、`<PRODUCT_VERSION>1.3.3.0`、`<FILE_VERSION>1.3.3.0`。
- `WEASEL_BUILD` 保持 0 ⇒ `weaselx64.1.3.3.0.dll` 从未存在 ⇒ **无 L107 风险**。
- `env.bat` / `weasel.props` 被 gitignore ⇒ **不入提交**；`version.txt` 入提交。

---

## 4. 恢复指引（AU-2）

### 4.1 事实

重置口令 ⇒ 服务端换 `auth_key` ⇒ 本地 `enc_key` 随之变化 ⇒ R2 上旧密文**永不可解**（服务端**永不见** `enc_key`、**不参与** 任何 KDF ⇒ 没有代解的余地）。`CloudSyncService.cpp:564` 的 `crypto::AesGcmOpen` 失败 ⇒ 整轮 pull 失败（「解密云端条目失败」）。

### 4.2 本批落好的可执行路径（**非破坏性**）

用新口令登录 → 「设备与云数据」→ 点「清空云端数据」→ 本地 sidecar 重置 → 本地词库整库重推。

- 「清空云端」是**不可逆**删除，但**由用户亲手点击发起** ⇒ 程序不代劳（不擅自删用户数据）。
- 找回口令窗状态行文案：「密码已重置，请用新密码登录。旧云端数据已无法解密，可登录后在「设备与云数据」清空后重传。」（状态行 376×40 = 两行区，放得下）

### 4.3 仍待用户拍板（AT-2 遗留 A/B/C，本批**不代决**）

- **A** 重置即清空云端（服务端自动删 ⇒ 不可逆数据删除 ⇒ 需用户明确同意）
- **B** 只换 `auth_key` 保留 `enc_key` ⇒ **违反红线**，不建议
- **C** 客户端两段式重推全量

---

## 5. 门禁数字（本轮独立复跑）

| 项 | 结果 |
|---|---|
| `build.bat weasel installer` | **EXIT=0**；MSBuild **0 警告 0 错误**；makensis **8 条 `warning 7010`**（既有可选产物告警：ARM/ARM64 产物 + `data\*.gram` + `data\lua\*.txt`） |
| 云单测 | **`RESULT: PASS=502 FAIL=0`** / `VERDICT: ALL PASS`（基线 492 ⇒ **+10**） |
| 版本资源 | 5 个二进制全部 **1.3.3.0**：`output\Win32\WeaselServer.exe`、`output\Win32\WeaselDeployer.exe`、`output\weasel.dll`、`output\weaselx64.dll`、`output\WeaselSetup.exe` |
| 产物 | `output/archives/fluxing-1.3.3-installer.exe` —— 42968958 B · sha16 `20A8B6754ED3C656` |
| 包内二进制 | 7z 解出 `WeaselServer.exe` **2748416 B / filever `1.3.3.0`**（1.3.2 包内同法解出为 `1.3.2.0` ⇒ **判别通过**） |
| AU 代码在包内 | 1.3.3 包内可检出 AU 新文案（如「云端数据已清空（账号本身保留）。本地词库会重新上传，」「密码已重置，请用新密码登录。旧云端数据已无法解密，」），1.3.2 包内**无** ⇒ 判别通过 |

---

## 6. 已验证 / 推断 / 未验证

### 6.1 已验证（实测）

- 构建 EXIT=0、0 警告 0 错误；云单测 502/0；版本资源四处一致 1.3.3；包内 `WeaselServer.exe` filever 1.3.3.0；AU 新文案在 1.3.3 包内可检出、1.3.2 包内无。
- sidecar 重置的 6 组断言在单测里真跑（重置后行全消失 / `seq` 归零 / 版本行保留 / 空路径视为成功 / 不可写路径失败并有中文原因 / 协调器成员同语义）。

### 6.2 推断（未实测）

- 1.3.3 包内 AU 代码「能修好真实场景的清空后重推」——**未在真机端到端走通**（需装包 + 用户在场）。
- 状态行两行放得下新文案 —— 依据布局真相源（`CloudAccountUi.h` 状态行 376×40 / 456×40）与 Win32 静态控件默认换行行为，**未目视**。

### 6.3 未验证 / 未闭环

- **未装机**（`WeaselServer.exe` 是 **PPL** ⇒ 装机前必须注销 / 重启，否则仍跑旧映像；磁盘哈希不能证明在跑的是新码）。
- 用户在场 AC：账号页两态目视 / 验证码独立窗 / 注册 / 找回密码发信 / 「保存账号」勾与不勾两次登录 / 重置口令后重新登录与清空重推。
- 双机 AC 7 条（AC-03/04/05/08/09/13/14）+ restore 端到端 + 输入延迟两组对照。
- **AT-2 产品级决策 A/B/C**（见 §4.3）。
- 仓库累计提交**未 push**（无 push 授权）。

---

## 7. 证据路径

- 构建日志：`F:\soft\00selfmade\rime_claude\tmp\au-build-3.log`（本轮最终）、`tmp\au-build-1.log`（首次，版本资源不符故作废）、`tmp\au-build-2.log`
- 云单测日志：`F:\soft\00selfmade\rime_claude\tmp\au-cloud-tests-1.log`（`RESULT: PASS=502 FAIL=0`）
- 包内解包：`tmp\au-pkg133\WeaselServer.exe`（1.3.3 包内）/ `tmp\au-pkg132\WeaselServer.exe`（1.3.2 包内，对照）
- 上一批真相源：`docs/archive/handoffs/handoff-2026-11-19-batch-at-account-page-ui-refactor.md`
- ADR：`docs/adr/0064-server-password-reset-endpoints.md`（Proposed）、`docs/adr/0063-account-page-information-architecture-and-state-views.md`
- 规格任务：`.specify/specs/100-fluxing-cloud-account-sync/tasks.md`（批次 AT 段 + 批次 AU 段）

---

## 8. 本轮工具坑（勿重试）

1. **改 `weasel.props` 不触发 MSBuild 重编译** ⇒ `.rc` 不重编 ⇒ 版本资源停在旧版本号 ⇒ 必须 **touch 4 个 `.rc`**（只改 mtime；`WeaselServer.rc` 是 UTF-16LE+BOM+CRLF ⇒ 禁止重写内容）。
2. **`version.txt` 是 5 字节无尾换行** ⇒ 改版本要按 HEAD 字节**等长替换**（`1.3.2`→`1.3.3` 等长，安全）。
3. **`env.bat` / `weasel.props` 用 `set "KEY=VALUE"` / XML 标签格式**；本轮用 **Latin1(28591) 字节往返**做替换 ⇒ 除替换外字节逐字不变（BOM / 行尾 / 中文注释全保住）。曾漏改 `PRODUCT_VERSION`/`FILE_VERSION`/`VERSION_PATCH` ⇒ **改版本必须全键位复查**，只看 `FLUXING_VERSION` 会漏。
4. **`CHANGELOG.md` 是 CRLF（7530 CRLF / 0 bare LF）+ UTF-8 无 BOM**；`tasks.md` / `docs/**` 是 **UTF-8 无 BOM + LF** ⇒ 写文件时必须区别处理（本轮用 here-string + 显式 LF→CRLF 转换）。
5. **二进制判别令牌**：中文窄字符串在产物里是 **UTF-8 字节**，宽字符串是 **UTF-16LE** ⇒ 判别时要两种编码都试（本轮 `NEW_ONLY` 差集法有效）。
6. `git commit -F -` **不吃 stdin** ⇒ 提交消息写临时文件；`cd` 后写入会被安全策略拦 ⇒ 一律绝对路径 + `git -C`。
7. 安全策略：`rd /s` 字样会被拦；正则里出现 `\\(output|librime|\.git)\\` 会被判成 UNC 网络共享路径 ⇒ 改用 `-notlike '*output*'`。
8. 全仓 grep **必须给窄路径**，否则超时。

---

## 9. 下一批建议顺序

1. **装机 1.3.3**（先注销 / 重启，因 PPL）⇒ 用户在场 AC（重点：重置口令 → 用新口令登录 → 「设备与云数据」清空云端 → 「立即同步」→ 云端重新出现本地词条 = AU-1 的端到端闭环）。
2. **AT-2 决策 A/B/C** 落定（若选 A：服务端在 confirm 成功后清空该账号的 R2 数据 + 写明审计；本批已把客户端侧的「清空 + 重推」路径修通，缺的是服务端是否自动清）。
3. 双机 AC 7 条 + restore 端到端 + 输入延迟两组对照（需第二台设备 / 第二个 Windows 账户 —— 注意 `CloudCredentialStore` 是 Windows 用户级全局单例）。
4. push（累计提交未 push，需用户授权）。

> 接力 prompt 按用户 2026-11-08 指示**直接给在当轮回答正文**，不另落文件。
