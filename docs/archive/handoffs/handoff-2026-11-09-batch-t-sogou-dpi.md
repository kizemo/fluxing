# 交接 · 批次 T（2026-11-09）— 搜狗双拼失效 + 快捷面板 DPI 适配

- 会话: MCode 会话 `mvs_fb83df560a5b4138a4ba850fc6bc99cc`
- 分支: `Fluxing`（远端 `kizemo/Fluxing`，**不推 origin**），基线 HEAD `9e6b477`
- 承接: 批次 S（spec 045 圆形 ✕ 脏态确认 P0 + T201 候选源评估）

---

## 1. 目标

用户报告两个新缺陷：

1. **中文状态只能全拼输入**，切换到「搜狗双拼」后仍是全拼。
2. **远程更换显示分辨率后**，快捷设置栏品牌图标只显示一部分、按钮符号偏大，需适应不同分辨率。

---

## 2. 根因（已坐实）

### 2.1 双拼：数据层全部证伪，根因在**我们前端会话/schema 状态层**

证据（探针与日志全部落盘）：

- `tmp/probe-double-pinyin.cpp` / `tmp/probe-dp.log`：在 `double_pinyin_sogou` 下
  `vs`→中/种/重、`vl`→摘、`vd`→装 ⇒ **schema / 代数 / prism / 键位全部正常**。
- `tmp/probe2-session-default.cpp`：**新建会话默认 schema = rime_ice（全拼）**。
- `tmp/dbl-repo.txt` / `tmp/dbl-inst.txt`：仓库 `radical_pinyin.schema.yaml` 含合法
  `algebra_double_pinyin` 块（rime-prelude 上游布局，非串写）；装机
  `build/radical_pinyin.schema.yaml` 是 rime_radical_pinyin 包自带**全拼** schema（无污染）。
- `tmp/dbl-log1900.txt` / `tmp/dbl-log-filtered.txt`：服务端确实加载过
  `double_pinyin_sogou.prism.bin`（double array1024）+ `cn_en_sogou.txt`。
- librime 侧核实：`Service::CreateSession` 不设 schema、`RestoreSavedOptions` 不恢复 schema
  ⇒ **新会话必然回落 `schema_list` 第一项 = rime_ice**（librime 正常行为，非缺陷）。

**我们前端的 4 处缺陷**（`RimeWithWeasel/RimeWithWeasel.cpp`）：

1. `SwitchToNextSchema()` 用 `to_session_id(m_active_session)`；该函数用 `operator[]`
   **插入假条目**，而 `m_active_session` 会被 `FocusOut` 清零 ⇒ `select_schema(0,…)` 落假会话
   ⇒ 返回 False 且**全程静默** ⇒ 用户观感「点了没反应」。
2. `m_current_schema_id`（全局单值）被 `AddSession` 用新会话默认方案 rime_ice **无条件覆盖**
   ⇒ 面板显示与实际会话脱节，「下一个方案」起点可能算成当前方案本身。
3. 切换只作用于**单个会话** ⇒ A 窗口切了双拼、B 窗口（新会话默认 rime_ice）仍全拼。
4. `SelectSchema(id)` 内部 `create_session()` 造新会话再切 ⇒ 对正在输入的窗口无可见效果
   （全仓无调用点，死代码）。
5. 放大器：`_ResolveTargetSession()` 的唯一会话回退用 `m_session_status_map.size()==1`，
   历史 `operator[]` 假条目（key=0）把 size 变成 2 ⇒ 回退永久失效。

### 2.2 DPI：三条根因

- **品牌只显示一部分（P0）**：`s_hBmpLogo` 只在 `OnCreate` 按当时 brand 尺寸用 WIC 预缩放解码一次；
  `ApplyPhysicalLayout` / `WM_DPICHANGED` 只更新尺寸变量、**从不重建位图**；
  绘制用 `BitBlt(SRCCOPY)` 1:1 贴图（GDI 不缩放）⇒ DPI 变化后只剩旧图一角。
