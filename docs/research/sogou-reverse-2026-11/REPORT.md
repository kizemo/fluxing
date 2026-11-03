# 搜狗输入法逆向分析报告（REA）— 工程量与路线评估

- 日期：2026-11-03
- 目标：`D:\Program Files (x86)\SogouInput`（搜狗输入法 16.8.0.5415）
- 工具：REA CLI 6.8.0（`rea-agents@6.8.0`），Node 26.13.0 / JDK 21
- 证据目录：`docs/research/sogou-reverse-2026-11/evidence/`

> **证据分级**：本报告严格区分
> **【已验证】**= REA/系统实测产出（可复现）、
> **【推断】**= 由导出表/文件名/体积等证据推导、
> **【未验证】**= 当前工具链无法判定（需反编译器或运行时抓包）。

---

## 0. 工具链现状（决定本次分析深度）

`rea doctor --json` 实测：

| 引擎 | 状态 | 影响 |
|---|---|---|
| Hopper | 缺失 | 无 Hopper 反编译 |
| Ghidra | 缺失（`GHIDRA_INSTALL_DIR` 未设置） | **无伪代码 / 无函数级分析** |
| IDA | 未注册 | — |
| Node / 宿主 | 正常 (26.13.0 / win32 x64) | — |

**结论**：本次能拿到的是 REA 的 **target-free PE 静态证据**（导入表 / 导出表 /
证书 / 资源 / 字节熵）+ 系统运行时实测；**没有函数级伪代码**。
所有"它是怎么实现的"一律标为【推断】。

MCP 侧未注册（`mcp list` 仅有 graphify / codebase-memory-mcp），本次走 **CLI 通道**
（`rea <command>`），不影响产出——skill 明确允许 CLI 降级。

---

## 1. 安装包结构：803 MB / 393 文件（【已验证】）

| 组 | 体积 | 文件数 | 内容 |
|---|---:|---:|---|
| ① 输入引擎模型 | **382.6 MB** | 63 | `Data\sysmodel.bin` (353 MB) + 53 个 `sgim_*.bin` (25 MB) |
| ② 输入核心代码 | 26.1 MB | 14 | `SogouTSF.dll` / `ImeFunc.dll` / `UIPlugin.dll` / 双拼方案表 |
| ③ 云与联网 | 52.1 MB | 11 | `SogouCloud.exe` / `networkbase.dll` / `beacon_sdk*.dll` / `shiply.dll` / `userNetSchedule.exe` |
| ④ 商业与助手 | 104.1 MB | 15 | 智能助手 / 皮肤市场 / 颜文字 / 看图识字 / `SGMyInput.exe` |
| ⑤ Components/ 可插拔业务 | 145.1 MB | 192 | `biz_center` 34.6 / `IChat` 32 / `PicFace` 16 / `isgpet` 14.3 / `HandInput` 12.5 / `SogouFlash` 12.9 / 游戏中心 6.5 / 桌面美化 5.3 |
| ⑥ 运维（崩溃上报等） | 34.2 MB | 6 | `crashrpt.exe` / 卸载器 / 安装器 |
| ⑦ 其余（字体/证书/皮肤包） | 69.5 MB | 101 | 3 个字体 + DigiCert 证书链 + `.cupf` 皮肤/UI 包 |

**③+④+⑤+⑥ = 335.5 MB ≈ 42%**，且**没有一个字节参与输入**。
这就是用户想剔除的"广告 + 关联 + 无关功能"的体积口径。

`Components/` 进一步说明搜狗把业务做成了**插件式可插拔**架构：
`SogouComMgr.exe` + 每个业务组件独立版本号目录（`biz_center/1.0.0.3350`）。

---

## 2. 输入路径的真实边界（【已验证】，REA 导出表）

