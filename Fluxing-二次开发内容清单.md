# Fluxing 二次开发内容清单

> **这是什么**：`rime/weasel`（小狼毫）→ **Fluxing / 火流猩输入法** 的全部二次开发内容盘点。
> 按能力域列出**哪些已完成**、**哪些部分完成**、**哪些待完成**，每条都带可核对的证据位置。
>
> **怎么用**：直接在本文档里改。
> - 改**状态列**（`✅` / `🟡` / `⬜` / `⛔` / `⚠️`）来更新进度；
> - 在 [§7 待补充开发内容](#7-待补充开发内容) 里加你要开发的新条目；
> - 每加一条，**同时**补一行「验收方式」——没有验收方式的功能不进这份清单。
>
> **本轮状态**：2026-10-28 快照。当前版本 **0.21.0.9**（CHANGELOG 已记录，安装包已构建于
> `output/archives/`，**尚未进 `release/` 发布链**）。最近完成 **T1 裸 Shift 切中英文**（ADR 0010 / spec 082），
> 提交 `ceff6b6`，已推送至 `kizemo/Fluxing`（HEAD `ceff6b6`）。
> **权威沙箱门禁在本机跑不起来**（Windows Sandbox 虚机不启动，见 §4.3 T19）
> ⇒ 0.21.0.9 的真机验收**只能由人手敲**（spec 082 §3.4 六条）。
> 依据：相对上游 `master` 的 `git diff --numstat master...Fluxing`、CHANGELOG 版本节、66 份 spec 的 tasks 统计。

---

## 0. 基线事实

| 项 | 值 | 依据 |
|---|---|---|
| 上游基线 | `rime/weasel` **`master`** 分支（0.17.4 系） | `origin` remote，只读 |
| 分叉分支 | `Fluxing` → 推 `kizemo/Fluxing` | 宪法 P8 品牌分叉豁免 |
| 引擎 | `librime`（git submodule，**不改**） | 宪法 P3 |
| 当前版本线 | `0.21.0.9`（CHANGELOG 顶部） | 80+ 个 fork 版本节 |
| fork 版本节总数 | **83** 个（0.18.0 → 0.21.0.9） | `CHANGELOG.md` 标题统计 |
| 相对上游变更规模 | **1097 个文件**，`+2,760,215 / -3,723` 行 | `git diff --shortstat master...Fluxing` |
| 其中生产代码（C++/头） | **+13,706 / -1,093** 行，11 个模块目录 | 同上，按目录汇总 |
| 其中测试代码 | **+18,815 / -390** 行，123 个文件 | `test/` |
| 其中部署配置与预编译词典 | `+2,560,101` 行，87 个文件 | `output/`（含 `data/` 方案与词典） |
| vendored 插件 | `+36,768` 行，104 个文件 | `thirdparty/librime-lua/` |
| 文档与规格 | `+45,892`（`.specify/`）+ `+29,515`（`docs/`） | — |
| 构建脚本 | `+4,588`（`scripts/`）+ `+5,538`（`tools/`） | — |

**体量判读**：生产代码只占全量差异的 0.5%，其余是**配置、词典、文档、测试**。
这说明二次开发的主体不是「重写输入法」，而是**在 Weasel 骨架上叠加品牌、配置、UI 组件、
测试门禁与运维能力**——评估工作量时不要按 276 万行估。

---

## 1. 能力域总览

| 域 | 代号 | 状态 | 一句话 | 主要证据 |
|---|---|---|---|---|
| A 品牌与安装体系 | A1–A7 | 🟡 | 品牌、路径、注册表、静默安装已成型；首启引导未做 | spec 000/002/011/013 |
| B 构建与发布流水线 | B1–B6 | 🟡 | 双构建 + CI 四 job + 沙箱工装齐备；arm64 缺目标 | spec 003、CI |
| C 快捷键与输入语义 | C1–C8 | 🟡 | 死绑定已清零、门禁已建立；裸 Shift 切中英已**实施**（待真机验收） | ADR 0007/0009/0010、spec 077/081/082 |
| D 候选窗 UI 与组件库 | D1–D6 | 🟡 | 组件库可用、暗色主题 ship；动画与 Token 收敛未完成 | spec 033/034/037/039/040/041 |
| E 托盘与快捷设置面板 | E1–E7 | 🟡 | v0 面板 ship、自动弹出 ship；v4/v3-rev3/长显未收口 | spec 036/049/052/070/080 |
| F 候选字编辑 | F1–F4 | ✅ | 右键删除/屏蔽五阶段完成，托盘可恢复 | spec 008/028/030/031/032 |
| G 常用短语 | G1–G4 | 🟡 | 独立进程对话框 ship；UI 打磨未收口 | spec 051、PRD F5 |
| H 设置与配置 UI | H1–H5 | 🟡 | 快捷键页 MVP ship；样式/方案/词典三页未做 | spec 007/024/044/045/050 |
| I 运维与更新渠道 | I1–I5 | 🟡 | Sparkle 改源、404 修复；孤儿恢复未验证 | spec 042、0.21.0.2 |
| J 编码与文档修复 | J1–J3 | ✅ | 仓库历史编码损坏已修 | spec 078/079 |
| K 工程能力（门禁/事故库） | K1–K6 | ✅ | CI 门禁、沙箱工装、事故库、skills 校准 | `scripts/`、`lessons-learned.md` |
| L 计划中未启动 | L1–L8 | ⬜ | 云同步、引导、Emoji、剪贴板历史等 | PRD §3.2/§3.3 |

---

## 2. 已完成

### A. 品牌与安装体系

| 编号 | 内容 | 状态 | 证据 |
|---|---|---|---|
| A1 | 品牌分叉：产品名、CLSID、Profile GUID、注册表键、二进制名全部换血 | ✅ | spec 000；宪法 P8 |
| A2 | 安装路径与注册表契约（`InstallDir` / `RimeUserDir` / `Uninstall`） | ✅ | spec 002；`AGENTS.md` §2.5 断言 a–g |
| A3 | 用户数据目录**一次性**迁移（`%AppData%\Rime` → `%AppData%\Fluxing`，`HKCU\...\ImportedFrom` 记账） | ✅ | 宪法 P6；`CONTEXT.md` §4-1 |
| A4 | `/D=` 静默安装路径覆盖修复 | ✅ | spec 013；L17 / L112 |
| A5 | 静默安装强制显式 `/D=<path>` + 先清 `HKLM\Software\Fluxing\Weasel` 与 `HKCU\Software\Fluxing` | ✅ | L13 / L54；`AGENTS.md` §2.5 |
| A6 | NSIS 脚本 UTF-8 BOM(`EF BB BF`) + 100% CRLF | ✅ | L09 |
| A7 | 升级**不再需要重启**：16 处 `Delete /REBOOTOK` 改写、版本化 shim、64/32 位 regsvr32 分离、移除 ctfmon/TextInputHost taskkill 循环 | ✅ | 0.21.0.3；md5 三方全等 + PendingFileRenameOperations 零新增 |

### B. 构建与发布流水线

| 编号 | 内容 | 状态 | 证据 |
|---|---|---|---|
| B1 | Windows 双构建路径（msbuild `build.bat` + xmake `xbuild.bat`），CI 双跑 | ✅ | spec 003；CI `build (msbuild)` / `build (xmake)` |
| B2 | 发布构建**统一走 MSBuild**，消除「装上就注册不上」（xmake 链 `VCOMP140.DLL` 依赖） | ✅ | 0.21.0.6；L127 / L128 |
| B3 | CI 四 job：`lint` / `test` / `build (msbuild)` / `build (xmake)`（+ `sandbox-gate`） | ✅ | `.github/workflows/ci.yml`；两轮 run 全 success |
| B4 | 本地一键测试套件 `scripts/test-infra/run-test-suite.bat`（16 个测试工程） | ✅ | `=== ALL TESTS PASSED ===` |
| B5 | Windows Sandbox 一次性虚机验收工装（~46 项断言，仓外） | ✅ | `F:\soft\00selfmade\sandbox-verify\rime-claude\` |
| B6 | 版本线管理：`build.bat`/`xbuild.bat` 的 `VERSION_*` 为追踪版本线 | ✅ | `AGENTS.md` §3.3；L145 |

### C. 快捷键与输入语义

| 编号 | 内容 | 状态 | 证据 |
|---|---|---|---|
| C1 | 默认快捷键 rev2（`,`/`.` 翻页、Shift_L/R 选 2/3 候选、Shift+space 切中英、Ctrl+Shift+numbersign/dollar） | ✅ | spec 005；`TestDefaultHotkeys` 89/89 |
| C2 | Shift 释放选候选**重写到 IPC 状态机**（8 次 yaml 修复失败后的根因重写） | ✅ | spec 077 / ADR 0007；0.21.0.1 |
| C3 | 快捷键「可触发性」门禁 `TestHotkeyBindings`（含布局探测、keycode 解析、死绑定判定） | ✅ | spec 081 / ADR 0009 |
| C4 | 4 条 `Shift+标点` 死绑定**改名修好**（`Shift+quotedbl` / `bar` / `braceleft` / `braceright`） | ✅ | 0.21.0.8；`bindings=39\|reachable=39\|dead=0` |
| C5 | 2 条永不触发的重复快捷键删除（`Control+Shift+9` / `Control+Shift+0`） | ✅ | 0.21.0.7；L148 |
| C6 | 4 条 `send:` 多字节 UTF-8 死绑定移除 | ✅ | 0.21.0.6；L113 |
| C7 | 死绑定基线**清零**（4 → 0）——空基线 = 最严阈值，任何死绑定直接让构建变红 | ✅ | `known-dead-bindings.txt`；5 场景负对照 M0–M4 |
| C8 | 裸 Shift 切中英文（**上下文消歧**：候选窗没开 ⇒ 切中英；候选窗开着 ⇒ 仍选第 2/3 候选） | ✅ | ADR 0010 / spec 082；0.21.0.9；`TestShiftIPCStateMachine` 28 PASS / 0 FAIL |
| C9 | 裸 Shift 的 3 个缺陷修复：**候选窗开着时真正上屏第 2/3 候选**、**跨会话隔离**（A 切 A、B 不动）、**语言栏中/英图标随模式切换**；另版本号 4 段改 3 段 | ✅ | ADR 0012；1.0.0；`TestShiftIPCStateMachine` 34 PASS / 0 FAIL；`TestWeaselIPC /verify-shift-body` PASS；部署产物门禁 `bindings=39\|reachable=39\|dead=0` |

### D. 候选窗 UI 与组件库

| 编号 | 内容 | 状态 | 证据 |
|---|---|---|---|
| D1 | `FluxingComponents` v0：7 个 mac 风控件（Button/Toggle/Panel/Label/D2DRenderer/Theme/stdafx） | ✅ | spec 037；0.18.25.0 |
| D2 | `QuickPanelDialog` 重构到 FluxingComponents | ✅ | spec 038 |
| D3 | 暗色主题跟随系统（`FluxingDarkModeBridge` + 广播测试），含 0x001E1E1E palette 字节级核验 | ✅ | spec 033/034；0.18.22.0；L42 |
| D4 | 多 DPI 验证（100/150/200） | ✅ | spec 041；0.18.28.0 |

### E. 托盘与快捷设置面板

| 编号 | 内容 | 状态 | 证据 |
|---|---|---|---|
| E1 | 托盘图标 + 快速设置面板 v0（单进程内嵌，8 入口 mac 风完整化） | ✅ | spec 036/045；0.18.29.0 |
| E2 | 切换到 Fluxing 时**自动弹出**面板（补回 `AdviseSink` 注册，零 I/O 合规） | ✅ | spec 080；0.21.0.5；`panel hwnd=… visible=1 notifications=4` |
| E3 | 第二次及以后显示不再自动隐藏（`SetTimer` 复用路径缺失） | ✅ | 0.21.0.5；`first/second auto-hide` 双轮 PASS |
| E4 | 失焦自动隐藏 + 2 秒宽限 + 鼠标移开 1.5 秒 | ✅ | spec 036；`TestWeaselIPC /verify-quickpanel` |

### F. 候选字编辑

| 编号 | 内容 | 状态 | 证据 |
|---|---|---|---|
| F1 | 右键派发链路（TSF 事件 → panel → 命中测试） | ✅ | spec 030；0.18.21.0 |
| F2 | 候选字**删除**（走 `Memory::OnDeleteEntry`） | ✅ | spec 028 |
| F3 | 候选字**屏蔽**（客户端过滤） | ✅ | spec 031 |
| F4 | 托盘「恢复」按钮还原误删（user.db + phrases.json 备份） | ✅ | spec 006 US2；`TestTrayRestoreIgnored` |

### G. 常用短语

| 编号 | 内容 | 状态 | 证据 |
|---|---|---|---|
| G1 | 常用短语**独立于用户词典**的 `custom_phrase.txt` 机制 + 编译进 `custom_phrase.table.bin` | ✅ | `project-knowledge.md` §4.1 |
| G2 | 短语对话框**独立进程** `FluxingPhrasesDialog.exe`（+2,471 行） | ✅ | spec 051；0.19.0.52 |
| G3 | 触发热键 Alt+. / Alt+/ | ✅ | PRD F5 |

### H. 设置与配置 UI

| 编号 | 内容 | 状态 | 证据 |
|---|---|---|---|
| H1 | `YamlRoundTrip`：yaml-cpp 封装，**保留 key 顺序**，支持 sequence index | ✅ | spec 024；`FluxingConfigEditor/` +1,185 行 |
| H2 | yaml 快捷键可视化编辑器 **MVP（仅第 1 页「快捷键」）** | ✅ | spec 050；0.18.30 |
| H3 | 快捷键设置界面的**内置/默认快捷键表**与出货配置同步 | ✅ | spec 045；0.21.0.7 同步孪生键 |
| H4 | 用户词典导入/导出（TSV 路径，RimeLeversApi） | ✅ | `WeaselDeployer/DictManagementDialog.cpp` |

### I. 运维与更新渠道

| 编号 | 内容 | 状态 | 证据 |
|---|---|---|---|
| I1 | Sparkle 更新源改指自有渠道 `aiec.fun`（4 个 APPCAST 字节表） | ✅ | 0.21.0.2；二进制内核验 `rime.github.io` 归零 |
| I2 | `download.php` 找不到版本时返回 **404** 而非错版本二进制 | ✅ | 0.21.0.2 |

### J. 编码与文档修复

| 编号 | 内容 | 状态 | 证据 |
|---|---|---|---|
| J1 | `lessons-learned.md` / `CHANGELOG.md` 历史编码损坏修复 | ✅ | spec 078 |
| J2 | 仓库剩余 14 个文件的 C 转义损坏修复 | ✅ | spec 079（39/39 tasks） |
| J3 | 说明书 md + html 同步；修掉 3 处「界面在骗人」的过时描述 | ✅ | 0.21.0.8；`Rime-设置说明书-重点快捷键.{md,html}` |

### K. 工程能力（这一域是本 fork 最实的资产）

| 编号 | 内容 | 状态 | 证据 |
|---|---|---|---|
| K1 | 快捷键可触发性门禁 + **空基线**（最严阈值） | ✅ | `test/TestHotkeyBindings/`；spec 081 |
| K2 | `TestDefaultHotkeys` 89 条断言，含**否定断言**（钉住旧写法不许复活） | ✅ | 0.21.0.8 |
| K3 | 判别力负对照方法论（造坏数据证明门禁真会红） | ✅ | 5 场景 M0–M4 全过 |
| K4 | 事故库 `lessons-learned.md` L01–L149，每条带根因 | ✅ | `.specify/memory/lessons-learned.md` |
| K5 | ADR 机制（0001/0007/0008/0009/0010） | ✅ | `docs/adr/` |
| K6 | 入库的 rime-skill 校准（不再教人写死绑定） | ✅ | `rime-skill/` 24 文件；安装副本逐字节一致 |

---

## 3. 部分完成（要收口的部分）

| 编号 | 内容 | 状态 | 已完成部分 | 缺口 | 依据 |
|---|---|---|---|---|---|
| P-A1 | 首启引导 / 现代化安装器 | 🟡 | spec 011 设计完成（2/13） | 实际引导流程未实现 | PRD F13（v2.2+） |
| P-B1 | ARM64 构建目标 | 🟡 | x64 TSF 正常出包 | `build (xmake)` arm64 目标缺失 | T2.13 |
| P-D1 | FluxingComponents 动画扩展 | 🟡 | hover/press 已做 | Slider/Dropdown/Checkbox/Radio 未做 | spec 039（0/40） |
| P-D2 | Fluent UI Token 收敛 | 🟡 | 3/41 tasks | 主体未做 | spec 040 |
| P-E1 | QuickPanel v4 macOS 风格 | 🟡 | 7/10 tasks | 收尾未做 | spec 049 |
| P-E2 | QuickPanel v3-rev3 重新启用 | 🟡 | 设计+部分 | 未收口 | spec 070（0/25） |
| P-E3 | QuickPanel 长显模式 | 🟡 | spec 084 + ADR 0013 已实现（菜单常驻 + 20% 淡显 / 悬停变清晰） | 待真机验收（T013） | spec 084 / ADR 0013 |
| P-G1 | 常用短语 UI 打磨 | 🟡 | 独立进程 + 过滤可用 | UI 细节未收口 | spec 042-phrases-ui / 043 |
| P-H1 | yaml 设置 UI **剩余 3 页** | 🟡 | 第 1 页（快捷键）ship | 样式 / 方案 / 用户词典 3 页未做 | spec 007；PRD F3 |

---

## 4. 待完成

### 4.1 已立项、已设计、有明确技术路径

| 编号 | 内容 | 优先级 | 阻塞/依赖 | 验收方式 |
|---|---|---|---|---|
| T1 | **裸 Shift 切换中英文**（ADR 0010） | P0 | 无 | `TestShiftIPCStateMachine` 扩展 + 真机按键 | ✅ **已完成**（2026-10-27，见 §2 C8） |
| T2 | 设置界面抓键路径 **layout-aware 改造**（现在用虚拟键码拼 `accept:`，能造出 `Control+Shift+9` 这类死绑定） | P0 | 无 | 门禁 + 界面生成绑定全部可触发。🟡 **已定稿**（ADR 0011 `Proposed` + spec 083，2026-10-28）：死绑定已用门禁对照实验坐实（`Control+Shift+9` ⇒ `DEAD_KEYCODE`，前端实际发 `0x0028`）；**M1 共享键名单元已落地**（2026-10-28，`include/KeyNameMapper.h` + 门禁 `--selfcheck-mapper`，见 §4.1.1）；**M2 抓键改造已落地**（2026-10-28，`fluxing::CaptureAccept` + 对话框改吃共享单元，见 §4.1.2）；**M3 第二条路径已落地**（2026-10-28，`WeaselServer/ShortcutSettings.cpp` 切共享单元 + 冲突检测改语义比较，见 §4.1.3）；**M4 已落地**（2026-10-28，抓键产物专项 fixture + 门禁保真模式 `--selfcheck-recorder`，见 §4.1.4）；**M5 已完成**；**M6 已落地**（2026-10-28，装器载荷 `data\build\` 全量重编 + 新装包，见 §4.1.5）|
| T3 | yaml 设置 UI：样式页 / 方案页 / 用户词典页 | P1 | T2 | 界面改动后 `bindings` 数与门禁一致 |
| T4 | QuickPanel v4 / v3-rev3 / 长显模式收口 | P2 | 无 | 截图 + 沙箱断言 |
| T5 | FluxingComponents 动画 + Fluent Token 收敛 | P2 | 无 | 视觉回归 |

### 4.1.1 T2 · M1 里程碑（2026-10-28 已完成，ADR 0011 D1）

spec 083 的 **M1（共享单元 `include/KeyNameMapper.h`）** 与 **M5（门禁复跑）** 已完成：

| 项 | 落点 | 状态 |
|---|---|---|
| 共享单元 | `include/KeyNameMapper.h`（`namespace fluxing`，header-only，无 GUI / 无 rime_api 依赖） | ✅ |
| 键名表 | `include/keysyms_generated.inc`（include guard + `fluxing::keysyms` 命名空间 + 函数内 `static` 表）；生成器仍是 `test/TestHotkeyBindings/gen-keysyms.ps1` | ✅ |
| 前端复用 | `WeaselTSF/KeyEvent.cpp` 改为调用 `fluxing::ResolveCharacter`（掩码逻辑逐字保留，行为不变） | ✅ |
| 门禁复用 | `test/TestHotkeyBindings` 删本地副本改吃共享单元；新增 `--selfcheck-mapper`（表名回环 / accept 回环 / 18 固定和弦 / 14 解析边界 / 前端全组合枚举） | ✅ |
| 门禁数字 | 仓库源 `bindings=39\|reachable=39\|dead=0`；部署产物源 YAML 同；部署产物**重编后** `bindings=337\|reachable=337\|dead=0`；负控 `bindings=9\|reachable=3\|dead=6`；`MAPPER-SUMMARY\|failures=0`；全套件 `ALL TESTS PASSED` | ✅ |

两条坐实的事实：**librime 键表只有大写 A–Z** ⇒ `Control+a` 解析不了（F1 坐实）；
**修饰键本身不能构成 `accept:`**，必须拒绝而不是改写（D6）。
**M2 已完成**（见 §4.1.2）；**M3 已完成**（见 §4.1.3）；**M4 已完成**（见 §4.1.4）；**M5 已完成**（门禁复跑随每个里程碑重跑一遍）；**M6 已完成**（见 §4.1.5；只剩实机验收 T6.3 与 ADR 0011 转正 T6.4，二者都要用户真机按键）。

### 4.1.2 T2 · M2 里程碑（2026-10-28 已完成，ADR 0011 D1/D3/D6）

spec 083 的 **M2（抓键改造 `FluxingConfigEditor/KeyRecorder.cpp`）** 已完成：

| 项 | 落点 | 状态 |
|---|---|---|
| 决策收口 | `include/KeyNameMapper.h` 新增 `fluxing::CaptureAccept()` / `CapturedAccept`，另加 `kAcceptWinKey`、`kAcceptNoCharacter`、`ParseAcceptWide()`、UTF-8↔UTF-16 辅助（顺带修掉 `BuildAccept` 宽字符重载的逐字节加宽） | ✅ |
| 对话框 | `FluxingConfigEditor/KeyRecorder.cpp` 整文件重写：删除本地 `MapKeyCodeToLibrimeName`，改调 `CaptureAccept`；字符与修饰键取自**同一次** `GetKeyboardState()` 快照 | ✅ |
| 拒绝路径 | Win 键 ⇒ `kAcceptWinKey`（前端从不上报 Win）；修饰键自身 ⇒ `kAcceptModifierKey`；librime 无名 + 带修饰键 ⇒ `kAcceptNoName`（提示「请按住 Shift 再试」） | ✅ |
| 中文提示 | 对话框内 `RefusalText()`（宪法 P2）；共享单元错误码保持 ASCII | ✅ |
| 门禁 | `--selfcheck-mapper` 新增第 6 类「捕获一致性」（vkey 0x08–0xFE × ext/shift/ctrl/alt 全枚举，逐条比对 `CaptureAccept` 与前端 `ConvertKeyEvent`）+ 第 7 类 8 个固定断言 | ✅ |
| 死键兜底 | 同一和弦连解两次，结果不一致即判为「布局含死键」并跳过（`dead_key_skipped` 计数），避免把布局噪声误判成抓键缺陷 | ✅ |
| 附带修掉 | 对话框占位提示原本是乱码（「亟慑组合键...」），已改回「请按组合键...」 | ✅ |

**未决依赖**：修饰键只输出 **Shift / Control / Alt**、**不输出 Lock** —— CapsLock 维度
属清单 **T20**（ADR 0011 Q2），必须先实测再定，本里程碑不预判。
（**已结案**：T20 实测结论是「引擎整链中断」，修法为前端不再上报锁存状态 + 门禁纳入 caps 维度，见 4.1.6 与 ADR 0014。）

### 4.1.3 T2 · M3 里程碑（2026-10-28 已完成，ADR 0011 D1/D3）

spec 083 的 **M3（第二条路径 `WeaselServer/ShortcutSettings.cpp`）** 已完成：

| 项 | 落点 | 状态 |
|---|---|---|
| 第二条路径 | `WeaselServer/ShortcutSettings.cpp`：删掉自造的 `VKeyToName` / `NormalizeCombo`，改调 `fluxing::CaptureAccept()`；低阶钩子用 `KBDLLHOOKSTRUCT` 造 `BYTE[256]` 快照，`KeyInfo` 由 `scanCode<<16 \| LLKHF_EXTENDED` 构造 | ✅ |
| 内置表 | **一个字没动**（T3.3）：`Control+Shift+F` 保持原样 | ✅ |
| 冲突检测 | `CheckConflict()` 两处字符串精确比较 → **语义比较** `fluxing::AcceptEquals()`（keycode + modifier 掩码） | ✅ |
| 拒绝文案 | 共享单元新增 `fluxing::AcceptErrorMessage()`，两条路径共用同一套中文文案（宪法 P2） | ✅ |
| 用户可见 | 抓到的组合键实时显示在 popover + 面板状态行；**拒绝时清空 pending chord** 并说明原因 | ✅ |
| 门禁 | `--selfcheck-mapper` 新增第 8 类「第二路径语义一致性」（14 条内置表用例 + `equal` / `Shift+plus` 矛盾用例），输出 `MAPPER\|builtin_equivalents=15` | ✅ |

**顺带消掉的三处漂移**：`Shift`+`=` 从「永远写 `equal`（永不触发）」改成 `Shift+plus`；`Ctrl`+小写字母从「写出 librime 解析不了的名字（死绑定）」改成**拒绝**；`Super+`（Win）从「写出伪绑定」改成**拒绝**。

**门禁复跑（M5）**：源门禁 `bindings=39|reachable=39|dead=0`；部署产物重编 12 个 schema 后 `bindings=337|reachable=337|dead=0`；`MAPPER-SUMMARY|failures=0`；全套件 `ALL TESTS PASSED`；整解 Release/Win32 编译 exit 0。

**未决依赖**：修饰键仍**不输出 Lock** —— CapsLock 维度属清单 **T20**（ADR 0011 Q2），必须先实测再定。（**已结案**：见 4.1.6 与 ADR 0014。）

### 4.1.4 T2 · M4 里程碑（2026-10-28 已完成，ADR 0011 D3）

spec 083 的 **M4（抓键产物专项）** 已完成：

| 项 | 落点 | 状态 |
|---|---|---|
| fixture | `test/TestHotkeyBindings/fixtures/recorder-output.yaml`：20 条**真实抓键产物**（`a` / `Shift+A` / `Control+1` / `Alt+comma` / `Shift+Control+F` / `Shift+Alt+F` / `Shift+plus` / `space` / `Return` / `F1` / `Shift+Control+numbersign` / `comma` / `Tab` / `Shift+Tab` / `grave` …）+ 负控 `Control+Shift+9` | ✅ |
| 判定清单 | 同目录 `recorder-output.expect.txt`：20 条逐条 `REACHABLE` + 负控 `DEAD_KEYCODE` | ✅ |
| 保真门禁 | 门禁新增 `--selfcheck-recorder`：① 抓键产出的每个 accept 都必须**可达**；② fixture 里每条 `REACHABLE` 必须**真能被抓出来**；③ 负控必须**抓不出来** | ✅ |
| 产物导出 | 门禁新增 `--dump-recorder`：导出抓键真实产出的 accept（850 条），fixture 由它生成 ⇒ fixture 不可能与实现漂移 | ✅ |
| CI 接线 | `scripts/test-infra/run-test-suite.bat` 增加 2 次门禁调用（逐条判定 + 保真自检）⇒ 自动进 CI `test` job，不必改 `.github/workflows` | ✅ |

**门禁数字**：`RECORDER|products=850|rows=21|produced=20|not_produced=0|negative_ok=1`；`RECORDER-SUMMARY|failures=0`；fixture 判定 `bindings=21|reachable=20|dead=1|mismatches=0|unused=0`；源门禁 `bindings=39|reachable=39|dead=0`。

**未决依赖**：修饰键仍**不输出 Lock** —— CapsLock 维度属清单 **T20**（ADR 0011 Q2），必须先实测再定。（**已结案**：见 4.1.6 与 ADR 0014。）

### 4.1.5 T2 · M6 里程碑（2026-10-28 已完成，ADR 0011 D3）

spec 083 的 **M6（打包）** 已完成 —— 装包产出 + 载荷复验，并把「装器多带了一份陈旧编译产物」这条遗留一起清掉：

| 项 | 落点 | 状态 |
|---|---|---|
| 版本号 | `env.bat`：`FLUXING_VERSION=1.0.0` / `RELEASE_BUILD=1`（gitignored；原计划的 `0.21.0.10` 已被 ADR 0012 的 3 段化取代） | ✅ |
| 装包 | `build.bat weasel installer` ⇒ `output\archives\fluxing-1.0.0-installer.exe`（42,756,263 B, SHA256 F040D989872FB8D135917B9416E40DE4D3010002EB669EF43AA98544A8344A8A） | ✅ |
| 载荷重编 | 装器载荷 `data\build\` **29 个文件全量重编**（`rime_deployer --build` 从当前 `output\data` 源重编）：陈旧 `default.yaml` 的 2 条死绑定 ⇒ **0** | ✅ |
| 产物复跑 | 载荷 `data\build\*.yaml` 门禁 `325/325/0` PASS；载荷 `data\*.yaml` + `user-custom` 门禁 `40/40/0` PASS；29 个载荷文件与工作区 SHA256 **全 MATCH**；7 个二进制 **全 MATCH** | ✅ |
| 可回滚 | 旧包装档为 `fluxing-1.0.0-installer.pre-m4.exe`（SHA256 `B1C17AB8…0808F`） | ✅ |
| 实机验收 | T6.3 交用户按键验收 | ⏳ 等用户 |
| ADR 转正 | T6.4 实测通过后 ADR 0011 `Proposed` → `Accepted` | ⏳ 等 T6.3 |

**为什么必须重编而不是删掉**：装器 `output\install.nsi` 用 `File "data\build\*.bin"` / `File "data\build\*.yaml"` 收录预编译产物，**两条都不是 `/nonfatal`** ⇒ 目录空了 NSIS 直接 abort，不能靠删除规避。

### 4.1.6 T20 · CapsLock 维度结案（2026-10-28 已修复，ADR 0014 D1）

- **根因**：不是「按键匹配失配」，而是**输入引擎在 CapsLock 打开后不再处理任何按键**。
  前端 `WeaselTSF/KeyEvent.cpp` 在 CapsLock 打开时给按键事件多置 `LOCK_MASK`(0x2) ⇒
  `expand_ibus_modifier`（`RimeWithWeasel.cpp:44-46`）原样保留该位 ⇒ librime 精确比较
  modifier（`librime/src/rime/key_event.h:43-44`）失配；**更关键**的是本项目配置
  `good_old_caps_lock: true`（`output/data/default.yaml:72-73`）让英文输入组件对
  「带着锁存标记」的键返回 `kRejected` ⇒ **整条处理器链中断**
  （`librime/src/rime/engine.cc:105-107`）⇒ 候选选择 / 翻页 / 切中英 / 切标点 /
  切简繁**全部**失效。
- **修法**：取唯一源头 —— 前端不再上报锁存状态（ADR 0014 D1）。CapsLock 键自身的
  切中英 / 交还系统行为不变（D2），字母仍产出大写（D3），引擎不碰（D6）。
- **门禁**：新增 CapsLock 维度（开 / 关两态分别枚举）+ 内建负控（模拟修复前，必须
  严格更差）。数字对照见 spec 085 §5.3.1。
- **残留限制**：写成小写字母的快捷键在 CapsLock 打开时仍不触发（ADR 0014 D5），
  绕行写法是把配置写成大写变体（如 `Control+A`）。

### 4.1.7 T21 / T22 · 1.1.0 真机反馈的另两条缺陷（2026-10-28 已修复，ADR 0015）

- **T21 · 裸 Shift 上屏候选左右颠倒**（用户反馈 5）
  - **根因**：线缆约定（`wParam bit0 = is_left，0=L / 1=R`）在客户端与服务端
    **各写了一份字面量**（`is_left ? 0u : 1u` / `(wParam & 1u) != 0`），
    客户端写对、**服务端读反** ⇒ 送进状态机的「是否左 Shift」与真实按键相反 ⇒
    左 Shift 走右 Shift 分支（选第 3 个）。前端判左右、键名映射、状态机映射
    **全部正确**，错点唯一。
  - **修法**：编解码收进共享单元 `include/ShiftStateMachine.h` 的一对纯函数
    `EncodeShiftIsLeft` / `DecodeShiftIsLeft`，两端各调一个（ADR 0015 D2）⇒
    单端漂移在结构上不再可能。
  - **回归**：`test/TestShiftIPCStateMachine` 新增 `Test_EncodeDecode_RoundTrip`，
    **直接钉位值**（左编码=0、右编码=1、`Decode(0)==true`、`Decode(1)==false`）⇒
    连「两端一起错」也能证伪（ADR 0015 D3）。
- **T22 · 输入法弹出菜单缺「快速面板常驻」**（用户反馈 4）
  - **根因**：**菜单有两份资源** —— 服务端托盘图标菜单（`WeaselServer/WeaselServer.rc`，
    上一条已加该项）与输入法前端弹出菜单（`WeaselTSF/WeaselTSF.rc`，分简 / 繁 / 英
    三块，**从头到尾没有该项**）；且该命令编号只在服务端自己的头文件里，
    共享头 `include/resource.h` 没有 ⇒ 前端连写都写不出来。路由不缺，服务端处理器
    已在（`WeaselServerApp.cpp:371`）。
  - **修法**：共享头补 `ID_QUICKPANEL_ALWAYS_SHOW 40019`（与服务端取值一致），
    前端三个弹出菜单各补一项（繁体用「快速面板常駐」）。**不扩协议**（复用既有
    托盘命令通路，ADR 0015 D4–D6）。
  - **验证**：**对构建产物做 PE 资源级检查**（`weasel.dll` / `weaselx64.dll` 的
    MENU 105 / 108 / 109 是否含该项），不只看源码。
- **T23 · 装机时 64 位前端 DLL 被占用未替换**（用户反馈 1 / 2 的真因）
  - 装机目录里 `weaselx64.dll` 仍是**前一天**的旧件，而
    `weaselx64.1.1.0.0.dll`（新件）与 `weaselx64.dll.stage2` 标记都在 ⇒
    安装器的「TSF 被占用 ⇒ 装 versioned shim + 开机补换」机制**正常工作**，
    只是**机器尚未重启** ⇒ 64 位进程仍在加载旧件 ⇒ 旧件里还带着
    CapsLock 锁存标记上报 ⇒ 引擎整链中断 ⇒ 候选窗消失 / 快捷键全失效。
  - **结论**：反馈 1 / 2 是**部署侧问题，不是代码缺陷**（spec 085 / ADR 0014 的修复
    已在包内的新件里）。⇒ **重启 Windows** 即完成换装；重启后复测，若仍失败再另立条目。

### 4.1.8 T24 / T25 / T26 / T27 · 1.1.0 第二轮真机复测的收口条目（2026-10-29 部分修复，ADR 0016）

- **T24 · 勾了「快速面板常驻」仍会随输入焦点切换消失**（用户反馈 A）
  - **根因**：面板共有 **9 个收起发起点** —— 4 条**自动**路径**都在面板内部**
    （`WeaselServer/QuickPanelDialog.cpp:682` / `:740` / `:1317` / `:1326`），逐条带
    `AlwaysShowMode()` 守卫；5 条是**需要用户明确手势**的路径（ESC `:1334`、
    `ToggleMode` 内 `:1453`、`WeaselServer/WeaselServerApp.cpp:387` / `:401` / `:421`）。
    漏掉的是**第 4 条自动路径**：由输入法前端上报「输入焦点离开」
    （`WeaselIPC/WeaselClientImpl.cpp:178` → `WeaselIPCServer/WeaselServerImpl.cpp:272` / `:277` / `:474`
    → `RimeWithWeasel/RimeWithWeasel.cpp`），在那里**裸调** `QuickPanelDialog::Hide()`
    ⇒ **绕开了面板内部的全部守卫**。
  - **修法**：守卫加在**调用点**（`RimeWithWeasel/RimeWithWeasel.cpp:498-499`），
    **不动** `Hide()` 本体（否则 5 条手势路径在长显态下全部失效，ADR 0016 D1 / D2）。
  - **验证**：静态复核守卫位置与 8 条未改路径；**真机验收待做**。
- **T25 · 64 位前端换装链路四环缺陷**（用户反馈 B）
  - **根因**：换装是**四环相扣**的链 —— ① 版本化安装件的拷贝 → ② 开机补换任务的登记
    → ③ 开机时的拷贝执行 → ④ 补换脚本的**拷贝源**解析。四环各有一个缺陷：拷贝**不覆盖**已存在的同名目标；
    `schtasks /TR` **引号断裂** ⇒ 任务**根本没被创建**（用户机上两个任务均不存在）；
    `xcopy … "dst*"` 对**单个文件**非法。
  - **修法（四环已全部改到位）**：
    ① 版本化件落盘 —— `output/install.nsi:644-646`（32 位）/ `:711-713`（64 位）改
    `SetOverwrite on` + `File /nonfatal /oname=weasel.<ver>.<build>.dll "weasel.dll"`；
    **NSIS 唯一改名语法是 `File /oname=<新名> <源>`**（`File "src" "dst"` 会被 `makensis`
    当成两个源文件并报 `no files found` 中止 —— 本轮打包实测钉死）；写前 `Delete`
    陈旧版本化件；`/nonfatal` 把写失败转成错误位。
    ② 探测语义 —— **不再用错误位判断「文件是否被占用」**：`SetOverwrite try` 命中被占用目标时
    **静默跳过且不置错误位**（本轮实测）⇒ 原代码直接跳走、版本化分支根本没执行到；
    改用 `Rename` 成败判断。
    ③ 补换任务 —— `:1077-1085` / `:1103-1110` 脚本落 `$WINDIR\Temp`（**无空格**）
    ⇒ `schtasks /TR` **不带内层引号**；拷贝改 `copy /Y … >NUL`（`xcopy … "dst*"` 对单文件非法）。
    ④ 拷贝源 —— 批处理里的 `"$TEMP\fluxing-staged\…"` 里的 `$TEMP` 是 **NSIS 变量**，
    在 `FileWrite` 时**于安装期展开成安装用户的绝对路径**（不是 cmd 的 `%TEMP%`）
    ⇒ 不再受开机时（SYSTEM）环境里的 `%TEMP%` 影响（ADR 0016 D6 / Q-087-1；
    spec 087 tasks T008 已 `[x]`）。
  - **验证**：整解 `weasel.sln` Release/Win32 与 `WeaselTSF.vcxproj` x64 均 exit 0；
    `makensis /DFLUXING_VERSION=1.1.0 /DWEASEL_BUILD=0 output\install.nsi` exit 0；
    **真机换装验收待做**（需重新装机 + 重启，比对 `weaselx64.dll` 与
    `weaselx64.<ver>.<build>.dll` 的 SHA256 一致、且 64 位 CLSID 指向版本化件）。
- **T26 · 裸 Shift 左右颠倒：根因已闭合，修复已落地**（用户反馈 C，ADR 0017）
  - **已排除**：通道极性、接收侧解码（**运行时探针证明正确**）、发送端判定逻辑、
    消息路由、旧进程抢占、操作系统级扫描码重映射。
  - **根因闭合（spec 087 A.7）**：位域布局 / 键名映射 / 前端判定 / 线缆编解码 / 服务端状态机
    **每一环都逐环证明是对的** ⇒ 反转只能来自**宿主交给 TSF 的 (虚拟键, 扫描码)** 这一对输入本身
    （应用层交换，或更下面的驱动层交换）。
  - **形态从「二选一」改为「两形态合并」**（ADR 0017）：① 前端**扫描码优先**（0x2A=左 / 0x36=右），
    虚拟键只作兜底；② 服务端加**可配置的「左右互换开关」** `weasel.yaml` 的
    `shift_left_right_swap`（默认 `false`），用于扫描码也被交换、软件无从分辨的情况。
    **缺默行为完全不变**；**不扩 IPC 协议**（线缆不变 ⇒ 不触发四边同步）。
  - **回归**：`TestShiftIPCStateMachine` 新增 `Test_ShiftSideSwap`（4 条断钉住四个真值）、
    `TestHotkeyBindings` 新增 6 条 Shift 侧映射断言。
  - **仍未闭合**：**操作系统把物理左 / 右 Shift 报成什么** —— 只能靠一次真实按键取到
    （`tmp/shiftprobe/shiftside.exe` 已就位，一次按键出三列）。它现在只影响
    「要不要建议用户打开开关」，**不再阻塞代码落地**。
- **T27 · 上一轮复测单口径错误（CapsLock）**（用户反馈 D）
  - **定性**：英文模式下按 CapsLock 进入**大写英文输入**、不切回中文，是输入引擎的
    **既定正确行为**（`librime/src/rime/ascii_composer.cc:126-166` + `engine.cc:99-122`，
    `good_old_caps_lock: true` ⇒ `kRejected` 中断处理器链）⇒ **不是缺陷**。
  - **真正的缺陷在复测单**：它让用户在 CapsLock **打开态**测选候选 / 翻页 / 切中英 /
    切标点 / 切简繁，而那些状态下引擎**整条链不响应** ⇒ 测出来的「失效」是**测法**问题。
  - **处置**：**不改码**；语义写进用户可见文档 + 复测单口径改为 **CapsLock 关闭态**
    执行（ADR 0016 D8）。

### 4.2 规划中，未开工

| 编号 | 内容 | 优先级 | 出处 | 备注 |
|---|---|---|---|---|
| T6 | 跨设备云同步 | P2 | spec 010（2/10） | PRD R-005：同步范围**不含**输入历史 |
| T7 | Emoji 面板（Alt+E） | P3 | PRD F12 | 推迟到 v2.1+ |
| T8 | 候选字第二行：剪贴板 + 输入历史 | P3 | PRD F7 | 推迟到 v2.1+ |
| T9 | 中英混合输入 / ASCII 模式 auto | P3 | PRD F8 | 推迟到 v2.1+ |
| T10 | 托盘图标合成叠点 | P3 | PRD F9 | 推迟到 v2.1+ |
| T11 | 方案导入导出（rime.zip） | P2 | PRD F10 / spec 010 | 设计中 |
| T12 | 首启引导 + 现代化安装器 | P3 | PRD F13 / spec 011 | v2.2+ |
| T13 | 用户词典**逐条**编辑（现在只有导入导出） | P2 | spec 044 | 需 export→编辑→import 全量重写，或改 librime（**违反 P3**） |
| T14 | ARM64 构建目标 | P2 | T2.13 | librime Win32-only 约束需评估 |

### 4.3 基础设施缺口（非代码问题）

| 编号 | 内容 | 状态 | 说明 |
|---|---|---|---|
| T15 | `sandbox-gate` CI job | ⚠️ 排队 | 缺带 `sandbox` 标签、支持嵌套虚拟化的 Win11 自托管 runner |
| T16 | CI installer artifact 下载校验 | ⚠️ 受限 | 网速约 50 KB/s，43 MB 产物下载超时；已用本机包 + CI 日志门禁数字替代 |
| T17 | 共享 `data/` 目录其余文件升级 | 🟡 待用户操作 | 除 `default.yaml` 外仍是 2026-08 旧版；**装 1.0.0 包即可刷新**（`fluxing-1.0.0-installer.exe` 的 `data/` 载荷已含重编后的 `build/`） |
| T18 | **进 `release/` 发布链** | ✅ 已完成 | `release/` 已收录 `fluxing-1.0.0-installer.exe`（42,756,263 B，SHA256 `F040D989872FB8D135917B9416E40DE4D3010002EB669EF43AA98544A8344A8A`）与 `fluxing-0.21.0.9-installer.exe`（42,761,559 B，SHA256 `D1FCF78304FB89955F61F8925C894AB1F2CA5941FF9E9283D859D93A3E820E1C`）。注：本 fork 的 CI 发布步骤被 `github.repository == 'rime/weasel'` 门控 ⇒ `release/` 只是 git 跟踪的载荷仓，**不会**自动建 GitHub Release |
| T19 | **沙箱权威门禁在本机跑不起来** | ⚠️ 环境故障 | `run-verify.cmd` 两轮均报「3 次启动都没拿到 claims.json」（orchestrator exit 4）；`sandbox-artifacts\rime\logs\` 下**连 `boot.log` 都没有** ⇒ Windows Sandbox 虚机根本没启动（Hyper-V vSwitch 异常，同机 2026-10-25 曾成功跑出 `PASS=38 FAIL=2`）。根因未定位；**不阻塞提交**（本轮未改 `output/install.nsi` 与 `output/data/user-custom/**`），但 0.21.0.9 的沙箱断言仍为空 ⇒ 见 L150 |
| T20 | **CapsLock 打开时快捷键全部失效** | ✅ 已修复，待真机复验 | 根因**不是**「按键匹配失配」，而是**输入引擎在 CapsLock 打开后不再处理任何按键**：前端 `WeaselTSF/KeyEvent.cpp` 在 CapsLock 打开时给按键事件多置 `LOCK_MASK`(0x2) ⇒ `expand_ibus_modifier`（`RimeWithWeasel.cpp:44-46`）保留该位 ⇒ librime 精确比较 modifier（`librime/src/rime/key_event.h:43-44`）失配；**更关键**的是本项目配置 `good_old_caps_lock: true`（`output/data/default.yaml:72-73`）让英文输入组件对「带着锁存标记」的键返回「拒绝处理」⇒ **整条处理器链中断**（`librime/src/rime/engine.cc:105-107`）⇒ 候选选择 / 翻页 / 切中英 / 切标点 / 切简繁**全部**失效。修法取唯一源头：**前端不再上报锁存状态**（ADR 0014 D1）；门禁新增 caps 维度 + 内建负控（ADR 0014 D4）。详见 spec 085 §5.3.1 与 4.1.6。门禁证据：源门禁 `caps_reachable=39 / caps_dead=0 / caps_limited=0`，负控 `caps_legacy_reachable=0 / caps_legacy_dead=39`（模拟修复前 39/39 全死）⇒ 门禁对缺陷敏感 |
| T21 | **裸 Shift 上屏候选左右颠倒** | ✅ 已修复，待真机复验 | 线缆约定（`wParam bit0 = is_left`，0=L/1=R）在客户端与服务端**各写一份字面量**，客户端写对、**服务端读反** ⇒ 左 Shift 走右 Shift 分支（选第 3 个）。修法：编解码收进共享单元 `include/ShiftStateMachine.h` 的 `EncodeShiftIsLeft` / `DecodeShiftIsLeft`，两端各调一个（ADR 0015 D2）；回归测试**直接钉位值**（左=0、右=1、`Decode(0)==true`、`Decode(1)==false`）⇒ 连「两端一起错」也能证伪（D3）。详见 spec 086 §5.3 与 4.1.7 |
| T22 | **输入法弹出菜单缺「快速面板常驻」** | ✅ 已修复，待真机复验 | **菜单有两份资源**：服务端托盘图标菜单（`WeaselServer/WeaselServer.rc`，spec 084 已加该项）与输入法前端弹出菜单（`WeaselTSF/WeaselTSF.rc`，简 / 繁 / 英三块，**没有该项**）；命令编号也只在服务端头文件里，共享头 `include/resource.h` 没有 ⇒ 前端连写都写不出来。修法：共享头补 `ID_QUICKPANEL_ALWAYS_SHOW 40019` + 前端三个弹出菜单各补一项，**不扩协议**（复用既有托盘命令通路，ADR 0015 D4–D6）。验证用**构建产物的 PE 资源级检查**，不只读源码。详见 spec 086 §5.3 与 4.1.7 |
| T23 | **装机时 64 位前端 DLL 被占用未替换** | ⚠️ 待用户重启 | 用户反馈 1 / 2（CapsLock 打开后候选窗消失、快捷键全失效）的真因**不是代码缺陷**：装机时 `weaselx64.dll` 被 TSF 占用，装的是 versioned shim + `weaselx64.1.1.0.0.dll` + `weaselx64.dll.stage2` 标记 ⇒ **开机补换**机制正常，机器尚未重启 ⇒ 64 位进程仍加载前一天旧件（旧件里还带着锁存标记上报 ⇒ 引擎整链中断）。⇒ **重启 Windows 即完成换装**，重启后复测反馈 1 / 2；若仍失败再另立条目 |
| T24 | **勾了「快速面板常驻」仍会随输入焦点切换消失** | ✅ 已修复，待真机复验 | 面板共有 **9 个收起发起点**：4 条自动路径**都在面板内部**且逐条带 `AlwaysShowMode()` 守卫（`WeaselServer/QuickPanelDialog.cpp:682` / `:740` / `:1317` / `:1326`），5 条是**需用户手势**的路径（ESC `:1334`、`ToggleMode` 内 `:1453`、`WeaselServer/WeaselServerApp.cpp:387` / `:401` / `:421`）。漏掉的是**第 4 条自动路径** —— 由输入法前端上报「输入焦点离开」，一路送到 `RimeWithWeasel/RimeWithWeasel.cpp` 后**裸调** `Hide()` ⇒ 绕开面板内部全部守卫。修法：守卫加在**调用点**（`RimeWithWeasel/RimeWithWeasel.cpp:498-499`），**不动** `Hide()` 本体（否则 5 条手势路径在长显态下全失效，ADR 0016 D1 / D2）。详见 spec 087 §5.3 与 4.1.8 |
| T25 | **64 位前端换装链路四环缺陷** | ✅ 四环已全部改到位，真机换装验收待做 | 换装是**四环链**：① 版本化件拷贝（**不覆盖**已存在目标 ⇒ 永远停在上一版）② 开机补换任务登记（`schtasks /TR` **引号断裂** ⇒ 任务**根本没被创建**，用户机上两个任务都不存在）③ 补换动作内的拷贝（`xcopy … "dst*"` 对**单文件**非法）④ 补换脚本的**拷贝源**解析。修法：`output/install.nsi:644-646` / `:711-713` 改 `SetOverwrite on` + `File /nonfatal /oname=…`（NSIS **唯一**改名语法，`File "src" "dst"` 实测会被 `makensis` 当成两个源文件并中止）；写前 `Delete` 陈旧版本化件；**不再用错误位探测占用**（`SetOverwrite try` 静默跳过且不置错误位 —— 实测），改用 `Rename` 成败判断；`:1077-1085` / `:1103-1110` 脚本落 `$WINDIR\Temp`（**无空格**）⇒ `schtasks /TR` **不带内层引号**；拷贝改 `copy /Y … >NUL`；**拷贝源用 NSIS 的 `$TEMP` 在安装期展开成绝对路径** ⇒ 不再受开机时（SYSTEM）`%TEMP%` 影响。**四环均已改到位**（ADR 0016 D3–D6 / Q-087-1；spec 087 tasks T008 已 `[x]`），**真机换装验收待做** |
| T26 | **裸 Shift 左右颠倒** | ✅ 修复已落地，待真机复验 | 根因**已闭合**（spec 087 A.7）：链路每一环（位域布局 / 键名映射 / 前端判定 / 线缆编解码 / 服务端状态机）都逐环证明是对的 ⇒ 反转来自**宿主交给 TSF 的 (虚拟键, 扫描码)** 这一对输入本身。修法（ADR 0017）：前端**扫描码优先**（0x2A=左 / 0x36=右），虚拟键兜底；服务端加**可配置「左右互换开关」** `shift_left_right_swap`（默认 `false`）。**缺默行为完全不变**，**不扩 IPC 协议**。回归：`Test_ShiftSideSwap` 4 条真值断言 + 6 条 Shift 侧映射断言。仍待：一次真实按键定「要不要建议打开开关」。详见 spec 087 §5.3 与 4.1.8 |
| T27 | **上一轮复测单口径错误（CapsLock）** | ✅ 已修正（文档口径） | 英文模式按 CapsLock 进入**大写英文输入**、不切回中文，是输入引擎的**既定正确行为**（`librime/src/rime/ascii_composer.cc:126-166` + `engine.cc:99-122`，`good_old_caps_lock: true` ⇒ `kRejected`）⇒ **不是缺陷**。真正的缺陷在**复测单**：让用户在 CapsLock **打开态**测选候选 / 翻页 / 切中英 / 切标点 / 切简繁，而那些状态下引擎**整条链不响应** ⇒ 测出来的「失效」是**测法**问题。处置：**不改码**，语义写进用户可见文档 + 口径改为 **CapsLock 关闭态**执行（ADR 0016 D8） |
| T28 | **Shift 方向的兜底开关与规则** | ✅ 已落地，待真机复验 | 兜底开关 `shift_left_right_swap` 放在**服务端**（`weasel.yaml` 的 `[global settings]`，与 `bare_shift_toggles_ascii` / `global_ascii` 同一机制），前端保持无状态 ⇒ 换开关**不必重装前端**。判定走**纯函数** `fluxing::ApplyShiftSideSwap` ⇒ 可单测。用户的用户目录 `%APPDATA%\Rime\weasel.yaml` 不带该键时即缺默 `false`。开关位置与意义见 ADR 0017 |


---

## 5. 明确不做 / 已废弃（不要重新捡起来）

| 内容 | 为什么不做 | 出处 |
|---|---|---|
| 改 `librime/` | 宪法 P3：子模块只读；lua 插件改动走 `thirdparty/librime-lua/` | P3 / L10 |
| `ascii_composer/switch_key` 里配 Shift | ADR 0010 证明**不可能**：TSF 把 Shift 整段吃掉，librime 永远看不到 | ADR 0010 |
| yaml 形式的 `Shift+Shift_L` 选候选绑定 | 已被 spec 077 的 IPC 状态机取代；L18/L19/L21 的 release 误匹配链 | L19 / L21 / ADR 0007 |
| 装 x64 的 `Weasel*.exe` | librime Win32-only，混架构会以 `0xC000007B` 崩 | L14 |
| 前端把 Shift 转发给 librime | 正是 ADR 0007 移除的东西；会重开 L18 的 release 误匹配 | ADR 0010 |
| 改 `.clang-format` / Boost / OpenCC 内容 | 宪法 P8 范围外 | P8 |

---

## 6. 已知遗留（技术债）

| 内容 | 影响 | 处置 |
|---|---|---|
| 共享 `data/` 目录除 `default.yaml` 外仍是 8 月旧版 | 源与产物漂移；装包后才一致 | 装 1.0.0 安装包 |
| `test/v0_19_0_30_e2e/build_e2e.bat` 写死 4 处机器路径 | 换机即废 | 已知不改 |
| `build (xmake)` arm64 目标缺失 | ARM 设备无 CI 覆盖 | 见 T14 |
| 真实按键行为无法由门禁证明 | 门禁只证「能触发」，证不了「产生预期效果」 | 需人手敲一次 `Shift+'` |
| MiniMax Code 运行时反复重启（1 h 内 6 次） | 持续杀掉在飞后台任务 | 已提报（仓外），产品侧排查中 |
| 沙箱权威门禁在本机跑不起来（T19） | 0.21.0.9 的 46 项沙箱断言一条没跑 | 需修 Hyper-V vSwitch；或换一台机器跑（见 L150） |
| ~~CapsLock 打开时快捷键全部失效（T20）~~ | 常开 CapsLock 的用户会以为快捷键坏了 | ✅ 已修（spec 085 / ADR 0014）：前端不再上报锁存状态 + 门禁纳入 caps 维度。**残留限制**：字母类快捷键在 CapsLock 打开时仍要写成大写变体（ADR 0014 D5） |
| ~~输入法弹出菜单没有「快速面板常驻」的勾选标记（T22）~~ | 常显开关的当前状态只在托盘图标菜单里可见 | ⚠️ 已知限制：补勾选需新增「查询开关状态」的往返消息 ⇒ **协议扩张**，本条不做（ADR 0015 D7）。留后续条目 |
| ~~装机后 64 位前端仍是旧件（T23）~~ | 重启前反馈 1 / 2 会**继续复现**（候选窗消失 / 快捷键全失效） | ⚠️ 部署侧：**重启 Windows** 即完成换装；包内新件已含 ADR 0014 修复 |

---

## 7. 待补充开发内容

> **这一节是给你填的。** 每加一行，请同时补「验收方式」——否则无法判定「完成」。
> 建议：直接复制下面三行模板往下加。

| 编号 | 想开发的内容 | 所属域 | 优先级 | 状态 | 验收方式 |
|---|---|---|---|---|---|
| N1 |  |  |  | ⬜ |  |
| N2 |  |  |  | ⬜ |  |
| N3 |  |  |  | ⬜ |  |

---

## 8. 验收门禁与证据索引

| 门禁 | 命令 | 通过标准 |
|---|---|---|
| 全套件 | `cmd /c scripts\test-infra\run-test-suite.bat` | `=== ALL TESTS PASSED ===`（16 工程） |
| 快捷键门禁（仓库源） | `TestHotkeyBindings.exe --baseline test\TestHotkeyBindings\known-dead-bindings.txt output\data\default.yaml output\data\user-custom\*.custom.yaml` | `SUMMARY\|bindings=39\|reachable=39\|dead=0` + `RESULT\|PASS` |
| 快捷键门禁（**部署产物**） | 同上，路径换成 `<user_data_dir>\build\*.schema.yaml` | `SUMMARY\|bindings=120\|reachable=120\|dead=0` + `RESULT\|PASS` |
| 默认快捷键断言 | `test\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe` | `Passed: 89 / 89` |
| Shift 状态机断言 | `test\TestShiftIPCStateMachine\Release\TestShiftIPCStateMachine.exe` | `Result: 28 PASS / 0 FAIL`（17 个测试函数 / 28 条断言：11 个函数属 spec 077，6 个属 ADR 0010） |
| 布局探测 | 门禁自带 `LAYOUT-PROBE\|Shift+quote=0x22 OK` | 键名解析与当前键盘布局一致 |
| CI | `gh run list --repo kizemo/Fluxing --workflow ci.yml` | run `40632079796`（head `ceff6b6`）：`lint` = `success`；`test` / `build (msbuild)` / `build (xmake)` 落笔时仍 `in_progress`（未取终态，不预判） |
| 沙箱 | `F:\soft\00selfmade\sandbox-verify\rime-claude\` | ~46 项断言 ⚠️ **0.21.0.9 本机未跑通**（T19：虚机不启动，无 `boot.log`） |

> **门禁要指向部署产物跑一遍。** 只扫仓库树会漏：源绿了也可能产物是坏的
> （本轮实测：扫仓库 `bindings=39`，扫真机产物 `bindings=120`，中间隔着一次部署）。

---

## 9. 附录 A：版本线时间轴（fork 自 0.18.0 起）

| 版本 | 日期 | 主线内容 |
|---|---|---|
| 0.18.3.0 – 0.18.7.0 | 2026-06-30 – 07-01 | 品牌分叉起步、默认快捷键 rev2、L19 修复、CI test job（R-008 关闭） |
| 0.18.8.0 – 0.18.19.0 | 2026-07-01 – 07-04 | 安装器链补齐（L40）、PRD/TDD 快照 |
| 0.18.20.0 – 0.18.26.0 | 2026-07-04 – 07-05 | 暗色桥接（spec 033/034）、FluxingComponents v0、L46 双路径对齐 |
| 0.18.27.0 – 0.18.34.0 | 2026-07-05 – 07-09 | 组件库与 DPI 验证、QuickPanel v0、托盘面板 v0 |
| 0.19.0.10 – 0.19.0.32 | 2026-07-12 – 07-15 | QuickPanel v4 / v3-rev3 / 长显模式迭代（spec 049/070/052） |
| 0.19.0.33 – 0.19.0.62 | 2026-07-17 – 07-24 | 候选字编辑五阶段、常用短语独立进程、yaml 快捷键编辑器 MVP |
| 0.20.0.2 | 2026-08-07 | — |
| 0.21.0.1 | 2026-08-16 | **spec 077**：Shift 释放选候选改走 IPC 状态机（ADR 0007） |
| 0.21.0.2 | 2026-10-23 | 全量重编（x64 TSF 首次含 Shift 状态机）、Sparkle 改源、download 404 |
| 0.21.0.3 | 2026-10-23 | 升级不再需要重启 |
| 0.21.0.5 | 2026-10-23 | spec 080：切到 Fluxing 自动弹面板；面板自动隐藏回归修复 |
| 0.21.0.6 | 2026-10-24 | L113 死绑定移除；发布构建统一走 MSBuild（L127/L128） |
| 0.21.0.7 | 2026-10-25 | ADR 0009：删 2 条永不触发的重复快捷键 |
| 0.21.0.8 | 2026-10-26 | ADR 0009 收尾：4 条 `Shift+标点` 改名修好；门禁基线清零；ADR 0010 成文 |
| 0.21.0.9 | 2026-10-27 | ADR 0010：裸 Shift 切中英文（上下文消歧，默认开，配置开关可关）；`TestShiftIPCStateMachine` 扩到 28 PASS；**已推送 `e0d6908`**；沙箱门禁因本机环境未跑通（T19），真机按键验收待做 |
| 1.0.0 | 2026-10-28 | ADR 0012：裸 Shift 上屏候选 / 跨会话隔离 / 语言栏中英图标；Shift IPC 回响应体（协议事实：服务端不回 body ⇒ 客户端解析陈旧响应）；版本号 4 段改 3 段；`TestShiftIPCStateMachine` 34 PASS；**已推送 `ceff6b6`**；沙箱门禁仍因本机环境未跑通（T19），真机按键验收待做 |

---

## 10. 附录 B：spec 索引（70 份）

> **注意**：tasks.md 的勾选框**不可信**。多份已 ship 的 spec（002 / 006 / 037 / 050 / 070 等）
> 仍显示 `0/N`。**权威记录是 CHANGELOG**，本清单 §2–§4 以 CHANGELOG + 门禁数字为准。

| 状态 | 份数 | 代表 spec |
|---|---|---|
| 全部完成（DONE） | 5 | 008（候选字编辑）、077（Shift IPC 状态机）、079（编码修复）、080（自动弹面板）、082（裸 Shift 切中英） |
| 部分完成（PART） | 13 | 001、003（构建）、004、005、010、011、036、049、051、075、078、081、087（面板「常驻」守卫 / 64 位换装 / Shift 取证） |
| 显示为未开工（OPEN） | 49 | 多数已 ship 但 tasks 未维护；真正未开工的是 010/011/050/052/070 等 |

**真正的「设计已完成、代码未开始」**：spec 010（云同步）、spec 011（首启引导）、
spec 050（yaml 编辑器剩余页）、spec 052（长显）、spec 070（v3-rev3）、
spec 083（抓键 layout-aware 改造，ADR 0011 已定稿；M1 / M2 / M3 已落地，M4 / M6 待做）。

---

## 11. 相关文档

| 文档 | 作用 |
|---|---|
| [`.specify/PRD.md`](.specify/PRD.md) | v2 产品需求（13 项 F1–F13 + SC-001~SC-006） |
| [`CHANGELOG.md`](CHANGELOG.md) | 83 个版本节的权威发布记录 |
| [`CONTEXT.md`](CONTEXT.md) | 领域词汇 + 17 条不变量 |
| [`AGENTS.md`](AGENTS.md) | 操作手册（构建/测试/危险区/反模式） |
| [`.specify/memory/lessons-learned.md`](.specify/memory/lessons-learned.md) | L01–L157 事故库 |
| [`.specify/memory/project-knowledge.md`](.specify/memory/project-knowledge.md) | 部署路径/注册表/三层覆盖架构 |
| [`docs/adr/`](docs/adr/) | 11 份架构决策记录（0011 / 0016 为 `Proposed`） |
| [`Rime-设置说明书-重点快捷键.md`](Rime-设置说明书-重点快捷键.md) | 用户可见快捷键手册（md + html） |

---

*最后更新：2026-10-29。数据来源：`git diff --numstat master...Fluxing`（HEAD `c55abfa` + 本轮 docs 提交）、
`CHANGELOG.md` 版本节统计、`.specify/specs/` 70 个 spec 目录（62 份带 tasks.md）的勾选统计、
本轮 spec 087 / ADR 0016 的落地与未闭合项记录。*