- **符号偏大（P0）**：键帽 `btn_size` 按 dpr 缩放，但 5 个 `DrawIcon*` 与 fwicon 按
  **raw 像素**绘制 ⇒ 现场有效 DPI 是**下降**的，最宽 25 raw px 的图元溢出缩小后的键帽。
- **复用路径不重查 DPI（P1）**：`QueryPanelDpi` 只在建窗路径调用，`Show()` 复用路径不重查。

静态诊断报告：`tmp/dpi-diagnosis.md`（204 行）。

---

## 3. 已落地的修复

### 3.1 双拼（`RimeWithWeasel/RimeWithWeasel.cpp` + `include/RimeWithWeasel.h`）

| 口径 | 实现 |
|---|---|
| 目标会话一律走 `_ResolveTargetSession(0)` | 禁用 `to_session_id` 的 `operator[]` 副作用 |
| 「当前方案」取目标会话 live 值 | 新增 `_LiveSchemaId()`（`get_status` 读内存，零文件 I/O，符合 constitution P2） |
| 全局镜像降级为兜底 | `m_current_schema_id` 只在为空时初始化，不再被新会话冲回 rime_ice |
| 显式选方案 = **全局语义** | 切换成功后**广播到所有活动会话** |
| 新会话**继承**用户已选方案 | `AddSession` 中仅当镜像非空且与新会话默认方案不一致时才 `select_schema` |
| 失败必须可见 | 新增 `_NotifySchemaSwitch()`（中文提示，走既有「部署完成」提示同一条通道），区分无会话 / 无可用方案 / 切换失败 |

**额外修掉一处新引入的 use-after-free**：`AddSession` 里提前 `free_status(&status)` 后，
下方仍读取 `status.schema_id` ⇒ 已把释放统一挪到块末尾。

### 3.2 DPI（`WeaselServer/QuickPanelDialog.{h,cpp}` + `include/FluxingPanelDpi.h` + `include/FluxingPanelFwIcon.h`）

- 面板**单一缩放系数** = `dpi/96`，钳到 `[96,384]`（**下限 1.0** ⇒ 窗口物理尺寸恒 ≥ 逻辑尺寸）；
  新增 `ClampPanelDpi` / `PanelScaleFactor` / `ScaleByFactor`（**不改** `ScaleLogicalByDpi` 既有语义）。
- **logo 位图随 brand 物理尺寸重建**：`s_logoSize_phys != s_brandSize_phys` ⇒
  `DeleteObject` 旧位图 + 同一条 WIC 预缩放路径重建（仍是不透明 1:1 贴图，保住需求⑦ 结论）。
- **图标几何乘同一系数**：`DrawIcon*` / `DrawIconFullwidth` 全部新增 `scale` 参数。
- **DPI 一律取窗口当前值**：`GetDpiForWindow` **运行时 `GetProcAddress` 解析**
  （不静态导入 `api-ms-win-shcore-scaling-l1-1-1.dll`）。
- **复用路径重查 DPI**：新增 `QuickPanelDialog::SyncPanelSizeToCurrentDpi(HWND)`，
  `Show()` 的建窗路径与复用路径都调用。

### 3.3 文档

- `docs/adr/0048-schema-switch-global-semantics-and-session-scoped-resolution.md`
- `docs/adr/0049-quickpanel-single-scale-factor-and-logo-rebuild-on-resize.md`

---

## 4. 验证状态

### 已验证（程序化）