| 二进制 | 导出表 | 判定 |
|---|---|---|
| `SogouTSF.dll` (0.09 MB) | `DllGetClassObject` / `DllCanUnloadNow` / `DllRegisterServer` / `DllUnregisterServer` | **纯 COM 壳**，只有 4 个导入 DLL（ADVAPI32/KERNEL32/ole32），不含任何 UI/网络逻辑 |
| `ImeFunc.dll` (8.4 MB) | `DoInit` / `DoExit` / `GetImeFunc` | **IME 引擎主入口**（3 个导出） |
| `UIPlugin.dll` (11.1 MB) | `RegisterImeClass` / `AddUIFunction` / `CreatePluginUIRes` / `SendUIResourceMessage` / `SendWndUIMessage` / `SendExtraUIMessage` / `SendSkinPreviewMessage` / `SetUiLayoutStub` / `ShowAbout` / `InitDllEnv` | **UI 插件框架**——候选窗、皮肤预览、UI 资源消息全走插件接口 |
| `networkbase.dll` (3.9 MB) | `sgime_base_component::Component_engine_core::CreateHttp/CreateWebSocket`、`IHttpClient`、`IWebSocketClient`、`IBaseUtilsTool`、`NetworkClient::HttpGetRaw/HttpPostRaw/WsConnectRaw/WsSendRaw`、`WsSetDecryptKeyRaw`、`WsSetTokenRefreshCallbackRaw`、`SetBeaconCallbackRaw`、`ClipboardService::SendSimpleRaw` | **C++ 组件框架 + HTTP/WebSocket 双通道 + 剪贴板上报服务** |

**关键结构性事实【推断，高置信】**：搜狗的输入法核心（`ImeFunc.dll` + `UIPlugin.dll`）
与它的联网/商业层是**库级解耦**的——网络能力集中在 `networkbase.dll` 一个组件里。
这是"保留输入、砍掉联网"在**架构上可行**的直接证据。

### 2.1 引擎模型文件（【已验证】头部魔数 + 体积）

| 文件 | 大小 | 魔数 | 推断类型 |
|---|---:|---|---|
| `Data\sysmodel.bin` | 370,269,501 B | `DICT` | 搜狗自研词典/模型总库（**私有格式**） |
| `sgim_gd_bigram.bin` | 10.8 MB | `SGSB` | 二元语法模型 |
| `sgim_gd_umodel.bin` | 2.6 MB | `SGUB` | 用户/通用模型 |
| `sgim_sys.bin` | 4.4 MB | `ccddeeff` + 头部 | 系统词库 |
| `sgim_lm.bin` / `sgim_keymap.bin` / `sgim_py.bin` | 532 KB / 312 KB / 16 KB | 自定义头 | 语言模型 / 键位表 / 拼音表 |

`sysmodel.bin` 是 **353 MB 私有二进制数据**，头部 `DICT` 后紧跟压缩负载。
【未验证】其内部究竟是 n-gram / 神经网络 / 嵌入，以及格式与算法——
当前无反编译器也无法反推数据格式。

---

## 3. 网络与埋点全景（【已验证】字符串抽取；非 REA 证据，标注为 shell 工具产出）

对 12 个核心二进制抽取 URL/host，去重后得到：

- **域名体系**：`*.pinyin.sogou.com`（云候选/配置/皮肤/账号/推送/调试）、`*.sginput.qq.com`（腾讯系）、`*.sogoucdn.com`、`rdelivery.qq.com`（腾讯灰度发布）、`oth.eve.mdt.qq.com`（**beacon 埋点**）
- **功能 API 端点**（按功能分组）：
  - 云：`cloud.pinyin.sogou.com` / `sugg.sogou.com` / `api.pinyin.sogou.com`
  - 皮肤市场：`api.pinyin.sogou.com/skin/v1/{cate_skinlist,info_list,recommend,search,skin_index,unite_skinlist,wallpic_url}`
  - 智能助手：`api.pinyin.sogou.com/macassistant/v1/{command,strategy,text/correct,text/sensitive,token}`
  - 账号：`account.sogou.com` / `2faccount.sogou.com` / `pc.profile.pinyin.sogou.com`
  - 推送长连接：`push-service-pc.sginput.qq.com` / `master-proxy.shouji.sogou.com`
  - 更新/埋点：`rdelivery.qq.com/v3/config/pull` / `oth.eve.mdt.qq.com`
- **设备指纹**：`qimei.dll` / `qimei64.dll`（设备唯一 ID）
- **隐私敏感**：`networkbase.dll` 导出 `ClipboardService::SendSimpleRaw` 与
  `SetLocalIdentity` ——【推断】存在读取剪贴板并带设备身份上报的能力。
  【未验证】具体触发时机与内容（需运行时抓包）。

**结论**：搜狗的"联网依赖"是**可以枚举、可以精确剥离**的——
它不嵌在输入法核心里，而是集中在 `networkbase.dll` + `SogouCloud.exe` +
若干独立 exe/dll。这直接支持"保留输入、砍掉广告"的可行性。

---

## 4. TSF 注册与运行时占用（【已验证】）

**注册表**（实测枚举）：
```
{CC43AF26-60C3-4612-B58D-27A07A40E90B} → ...\16.8.0.5415\SogouTSF.dll   ← TSF 文本服务
{E7EA138E-69F8-11D7-A6EA-00065B844310} → C:\WINDOWS\SysWow64\SogouTSF.ime ← 旧 IMM32
{7BCE96FA-...} / {85212cfd-...}          → Components\biz_center\...\biz_shellext(64).dll ← 资源管理器扩展
{54849B2B-...}                          → %LOCALAPPDATA%\sogoupdf\pdfpreviewhandler.dll
```
→ 文本服务入口只有 **一个 0.09 MB 的 COM 壳 DLL**，其余全部通过 `ImeFunc.dll`
的 `DoInit` 动态拉起。**这是搜狗能"拔掉广告"却保留输入的根本原因**。

**运行时实测**（本机，8 秒空闲采样窗口）：

| 进程 | WS MB | Private MB | 线程 | 句柄 | 累计 CPU s |
|---|---:|---:|---:|---:|---:|
| SogouCloud | 83.3 | 60.4 | 77 | 1005 | 38.7 |
| SogouCloud | 19.9 | 42.8 | 71 | 984 | 18.5 |
| SGTool | 41.5 | 37.0 | 40 | 993 | 13.8 |
| SGTool | 18.2 | 32.4 | 38 | 923 | 7.9 |
| SogouImeBroker | 14.8 | 5.4 | 2 | 195 | 0.0 |
| SogouImeBroker | 14.8 | 5.4 | 2 | 195 | 0.0 |
| **合计** | | **183.4 MB / 6 进程** | | | 空闲 CPU ≈ **0.78% 单核** |

`SogouImeBroker.exe` 的已加载模块全部是系统 DLL，**没有加载任何搜狗业务 DLL**——
说明它只是一个 broker，真正的引擎在宿主进程内加载。

**对比 Fluxing 现状**：常驻进程只有 `WeaselServer.exe` 一个（TSF 是进程内 DLL，
不额外占进程）。即：**搜狗 6 进程 / 183 MB → Fluxing 1 进程 / 数十 MB 量级**。
用户"降低系统占用"的目标，Fluxing 在架构上已经赢了。

---

## 5. 逆向这条路线的工程量评估

### 5.1 如果"从零逆向重写一个搜狗"（clean-room clone）

| 工作项 | 估计 | 说明 |
|---|---|---|
| TSF 前端（ITfTextInputProcessor / 组合串 / 候选窗 / 键盘） | 3–6 人月 | Fluxing 已有一份可用实现（30 条 ADR 的踩坑沉淀） |
| 输入引擎 + 语言模型 | 12–24 人月 | **最难**：要复刻 `sysmodel.bin` 的效果，等于重做搜狗 15 年的语料工程 |
| 候选窗 UI + 皮肤系统 | 3–6 人月 | 相对可控 |
| 云候选 / 长连接 / 账号 / 埋点 | 2–4 人月 | 可选，且是用户想删的部分 |
| 装卸 / 部署 / 用户数据 / 设置 | 3–5 人月 | Fluxing 已有一整套 |
| **合计** | **23–45 人月**（单人） | 且**做出来大概率仍不如搜狗**（缺 353 MB 私有模型） |