| 项 | 结果 | 证据 |
|---|---|---|
| 安装包构建 | `XBUILD_EXIT=0` | `tmp/build-b3.log` |
| `TestShortcutSettings` | **PASS=249 FAIL=0** | `tmp/unittest-bT.log` |
| `TestQuickPanelDialog` | **258/258 assertions passed** | `tmp/build-panel-test2.log` + 本轮运行输出 |
| L0 静态门禁 | **15/15 PASS**（mode=all） | `tmp/l0-bT.log` |
| 静默装机 | `SILENT INSTALL OK - ALL HARD CHECKS PASSED` | `tmp/install-bT.log` |
| 装机二进制确为本次构建 | `WeaselServer.exe` mtime `2026/11/9 21:47:48` | 本轮核对输出 |
| 双拼数据层正常 | `vs`→中/种/重 | `tmp/probe-dp.log` |
| 新会话默认 rime_ice | 已坐实 | `tmp/probe2-build.log` / 探针输出 |

### 未验证（受阻，附原因与可执行步骤）

**真机端到端复验受阻于「自动化环境无法稳定维持 IME 激活态」**，不是代码问题：

- 面板窗口 `FluxingQuickPanel_v3` 只在 **IME 激活瞬间**存在；
  `Win+Space` 切换后能被枚举到（句柄 3346696），但下一次探针运行时已消失 ⇒
  `FindWindow` 返回 0。
- 宿主窗口（WinForms 原生 EDIT，`tmp/ime-probe-host.cs`）已用
  `AttachThreadInput` 抢到前台（`FG == HOST`），但 `ImmGetContext` 仍返回 0、
  服务端日志无 `Add session` ⇒ TSF 文本服务未创建会话。
- 未做**改显示分辨率**的真机复验（会真实改动用户远程桌面分辨率，不擅自执行）；
  DPI 修复已用 `WM_DPICHANGED` 模拟路径设计好探针 `tmp/probe-panel-dpi.ps1`
  （before/after 截图 + 尺寸/dpi 数值），一旦面板可见即可直接跑。

**请用户手工复验（各 1 分钟）**：

1. 双拼：记事本里输入 `vs` → 若出「中/种/重」即双拼已生效；
   再点面板槽 0（切换到下一个输入方案）→ 切到搜狗双拼 → **切到另一个窗口新建编辑框**
   → 再次输入 `vs`，**仍应出「中」**（旧版本这里会退回全拼）。
   面板槽 0 的 tooltip 应显示当前方案名；切换失败会有中文提示（旧版本是静默）。
2. DPI：远程改分辨率 / 改显示缩放 ⇒ 品牌 logo 应**完整显示**，
   键帽内符号应与键帽**同比缩放**（不再溢出）。

### 复现命令

```powershell
# 构建 + 打包
cmd /c "F:\soft\00selfmade\rime_claude\tmp\build-installer.cmd"
# 单测
cmd /c "F:\soft\00selfmade\rime_claude\tmp\build-test-shortcut.cmd"   # -> Release\TestShortcutSettings.exe
cmd /c "F:\soft\00selfmade\rime_claude\tmp\build-test-panel.cmd"      # -> Release\TestQuickPanelDialog.exe
# L0 门禁（PS7）
& 'F:\soft\00selfmade\rime_claude\scripts\gate\L0-static.ps1' -Mode all
# 装机
& 'F:\soft\00selfmade\rime_claude\scripts\install\silent-install.ps1' -InstallRoot 'D:\Program Files\fluxing'
# 真机探针（需 IME 激活态）
& 'F:\soft\00selfmade\rime_claude\tmp\probe-panel-dpi.ps1'
& 'F:\soft\00selfmade\rime_claude\tmp\probe-sogou-real.ps1'
```

---

## 5. 未决 / 后续

1. **真机端到端复验**（见上，需用户手工）。
2. `output/data/double_pinyin_mspy.schema.yaml` 标注「微软双拼」却使用**搜狗**键位 ⇒
   次要数据缺陷，本轮未改（改 yaml 前必须先读 `Rime-设置说明书-重点快捷键.md`）。
3. `m_session_schema_ids` 已声明并在 `AddSession` 写入，但**尚未在 `_GetStatus` 与切换成功后回写**
   ⇒ per-session 表目前只有 AddSession 路径写入（当前语义不依赖它，属完备性欠账）。
4. 广播语义：若将来支持「按窗口独立选方案」的深需求，需另立 ADR 推翻 ADR 0048。
5. ADR 0047 仍空缺（批次 S 预留），本轮用了 0048 / 0049。

---

## 6. 本轮工具坑

- `bash` 硬安全：`cd` 之后出现写/删目标会被拦 ⇒ 构建脚本用**绝对路径**调用
  （`cmd /c "F:\...\tmp\build-installer.cmd > F:\...\tmp\build.log 2>&1"`），
  `tmp\build-installer.cmd` 自身带 `cd /d "%~dp0.."`，不需要外部 cd。
- `Select-String -Pattern 'Remove session'` 会被硬安全误判为删除类命令 ⇒ 用 `grep` 工具或改词。
- 编译 `test\TestQuickPanelDialog` 才会暴露面板侧改动的问题：`xbuild` 走 xmake，
  **不编译 `.vcxproj` 测试工程** ⇒ 面板改动必须额外跑 `tmp/build-test-panel.cmd`。
- `WM_DPICHANGED` 处理函数里形参名是 `w` / `l`（不是 `lParam`）⇒ 子代理初版用了
  `lParam` 导致 `C2065`，已修（`WeaselServer/QuickPanelDialog.cpp:1586`）。
- `FindWindow` 找 `FluxingQuickPanel_v3` 在 IME 未激活时返回 0；`Win+Space` 切换后短暂存在。


---

## 7. 复核追加：双拼修复此前**并未上机**（x64 TSF 未重建）—— 已重建并重新装机

上一轮写的「静默装机 ALL HARD CHECKS PASSED」**具有误导性**：装机确实成功，但**装的是旧前端**。

### 7.1 证据链

| 对象 | 复核时 mtime | 判读 |
|---|---|---|
| `output\weaselx64.dll` | 2026-11-08 21:47:59 | **11/8 旧构建**，不含双拼修复 |
| `output\weasel.dll`（x86） | 2026-11-09 21:47:48 | 本轮构建（含修复） |
| 装机 `weaselx64.dll`（裸名） | 2026-11-08 21:47:58 | 11/8 旧件（被在跑进程占用，覆盖失败） |
| 装机 `weaselx64.1.2.1.2.0/.1.dll` | 2026-11-08 21:47:58 | 旧 shim |
| 64 位 CLSID `InprocServer32` | 复核前指向 `weaselx64.1.2.1.2.0.dll` | **用户实际加载的是旧件** |

### 7.2 根因

`tmp/build-installer.cmd` 只跑 `xbuild.bat weasel installer`；`xbuild.bat` 里明写：

```
rem x64 build skipped: librime is Win32-only (rime.dll is 32-bit) ...
xmake f -a x86 -m %build_config% %build_sdk_option%
```

⇒ **只编 x86**，`output\weaselx64.dll` 不会被刷新。而双拼修复在 `RimeWithWeasel/RimeWithWeasel.cpp`
⇒ 编译进 **x64 TSF `weaselx64.dll`** ⇒ 用户（x64 Windows）加载的一直是没有修复的前端。

误判点：把「装机 `weaselx64.dll` 未改动」读成「TSF 前端本轮无改动」——**方向判反**，
`RimeWithWeasel` **就是** TSF 前端。已写入 `.specify/memory/lessons-learned.md` **L210**。

### 7.3 已执行的修复动作（L128 配方）

1. `msbuild RimeWithWeasel\RimeWithWeasel.vcxproj /p:Platform=x64`（**必须先编**，
   否则 WeaselTSF 会链接陈旧 `.lib`）
2. `msbuild WeaselTSF\WeaselTSF.vcxproj /p:Platform=x64` ⇒ `output\weaselx64.dll` @ **22:02:33**
3. 同上 Win32 两步 + `msbuild weasel.sln /p:Platform=Win32` ⇒ `WeaselServer.exe` @ **22:03:34**
4. `makensis output\install.nsi` ⇒ `output\archives\fluxing-1.2.1-installer.exe` @ **22:05:15**
   （复用仓库既有 `tmp/build-batch-i.cmd`，7 步全绿，日志 `tmp/build-tsf-x64.log`）