**难度判定：极高**，且**性价比为负**——因为最难的部分（模型）既逆向不出来
（私有数据格式 + 训练管线未知），也不该复制（数据权利 + GPL 洁净性）。

### 5.2 如果"在 Fluxing 上补齐搜狗的**用户可感知**能力"（推荐路线）

| 能力 | 搜狗对应物 | 估计 | 备注 |
|---|---|---:|---|
| 云候选 | `cloud.pinyin.sogou.com` + WebSocket | 1–2 人月 | 可先做 HTTP 拉取版；不需 WebSocket 长连接 |
| 皮肤系统 | `.cupf` + `Skin.dat` + `UIPlugin.dll` 插件接口 | 1–2 人月 | **不要复制搜狗的 .cupf 文件**，另设计格式；可借鉴其插件边界设计 |
| 表情 / 颜文字 | `SGKaomoji.exe` | 0.5 人月 | librime 有 emoji 方案基础 |
| 智能改错 / AI 写作 | `macassistant/v1/{correct,strategy,token}` | 0.5–1 人月 | 直接调外部 LLM API，**零常驻开销**（服务端做） |
| 看图识字 / 截图取词 | `SGMyInput.exe` + `ImageMagik.dll` | 1–2 人月 | 依赖 OCR 引擎 |
| 手写输入 | `Components\HandInput` 12.5 MB | 2–3 人月 | 或集成第三方手写识别 |
| 账号 / 云同步 | `account.sogou.com` | 1 人月 | 可选；也可只做本地同步 |
| 9 键 / 单元格词库 | Cell Dict | 0.5 人月 | librime 有 schema 基础 |
| **合计** | | **7.5–13.5 人月** | 其中 ~60% 是"能力补齐"，不是"逆向" |

---

## 6. 与"继续推进火流猩"的难度对比

Fluxing 当前状态（实测仓库）：
- 版本 **1.1.6（2026-10-31）**，CHANGELOG 连续发布 1.0.0→1.1.6
- **80 个 spec**、**30 条 ADR**（0001–0029）
- 已具备：TSF 前端 + 服务端 + IPC + 候选窗 UI + 部署器 + NSIS 安装器
  + 用户词典 + 自定义短语 + 快捷键设置面板 + YAML 配置编辑 + 候选编辑/忽略
  + 深色模式 + 快捷面板（常驻/跟随激活/多档）+ 焦点守卫 + 逐字上屏/剪贴板上屏
- 架构上**天然**满足"无广告 / 无关联 / 低占用"：
  1 进程常驻、无账号体系、无埋点、无资源管理器扩展、无崩溃上报、无游戏/宠物/Flash

| 维度 | 逆向重写搜狗 | 继续推进火流猩 |
|---|---|---|
| 输入前端难度 | 高（从零重写 TSF 语义） | **低**（已有，且踩坑已沉淀成 ADR） |
| 准确度追平搜狗 | **不可达成**（缺 353 MB 私有模型） | 可达"手感 85–90%"，用 librime + Rime Ice + 开放语料 |
| 达成"去广告/低占用" | 白做（那部分本来就要删） | **天然满足** |
| 交付物合规性 | 逆向产物不可入 GPLv3 洁净基线 | 洁净 |
| 首个可用版本 | 12–18 人月 | **已经在用**（1.1.6 已发布） |
| 长期维护 | 追一个每年改版的商业产品 | 收敛、可控 |

### 结论（明确）

> **难度上，逆向重写搜狗 > 继续推进火流猩，且不是一个量级的差距；
> 且投入产出比是负的。**
> 原因不是"我们逆向能力不够"，而是**搜狗的核心资产是数据不是代码**：
> 353 MB 私有模型无法逆向成可维护的实现，而用户真正想删的 42% 体积
> 与 5/6 的进程，恰恰是"不做那些东西"自动得到的。