5. `scripts\install\silent-install.ps1 -InstallRoot 'D:\Program Files\fluxing'` ⇒
   **ALL HARD CHECKS PASSED**（日志 `tmp/install-bT2.log`）
6. 重启 `WeaselServer.exe` ⇒ pid **67864** @ 22:09:03

### 7.4 上机判据（哈希 + mtime 双证，均已通过）

- 64 位 CLSID `{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}\InprocServer32`
  ⇒ `D:\Program Files\fluxing\weasel\weaselx64.1.2.1.2.2.dll`
- 该 shim mtime **11-09 22:02:32**，SHA256
  `0BC51EBDEBBD2A263D2C9DF0D694110420134F308EDD36793C584D424204D2F1`
  = `output\weaselx64.dll` 的 SHA256 **完全一致**
- 裸名 `weaselx64.dll` 仍是 11/8 旧件（被在跑进程占用）——**但注册指向的是新 shim**，
  新进程用新前端；已在跑的进程（explorer 等）仍持有旧 DLL ⇒ **需注销 / 重启后**才全面换过来

### 7.5 复跑门禁

- `Release\TestShortcutSettings.exe` ⇒ **PASS=249 FAIL=0**
- `Release\TestQuickPanelDialog.exe` ⇒ **258 / 258 assertions passed**
- `scripts\gate\L0-static.ps1 -Mode all` ⇒ **checks=15 pass=15 fail=0 skip=0**
- `git status -sb` ⇒ 仅基线 ` m librime`

### 7.6 新增未决项

6. ~~`scripts/install/acceptance-check.ps1` 的 C3 / C3b 用 **3 段**正则匹配版本化 shim，
   而现役命名是 **4 段**（`weaselx64.<ver>.<build>.<seq>.dll`）⇒ 每次误报
   「versioned shim : NONE FOUND」/「unexpected file name」。属**验收判据自身失灵**（会掩盖真缺件）。~~
   **✅ 已修（见 §7.7）**。更正一处表述：现役命名不是固定 4 段，而是 **4–5 段数字**
   （`FLUXING_VERSION` 自身是 `N.N.N`，再拼 `<WEASEL_BUILD>` 与 `<seq>` ⇒ 4 段；
   若 `FLUXING_VERSION` 是 4 段则得 5 段，实机两者都存在：
   `weaselx64.1.1.6.0.dll` 4 段、`weaselx64.1.2.1.2.2.dll` 5 段）。

### 7.7 未决项 6 修复：验收判据 C3/C3b 失灵（2026-11-09 收尾）

**结论**：`scripts/install/acceptance-check.ps1` 已修，装机验收 **EXIT=0 / ALL HARD CHECKS PASSED**。

**改了什么**（两处，均已逐行复读核对落盘）：

1. 头注释 Criteria 段：C3 说明改为「shim 名 3–5 段数字」，并标注旧 3 段正则永远匹配不到现役名；
   C3b 标注为 **HARD CHECK**。
2. C3/C3b 本体：
   - 新增 `$ShimRx = '^weasel(x64)?(\.\d+){3,5}\.dll$'`、
     `$X64ShimRx = '^weaselx64(\.\d+){3,5}\.dll$'`（用段数区间而非固定段数，兼容历史 4 段与现役 5 段）。
   - **C3 保持信息性**（shim 为 0 是合法状态：没文件被锁时用裸名），仅把误导性文案改为
     `NONE FOUND (nothing was locked; bare name is in use)`。
   - **C3b 由 `Note` 升级为 `Pass`/`Fail` 硬判**：注册表未找到指向 `weaselx64` 的 TIP 路径 ⇒ Fail；
     指向版本化 shim 且文件存在 ⇒ Pass；指向裸名且文件存在 ⇒ Pass；**文件缺失或文件名异常 ⇒ Fail**。
     这一条直接消除「掩盖真缺件」的根因——此前无论注册坏成什么样都只打印不置位 `$fail`。