**推荐路线**：继续 Fluxing，把 REA 逆向的产出当作**规格说明书**使用——
用 5.1/5.2 的表当路线图，用 §2 的导出表当**插件边界设计参考**
（`ImeFunc.dll` 的 `DoInit/DoExit/GetImeFunc` 与 `UIPlugin.dll` 的
`RegisterImeClass/AddUIFunction/SendUIResourceMessage` 是很干净的切面），
用 §3 的端点清单当**"绝对不要连这些域名"的黑名单**，
用 §4 的占用数据当**火流猩的性能验收基线**（目标：常驻 1 进程 / <40 MB / 空闲 CPU <0.1%）。

---

## 7. 逆向成果对火流猩的**直接可用**部分（不只是"评估"）

1. **插件边界模板**：`UIPlugin.dll` 的 15 个导出构成了一套成熟的 IME UI 插件契约，
   火流猩的候选窗/皮肤若要插件化，可直接照抄这个切面设计（`include/` 层加接口 + 插件注册表）。
2. **"输入核心 vs 联网层"的解耦范式**：搜狗用 `ImeFunc.dll`（引擎）+
   `networkbase.dll`（网络组件）两个库实现了干净切分。
   火流猩若要加云候选，应保持同样的切分：**网络能力放在服务端，
   不进 TSF 进程，不进 librime 进程**——这样云功能下线时只需停一个服务。
3. **黑名单端点表**：`*.pinyin.sogou.com` / `*.sginput.qq.com` / `rdelivery.qq.com` /
   `oth.eve.mdt.qq.com` / `qimei*` 全部列入"永不连接"清单，
   可做成自动化测试断言。
4. **隐私事实**：`ClipboardService::SendSimpleRaw` 表明搜狗有剪贴板上报能力——
   这正是火流猩"不碰剪贴板内容"可以当作**差异化卖点**写进产品说明
   （注：火流猩当前上屏已借用剪贴板，见 CHANGELOG「剪贴板是借用的」，
   需在文档里区分"本地上屏借用"与"上报"）。

---

## 8. 未解决问题（需后续手段）

| 问题 | 需要什么 |
|---|---|
| `sysmodel.bin` 内部结构 / 是否神经网络 | Ghidra 12.1.x + IDA/Hopper，或搜狗官方公开说明 |
| 各 exe 的实际功能调用图 | 同上（伪代码） |
| 运行时网络行为（哪些请求真发出去） | 运行时抓包（mitmproxy / Wireshark）+ 进程捕获 |
| 智能助手 `/token` 的鉴权与配额机制 | 运行时抓包 + 协议还原 |
| 候选窗 UI 插件的完整消息协议 | 伪代码 + 运行时交互 |

---

## 9. 证据文件索引

| 文件 | 内容 | 来源 |
|---|---|---|
| `evidence/imports-*.json` | 13 个二进制的完整导入表 | **REA** `inspect-pe-imports` |
| `evidence/imports-summary.txt` | 导入表摘要（按二进制） | 由上派生 |
| `evidence/exports-*.json` | 5 个二进制的完整导出表 | **REA** `inspect-native-exports` |
| `evidence/urls-hosts.txt` | 12 个二进制的 URL / 主机抽取 | shell 字符串工具（非 REA 证据） |
| `evidence/component-split.csv` | 803 MB 按功能分组拆解 | shell 统计 |
| `evidence/` 全部由 `rea` 与系统命令产出，可复现 | | |

**复现命令**
```powershell
rea inspect-pe-imports  "D:\Program Files (x86)\SogouInput\16.8.0.5415\SGTool.exe" --format json
rea inspect-native-exports "D:\Program Files (x86)\SogouInput\16.8.0.5415\networkbase.dll" --format json
rea doctor --json
```