**验证证据**（三路，改写后实跑）：

- `acceptance-check.ps1` 实跑 ⇒ **EXIT=0**；C1 size/sha256 双 PASS；C2 PASS；
  **C3 列出 26 个版本化 shim**（旧判据在这里必然 `NONE FOUND`，这就是 bug 的直接反证）；
  **C3b 命中 `D:\Program Files\fluxing\weasel\weaselx64.1.2.1.2.2.dll` ⇒ PASS**；C4 命中 1 条自启项。
- 正则正反例断言 **8/8 全绿**（`REGEX_ASSERT bad=0 of 8`）：
  命中 `weaselx64.1.2.1.2.2.dll` / `weaselx64.1.1.6.0.dll` / `weasel.1.2.1.1.0.dll` / `weaselx64.1.2.1.dll`；
  不命中 `weaselx64.dll`（裸名）/ `weaselx64.1.2.1.2.2.dll.tmp` / `weaselx64.1.2.1.2.3.4.5.dll`（6 段越界）/ `weaselx64.abc.dll`。
- `scripts/install/verify-installed.ps1:150-166` 复查：它用宽容前缀 `^{stem}\..+\.dll$`，
  **本来就没有这个 bug** ⇒ 失灵只发生在 `acceptance-check.ps1`，无需同步改。

**踩到的坑（值得记）**：第一版断言里我把 `weaselx64.1.2.1.dll` 的预期写成「不该命中」，
跑出 `bad=1` 差点判成正则坏了。实际它恰是 **3 段**、落在 `{3,5}` 区间内、**本该命中**；
是我把「旧正则是 3 段式（固定 3 段、因此认不出 4–5 段）」误记成「3 段名非法」。
⇒ 断言失败先分辨「生产代码错」还是「预期写错」，别直接去改正则。

**第二个坑（行尾）**：本文档 HEAD 版本是**混合行尾**（65 行 CRLF + 其余 LF，无 `.gitattributes`、`core.autocrlf=false`
⇒ 行尾即内容）。本轮 edit 后整文件统一为 **LF**，于是 `git diff` 显示 `104/68`，而
`git diff -w` 与 `git diff --ignore-cr-at-eol` 都是 **`39/3`** —— 差额 65/65 全是行尾归一化，**不是内容重写**。
判断 diff 是否有行尾噪音的判据就是这条：`--ignore-cr-at-eol` 后行数骤降 ⇒ 行尾变了。
本次选择保留统一 LF（混合行尾→单一 LF 是改善，文档无功能影响，完美恢复需字节级对齐、成本高于收益）。
`scripts/install/acceptance-check.ps1` 不受影响：**CRLF=137 / bareLF=0**，两种 diff 模式同为 `25/8`。

### 7.8 真机验收结论（用户手工复验，2026-11-09）

| 项 | 结论 | 判读 |
|---|---|---|
| 搜狗双拼输出 | ✅ **通过** | 切到双拼方案后能正确输出双拼 ⇒ **ADR 0048**（显式选方案 = 全局广播 + 新会话继承）真机闭环，批次 T 主目标达成 |
| DPI · logo | ✅ **通过** | 改分辨率后 logo 完整、无裁切 / 无黑框 ⇒ **ADR 0049**「logo 位图随尺寸重建 + 预合成不透明 + BitBlt」生效 |
| DPI · 键帽符号 | ❌ **未通过**（新） | 键帽符号**没有同比缩放**，线条**仍偏粗** ⇒ DPI 适配只闭环了 logo 一路，**键帽内容那一路没走缩放** |

**用户原话**：「改分辨率后，logo 完整，但键帽符号还是没有同比缩放，线条也还是偏粗。」

⇒ 这条是真机现象、不是推断；且它同时否证了「ADR 0049 改完 DPI 就全好了」的写法 —— §7.4 里那张
「图标几何统一缩放」的自评**范围过宽**，实际只覆盖 logo 位图。下阶段 P0 见 §8.1。

## 8. 下阶段计划（下一会话执行）

> 用户已明确：**§8.2 放到下一个会话执行**，本会话只登记不实现。§8.1 是 §7.8 暴露的未闭环项。

### 8.1 P0 · 快捷面板 DPI 收尾：键帽符号同比缩放 + 线宽同比

**症状**（真机，用户确认）：DPI 变高后**面板与 logo 都对**，但键帽上的**符号尺寸不变、描边线宽不变**。

**根因：未坐实。** 但已实测的代码事实足以把搜索面收窄到「键帽内容绘制路径」，不必从零探索：

- **面板几何确实走了缩放**（这是好的，说明缩放工具链可用）：
  `WeaselServer/QuickPanelDialog.cpp:1580-1582`、`:2059-2060`、`:2767`、`:2862-2864`
  都调 `ScaleLogicalByDpi(kPanelW/kPanelH/12, dpi)`。
- **本地已有两个缩放包装**：`:149-151` `ScaleLogicalByDpi(logical, dpi)` → `fluxing::panel_dpi::ScaleLogicalByDpi`；
  `:175-182` 按 `factor` 缩放的包装（内部 `fluxing::panel_dpi::ScaleByFactor(raw, (double)scale)`）。
  ⇒ 「不知道该用哪个函数」不是障碍，障碍是**键帽符号那一路没调用它们**。
- **图标缩放工具本身存在且声称已解决**：`include/FluxingPanelFwIcon.h:313` 注释写着
  「ScaleByFactor, 下限 1.0) ⇒ 任意 DPI 下『符号 : 键帽』的比例恒定」；
  同文件 `:76` 说明「逗号字形的**种子字号**（px，实际传给 `LOGFONTW.lfHeight` 时取负）」。
  ⇒ 符号是**按 px 常量定种子字号/几何**的，只要这条链路没拿到 `dpi`，就会出现「符号不放大」。
- `WeaselServer/QuickPanelDialog.h:546` 有关于 `ScaleByFactor` **下限 1.0** 的说明。

**建议的三个先查点（按代价从低到高，任一条命中即收工）**：

1. **DC 映射模式**：`QuickPanelDialog` 的绘制 DC 有没有 `SetMapMode` / `SetWindowExtEx` / `SetViewportExtEx`。
   若有映射缩放，则手写的几何缩放会**叠加成双倍**，而字体/笔宽会被映射**单独缩放** ——
   这种「一部分走映射、一部分走手算」的错配，恰好能同时解释用户看到的两种偏差（符号没变、线更粗）。
   **若映射是 1:1（MM_TEXT）**，则问题就是纯粹「符号路径没乘 factor」。
2. **符号绘制的 `dpi` 入参**：找到画键帽符号的函数，确认它的 `dpi` 是**真实面板 DPI** 还是
   构造期默认 `96`（面板类常把 `m_dpi` 初始化为 96，若绘制早于 `WM_DPICHANGED` 刷新就会用旧值）。
   判据：在绘制入口打一条临时日志/断言，打印该函数看到的 `dpi` 与 `ScaleByFactor` 结果。
3. **笔宽与 `lfHeight`**：符号若用 `CreatePen` 画线，宽度是不是硬编码 `1`；若用字体，`lfHeight` 是不是负的 px 常量。

**判据（改完怎么算修好）**：同一符号在 96 / 144 / 192 dpi 下，其**像素包围盒边长与笔画宽度的比值**
应分别接近 `1.0 / 1.5 / 2.0`（±1 px 容差）。**把「符号几何 + 笔宽」抽成生产/单测共用的纯函数**，
喂三档 dpi 断言输出 —— 这是唯一不依赖肉眼的分支；真机仍需**真实改一次分辨率**复验。

**注意**：自动化验证不了这一条（`FluxingQuickPanel_v3` 面板只在 IME 激活瞬间存在，跨进程 `FindWindow` 返回 0，
见 §5 与 L210 同批坑）⇒ 最终验收必须由用户手工改分辨率看一眼。

### 8.2 P1 · 去除两个键帽（用户明确要求 · 下一会话执行）

**需求**：去掉快捷面板上的 **「快捷键设置」** 与 **「用户登录」** 两个键帽，让设置栏更简洁。
（「用户登录」= 逻辑槽 3「关于·账号说明」，面板上文案见 `TooltipText`。）

**预期结果**：6 槽 → **4 槽**，视觉顺序 = 切换输入法 / 中英文 / 全半角 / 用户短语。
这实际是把 v4 stage2 加的两枚键**退回去**，所以 `kVisualToLogical` 应回到 `{0, 4, 5, 1}`。

**代码定位（行号为本会话实测，可直接照改）**：

| 位置 | 内容 | 处理 |
|---|---|---|
| `WeaselServer/QuickPanelDialog.h:417` | `static constexpr int kButtonCount = 6;` | → `4` |
| `WeaselServer/QuickPanelDialog.h:430` | `kVisualToLogical[kButtonCount] = {0, 4, 5, 1, 2, 3}` | → `{0, 4, 5, 1}` |
| `WeaselServer/QuickPanelDialog.h:434/:439-440` | `VisualToLogical` / `LogicalToVisual` 双向换算 | 复核：被删的两槽（2=快捷键设置、3=关于）`LogicalToVisual` 现在返回 `-1`，**逐个确认调用方能吃 -1**；吃不下就加防御 |
| `WeaselServer/QuickPanelDialog.h:64-66`、`:107`、`:112-115` | 槽位与派发注释 | 同步改注释，否则注释与新布局矛盾 |
| `WeaselServer/QuickPanelDialog.cpp:1454` | `DispatchSlotClick(int index)` 的 switch | 逻辑 2 / 3 分支**建议保留**（托盘「快捷键设置」仍走另一条路，见下），只确保面板不再派发到它们 |
| `WeaselServer/QuickPanelDialog.cpp:1431` | `TooltipText(int slot)` | 删 / 留两槽文案取决于是否保留分支；保留分支则文案留着无害 |
| `WeaselServer/QuickPanelDialog.cpp:1296` | `TooltipText(slot)` 的唯一调用点 | 复核 |
| `test/TestQuickPanelDialog/TestQuickPanelDialog.cpp` | 258 assertions，含几何 / 命中 / 文案断言 | **必然连带改**；布局类断言很可能硬编 6 槽，先跑一遍看红在哪 |

**边界（重要）**：托盘右键菜单的「快捷键设置」是**另一条独立入口**，与面板键帽共用资源（ADR 0038）：
`WeaselServer/WeaselServer.rc:165` `MENUITEM "快捷键设置 (&B)", ID_WEASELTRAY_SHORTCUT_SETTINGS`、
`WeaselServer/resource.h:69` 定义、`WeaselServerApp.cpp:483-485` 派发。
用户只要求去掉**面板键帽** ⇒ **托盘入口默认保留**；若也要摘，需用户另行确认（属功能删减，不在本轮授权内）。

**验收**：面板只剩 4 个键帽、几何居中 / 等距；4 个键帽点击都命中正确动作；
`TestQuickPanelDialog.exe` 全绿；托盘菜单「快捷键设置」仍在且能打开设置窗。

### 8.3 继续挂起（非本节任务）

§7.6 未决项 2（`double_pinyin_mspy.schema.yaml` 标「微软双拼」却用搜狗键位）、
3（`m_session_schema_ids` 未回写）、4（按窗口独立选方案，须先推翻 ADR 0048）、5（ADR 0047 空缺）。
